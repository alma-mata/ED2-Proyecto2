/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 ******************************************************************************
 *
 * ESTADOS:
 *   START
 *   PLAYING -
 *   CORRECCIÓN DE VIDAS
 *   CORRECCIÓN DE ANIMACIÓN
 *   CORRECCIÓN MAPA
 *   MOVIMIENTO LISTO
 *   GAME_OVER
 *   RESET
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "ili9341.h"
#include "Bitmaps.h"
#include "fatfs_sd.h"
#include "fisicas.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUFFER_PIXELS 320

FATFS fs;
FATFS *pfs;
FIL fil;
FRESULT fres;
UINT br;
DWORD fre_clust;
uint32_t totalSpace, freeSpace;

#define VIDA_W   7
#define VIDA_H   8
#define VIDA_GAP 2
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t rxData;
char msg[50];

#define ESP32_J1_ADDR (0x10 << 1)
#define ESP32_J2_ADDR (0x11 << 1)

int16_t prevX[NUM_JUGADORES];
int16_t prevY[NUM_JUGADORES];
int16_t prevEnemyX[MAX_ENEMIGOS];
int16_t prevEnemyY[MAX_ENEMIGOS];
uint8_t prevVidas[NUM_JUGADORES];

// Flag para saber si ya se pinto la pantalla de cada estado
uint8_t estadoPintado = 0;
volatile uint8_t anim_flag = 0; 	// Banderas de animación y movimiento
uint8_t frame_start = 0; 		// Frame de la animacion start
uint8_t mario_ready = 0; 		// Bandera para mario listo
uint8_t luigi_ready = 0; 		// Bandera para luigi listo
volatile uint8_t numFrame = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
void dibujarMapa(void);
void borrarSprite(int16_t x, int16_t y, uint8_t w, uint8_t h);
void repararTiles(int16_t x, int16_t y, uint8_t w, uint8_t h);
void dibujarHUD(void);
void actualizarHUD(void);
void gameLoop(void);
void iniciarJuego(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ---------- SD ---------- */
void transmit_uart(char *string) {
    uint8_t len = strlen(string);
    HAL_UART_Transmit(&huart2, (uint8_t*) string, len, 200);
}

void mount_SD() {
    fres = f_mount(&fs, "/", 1);
    if (fres == FR_OK) transmit_uart("SD montada OK.\r\n");
    else { char buf[60]; sprintf(buf, "Error SD: %d\r\n", fres); transmit_uart(buf); }
}
void open_ReadFile(char *filename) { fres = f_open(&fil, filename, FA_READ); }
void close_File(char *filename)   { fres = f_close(&fil); }
void unmount_SD()                 {
	f_mount(NULL, "", 1);
	if (fres == FR_OK)	 transmit_uart("Micro SD card is unmounted!\r\n");
	else if (fres != FR_OK) 	transmit_uart("Micro SD card was not unmounted!\r\n");
}

/* ---------- FONDOS Y ANIMACIONES ---------- */
void drawImageSD_Chunked (char *filename, uint16_t x, uint16_t y,
		uint16_t width, uint16_t height, uint8_t total_frames) {
	HAL_TIM_Base_Stop_IT(&htim6);
	uint16_t chunk = 10;
	uint16_t buffer[BUFFER_PIXELS*chunk];

	mount_SD();
	open_ReadFile(filename);
	for (uint8_t frame = 0; frame < total_frames; frame++) {
		for (uint16_t row = 0; row < height; row += chunk) {
			// Leer una fila completa
			uint16_t rows = (row + chunk > height) ? (height - row) : chunk;
			f_read(&fil, buffer, width * rows * sizeof(uint16_t), &br);
			if (br != width * rows * sizeof(uint16_t)) break;
			LCD_Bitmap(x, y + row, width, rows, buffer);
		}
	}
	close_File(filename);
	unmount_SD();
	HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
    	anim_flag = 1;
    	numFrame++;
    }
}

void makeScore(void) {
	// Proceso para determinar el score de cada uno
	drawImageSD_Chunked("score.bin", 0, 0, 240, 320, 1);
}
/* ---------- HUD ---------- */

void dibujarHUD(void) {
    for (uint8_t i = 0; i < jugadores[J_MARIO].vidas; i++) {
        uint16_t hx = 2 + i * (VIDA_W + VIDA_GAP);
        LCD_BitmapTransparent(hx, 2, VIDA_W, VIDA_H, (const uint16_t*)v_m, 0x0000);
    }
    for (uint8_t i = jugadores[J_MARIO].vidas; i < NUM_VIDAS; i++) {
        uint16_t hx = 2 + i * (VIDA_W + VIDA_GAP);
        FillRect(hx, 2, VIDA_W, VIDA_H, 0x0000);
    }
    for (uint8_t i = 0; i < jugadores[J_LUIGI].vidas; i++) {
        uint16_t hx = SCREEN_W - 2 - (NUM_VIDAS - i) * (VIDA_W + VIDA_GAP);
        LCD_BitmapTransparent(hx, 2, VIDA_W, VIDA_H, (const uint16_t*)v_l, 0x0000);
    }
    for (uint8_t i = jugadores[J_LUIGI].vidas; i < NUM_VIDAS; i++) {
        uint16_t hx = SCREEN_W - 2 - (NUM_VIDAS - i) * (VIDA_W + VIDA_GAP);
        FillRect(hx, 2, VIDA_W, VIDA_H, 0x0000);
    }
    prevVidas[J_MARIO] = jugadores[J_MARIO].vidas;
    prevVidas[J_LUIGI] = jugadores[J_LUIGI].vidas;
}

void actualizarHUD(void) {
    if (prevVidas[J_MARIO] != jugadores[J_MARIO].vidas ||
        prevVidas[J_LUIGI] != jugadores[J_LUIGI].vidas)
        dibujarHUD();
}

/* ---------- MAPA ---------- */

void dibujarMapa(void) {
    LCD_Clear(0x0000);
    for (uint8_t row = 0; row < MAP_ROWS; row++) {
        for (uint8_t col = 0; col < MAP_COLS; col++) {
            uint16_t px = col * TILE_W;
            uint16_t py = row * TILE_H;
            int16_t offset = calcularOffsetInclinacion(row, col);
            uint8_t blk = tilemap[row][col];
            if (blk == 1)
                LCD_BitmapTransparent(px, py+offset, 16, 8, (const uint16_t*)tile2, 0x0000);
            else if (blk == 2)
                LCD_BitmapTransparent(px, py+offset, 8, 16, (const uint16_t*)escalera, 0x0000);
            else if (blk == 3) {
                LCD_BitmapTransparent(px, py+offset, 16, 8, (const uint16_t*)tile2, 0x0000);
                LCD_BitmapTransparent(px, py+offset, 8, 16, (const uint16_t*)escalera, 0x0000);
            }
        }
    }
}

/* ---------- GRAFICOS ---------- */

void borrarSprite(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    if (x < 0 || y < 0) return;
    uint8_t dw = (x + w > SCREEN_W) ? SCREEN_W - x : w;
    uint8_t dh = (y + h > SCREEN_H) ? SCREEN_H - y : h;
    FillRect(x, y, dw, dh, 0x0000);
}

void repararTiles(int16_t x, int16_t y, uint8_t w, uint8_t h) {
    int cMin = (x / TILE_W) - 1;
    int cMax = (x + w - 1) / TILE_W + 1;
    int rMin = (y / TILE_H) - 1;
    int rMax = (y + h - 1) / TILE_H + 1;
    for (int r = rMin; r <= rMax; r++) {
        for (int c = cMin; c <= cMax; c++) {
            if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) continue;
            uint16_t tpx = c * TILE_W;
            uint16_t tpy = r * TILE_H;
            int16_t offset = calcularOffsetInclinacion(r, c);
            uint8_t blk = tilemap[r][c];
            if (blk == 1)
                LCD_Bitmap(tpx, tpy+offset, 16, 8, (const uint16_t*)tile2);
            else if (blk == 2)
                LCD_Bitmap(tpx, tpy+offset, 8, 16, (const uint16_t*)escalera);
            else if (blk == 3) {
                LCD_Bitmap(tpx, tpy+offset, 16, 8, (const uint16_t*)tile2);
                LCD_Bitmap(tpx, tpy+offset, 8, 16, (const uint16_t*)escalera);
            }
        }
    }
}

static uint8_t spritesOverlap(int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2,
                               uint8_t w, uint8_t h)
{
    if (x1 < 0 || x2 < 0) return 0;
    if (x1+w <= x2 || x2+w <= x1) return 0;
    if (y1+h <= y2 || y2+h <= y1) return 0;
    return 1;
}

static void dibujarJugador(uint8_t p) {
    Jugador_t *j = &jugadores[p];
    int16_t jx = Jugador_PixelX(p);
    int16_t jy = Jugador_PixelY(p);
    if (!j->vivo) {
        uint16_t color = (p == J_MARIO) ? 0xF800 : 0x07E0;
        FillRect(jx, jy, SPRITE_W, SPRITE_H, color);
        return;
    }
    if (j->invencible > 0 && (j->invencible % 8) < 4) return;
    const uint16_t *sprite = (p == J_MARIO)
        ? (const uint16_t*)mario : (const uint16_t*)luigi;
    LCD_Sprite_Transparent(jx, jy, SPRITE_W, SPRITE_H,
                           sprite, 1, 0,
                           (j->direccion < 0) ? 1 : 0,
                           0, 0x0000);
}

/* ---------- INICIAR JUEGO ---------- */

void iniciarJuego(void) {
    LCD_Clear(0x0000);
    dibujarMapa();
    Fisicas_Init();
    for (int i = 0; i < NUM_JUGADORES; i++) { prevX[i] = -1; prevY[i] = -1; }
    for (int i = 0; i < MAX_ENEMIGOS; i++)  { prevEnemyX[i] = -1; prevEnemyY[i] = -1; }
    prevVidas[J_MARIO] = 0;
    prevVidas[J_LUIGI] = 0;
    dibujarHUD();
    estadoJuego = ESTADO_PLAYING;
}

/* ---------- GAME LOOP (solo se llama en ESTADO_PLAYING) ---------- */

void gameLoop(void) {
    Fisicas_Update(&hi2c1, ESP32_J1_ADDR, ESP32_J2_ADDR);
    actualizarHUD();

    // === JUGADORES ===
    int16_t nx[NUM_JUGADORES], ny[NUM_JUGADORES];
    uint8_t moved[NUM_JUGADORES];
    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        nx[p] = Jugador_PixelX(p);
        ny[p] = Jugador_PixelY(p);
        moved[p] = (prevX[p] != nx[p] || prevY[p] != ny[p]) ? 1 : 0;
        if (jugadores[p].invencible > 0) moved[p] = 1;
    }
    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        if (!moved[p]) continue;
        if (prevX[p] >= 0) {
            borrarSprite(prevX[p], prevY[p], SPRITE_W, SPRITE_H);
            repararTiles(prevX[p], prevY[p], SPRITE_W, SPRITE_H);
            for (uint8_t q = 0; q < NUM_JUGADORES; q++) {
                if (q == p) continue;
                int16_t qx = (moved[q] && prevX[q] >= 0) ? nx[q] : prevX[q];
                int16_t qy = (moved[q] && prevY[q] >= 0) ? ny[q] : prevY[q];
                if (spritesOverlap(prevX[p], prevY[p], qx, qy, SPRITE_W, SPRITE_H))
                    dibujarJugador(q);
            }
        }
        dibujarJugador(p);
        prevX[p] = nx[p];
        prevY[p] = ny[p];
    }

    // === BARRILES ===
    for (uint8_t i = 0; i < MAX_ENEMIGOS; i++) {
        int16_t ex = FP_TO_PX(enemigos[i].x);
        int16_t ey = FP_TO_PX(enemigos[i].y);
        if (!enemigos[i].activo) {
            if (prevEnemyX[i] >= 0) {
                borrarSprite(prevEnemyX[i], prevEnemyY[i], BARRIL_W, BARRIL_H);
                repararTiles(prevEnemyX[i], prevEnemyY[i], BARRIL_W, BARRIL_H);
                prevEnemyX[i] = -1;
                prevEnemyY[i] = -1;
            }
            continue;
        }
        if (prevEnemyX[i] != ex || prevEnemyY[i] != ey) {
            if (prevEnemyX[i] >= 0) {
                borrarSprite(prevEnemyX[i], prevEnemyY[i], BARRIL_W, BARRIL_H);
                repararTiles(prevEnemyX[i], prevEnemyY[i], BARRIL_W, BARRIL_H);
            }
            LCD_Sprite_Transparent(ex, ey, BARRIL_W, BARRIL_H,
                                   (const uint16_t*)BARRIL_1, 1, 0,
                                   0, 0, 0x0000);
            prevEnemyX[i] = ex;
            prevEnemyY[i] = ey;
        }
    }
}

/*
 * Checa si alguno de los ESP32 mando el comando 'P'.
 * Se lee I2C de ambos jugadores.
 */
static uint8_t checarComandoPlay(void) {
    uint8_t cmd = CMD_NONE;
    if (hi2c1.State != HAL_I2C_STATE_READY) {
        __HAL_I2C_DISABLE(&hi2c1); HAL_Delay(1);
        __HAL_I2C_ENABLE(&hi2c1);
        hi2c1.State = HAL_I2C_STATE_READY; hi2c1.Lock = HAL_UNLOCKED;
    }
    // Checar J1
    if (HAL_I2C_Master_Receive(&hi2c1, ESP32_J1_ADDR, &cmd, 1, 50) == HAL_OK) {
        if (cmd == CMD_PLAY) return 1;
    }
    // Checar J2
    cmd = CMD_NONE;
    if (HAL_I2C_Master_Receive(&hi2c1, ESP32_J2_ADDR, &cmd, 1, 50) == HAL_OK) {
        if (cmd == CMD_PLAY || cmd == 'p') return 1;
    }
    return 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  MX_FATFS_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
    transmit_uart("\r\n--- REINICIO DEL SISTEMA ---\r\n");

    HAL_TIM_Base_Start_IT(&htim6);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_Delay(10);

    LCD_Init();
    LCD_Clear(0x0000);

    mount_SD();
    unmount_SD();

    // Debug I2C
    char pinDbg[60];
    uint8_t sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    uint8_t scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    sprintf(pinDbg, "I2C pins: SDA=%d SCL=%d\r\n", sda, scl);
    transmit_uart(pinDbg);

    transmit_uart("Escaneando bus I2C...\r\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
            char buf[40]; sprintf(buf, "  Encontrado: 0x%02X\r\n", addr); transmit_uart(buf);
        }
    }
    transmit_uart("Escaneo terminado.\r\n");

    estadoJuego = ESTADO_START;
    estadoPintado = 0;

    /*----------------- PRUEBA ESCENAS -----------------*/
    // Pantalla de reposo
    drawImageSD_Chunked("load_screen.bin", 0, 0, 240, 320, 1);
    HAL_Delay(1000);
    // Menu de inicio (ya en switch case)
//    drawImageSD_Chunked("start_menu.bin", 0, 0, 240, 320, 1);
//    LCD_Sprite(100, 100, 37, 7, mario_start, 1, 0, 0, 0);
//    HAL_Delay(1000);
//    // Animacion inicial
//    drawImageSD_Chunked("start_animation.bin", 0, 0, 240, 320, 22);
//    HAL_Delay(1000);
//    // Secuencia WIN
//    drawImageSD_Chunked("win.bin", 0, 0, 240, 320, 8);
//    drawImageSD_Chunked("score.bin", 0, 0, 240, 320, 1);
//    HAL_Delay(1000);
//
//    // Secuencia Game Over
//    drawImageSD_Chunked("game_over.bin", 0, 0, 240, 320, 9);
//    drawImageSD_Chunked("score.bin", 0, 0, 240, 320, 1);
//    HAL_Delay(1000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      switch (estadoJuego) {

      case ESTADO_START:
          if (!estadoPintado) {
              // Mostrar pantalla de inicio
              LCD_Clear(0x0000);
              if (fres == FR_OK) {
                  hspi1.Instance->CR1 &= ~SPI_BAUDRATEPRESCALER_256;
                  hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_2;
                  drawImageSD_Chunked("start_menu.bin", 0, 0, 240, 320, 1);
                  LCD_Sprite(146, 188, 39, 7, mario_start, 2, frame_start, 0, 0);
                  LCD_Sprite(146, 233, 39, 7, luigi_start, 2, frame_start, 0, 0);
              }
              transmit_uart("Estado: START - esperando P\r\n");
              estadoPintado = 1;
          }
          // Animacion de los botones
          if (anim_flag) {
        	  // ----- PRUEBA SPRITES ---------
//        	  frame_start = (numFrame/2)%5;
//        	  LCD_Sprite(10, 220, 18, 18, mario, 5, frame_start, 0, 0);
//        	  LCD_Sprite(40, 220, 18, 18, luigi, 5, frame_start, 0, 0);
//        	  LCD_Sprite(50, 150, 18, 18, mario_death, 5, frame_start, 0, 0);
//        	  LCD_Sprite(80, 150, 18, 18, luigi_death, 5, frame_start, 0, 0);
//
//        	  frame_start = (numFrame/2)%7;
//        	  LCD_Sprite(200, 200, 18, 18, mario_stairs, 7, frame_start, 0, 0);
//        	  LCD_Sprite(220, 200, 18, 18, luigi_stairs, 7, frame_start, 0, 0);

        	  // ---------- Animacion MENU START
        	  frame_start = (numFrame/2)%2; 	//XOR
        	  LCD_Sprite(76, 130, 88, 7, insert_coin, 2, frame_start, 0, 0);
        	  frame_start = (numFrame/2)%4;
        	  LCD_Sprite(111, 33, 18, 24, peach, 4, frame_start, 0, 0);
        	  // ------ CONDICIÓN DE PARPADEO ---------
        	  if (mario_ready) LCD_Sprite(146, 188, 39, 7, mario_start, 2, 0, 0, 0);
        	  else LCD_Sprite(146, 188, 39, 7, mario_start, 2, frame_start, 0, 0);

        	  if (luigi_ready) LCD_Sprite(146, 233, 39, 7, luigi_start, 2, 0, 0, 0);
        	  else LCD_Sprite(146, 233, 39, 7, luigi_start, 2, frame_start, 0, 0);
          }
          // Esperar comando 'P' para iniciar
          if (checarComandoPlay()) {
              estadoPintado = 0;
              iniciarJuego();  // cambia estado a PLAYING
              transmit_uart("Estado: PLAYING\r\n");
          }
          break;

      case ESTADO_PLAYING:
          gameLoop();
          // estadoJuego puede cambiar a GAME_OVER dentro de Fisicas_Update
          if (estadoJuego == ESTADO_GAME_OVER) {
              estadoPintado = 0;
          }
          break;

      case ESTADO_GAME_OVER:
          if (!estadoPintado) {
              // Borrar pantalla y mostrar GAME OVER
              LCD_Clear(0x0000);
              LCD_Print("GAME OVER", 60, 140, 2, 0xF800, 0x0000);
              LCD_Print("Press P", 75, 170, 1, 0xFFFF, 0x0000);
              transmit_uart("Estado: GAME_OVER - esperando P\r\n");
              estadoPintado = 1;
          }
          // Esperar 'P' para ir a RESET
          if (checarComandoPlay()) {
              estadoPintado = 0;
              estadoJuego = ESTADO_RESET;
              transmit_uart("Estado: RESET\r\n");
          }
          break;

      case ESTADO_RESET:
          // Limpiar todo y volver a START
          estadoPintado = 0;
          estadoJuego = ESTADO_START;
          break;
      }

      HAL_Delay(16);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8400-1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 2500-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin|LCD_D1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_SS_GPIO_Port, SD_SS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RST_Pin LCD_D1_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RD_Pin LCD_WR_Pin LCD_RS_Pin LCD_D7_Pin
                           LCD_D0_Pin LCD_D2_Pin */
  GPIO_InitStruct.Pin = LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_CS_Pin LCD_D6_Pin LCD_D3_Pin LCD_D5_Pin
                           LCD_D4_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_SS_Pin */
  GPIO_InitStruct.Pin = SD_SS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(SD_SS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

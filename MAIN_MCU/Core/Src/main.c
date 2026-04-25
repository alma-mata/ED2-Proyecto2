/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : PANTALLA - LOGICA - COLISIONES
 ******************************************************************************
 *
 * ESTADOS:
 * START
 * PLAYING -
 * CORRECCIÓN DE VIDAS
 * CORRECCIÓN DE ANIMACIÓN
 * CORRECCIÓN MAPA
 * MOVIMIENTO LISTO
 * GAME_OVER
 * RESET
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

/* Posiciones de DK y Peach */
#define DK_X     2
#define DK_Y     16
#define DK_W     34
#define DK_H     48
#define PEACH_X  111
#define PEACH_Y  8
#define PEACH_W  18
#define PEACH_H  24
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim6;
UART_HandleTypeDef huart4;
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

uint8_t pantalla_actualizada = 0;
volatile uint8_t flag_animacion = 0;
uint8_t frame_inicial = 0;
uint8_t mario_listo = 0;
uint8_t luigi_listo = 0;
volatile uint8_t contador_frames = 0;
uint8_t buffer_uart4;
volatile uint8_t bandera_play = 0;
volatile uint8_t bandera_skip = 0;
volatile uint8_t estado_J1 = 'N';
volatile uint8_t estado_J2 = 'n';


uint8_t frame_princesa = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */
void dibujarMapa(void);
void borrarSprite(int16_t x, int16_t y, uint8_t w, uint8_t h);
void repararTiles(int16_t x, int16_t y, uint8_t w, uint8_t h);
void dibujarHUD(void);
void actualizarHUD(void);
void gameLoop(void);
void iniciarJuego(void);
void dibujarDK(void);
void dibujarPeach(void);
static uint8_t revisar_boton_play(void);
uint8_t drawImageSD_Chunked_Skip(char *filename, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t total_frames);
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
void drawImageSD_Chunked(char *filename, uint16_t x, uint16_t y,
		uint16_t width, uint16_t height, uint8_t total_frames) {
	HAL_TIM_Base_Stop_IT(&htim6);
	uint16_t chunk = 10;
	uint16_t buffer[BUFFER_PIXELS*chunk];

	mount_SD();
	open_ReadFile(filename);
	for (uint8_t frame = 0; frame < total_frames; frame++) {
		for (uint16_t row = 0; row < height; row += chunk) {
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

/* NUEVA FUNCION: Animacion que se interrumpe si la bandera_skip es 1 */
uint8_t drawImageSD_Chunked_Skip(char *filename, uint16_t x, uint16_t y,
		uint16_t width, uint16_t height, uint8_t total_frames) {
	HAL_TIM_Base_Stop_IT(&htim6);
	uint16_t chunk = 10;
	uint16_t buffer[BUFFER_PIXELS*chunk];

	mount_SD();
	open_ReadFile(filename);
	for (uint8_t frame = 0; frame < total_frames; frame++) {

        // REVISAMOS SI EL USUARIO PRESIONO 'T'
        if (bandera_skip == 1) {
            close_File(filename);
	        unmount_SD();
	        HAL_TIM_Base_Start_IT(&htim6);
            return 1; // Retorna 1 indicando que fue interrumpido
        }

		for (uint16_t row = 0; row < height; row += chunk) {
			uint16_t rows = (row + chunk > height) ? (height - row) : chunk;
			f_read(&fil, buffer, width * rows * sizeof(uint16_t), &br);
			if (br != width * rows * sizeof(uint16_t)) break;
			LCD_Bitmap(x, y + row, width, rows, buffer);
		}
	}
	close_File(filename);
	unmount_SD();
	HAL_TIM_Base_Start_IT(&htim6);
    return 0; // Termino normalmente
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) {
    	flag_animacion = 1;
    	contador_frames++;
    }
}

void makeScore(void) {
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

void dibujarDK(void) {
    int16_t dy = 0;
    uint8_t frame_actual_dk = 0;


    if (dkTirando > 0) {
        if (dkTirando > DK_THROW_FRAMES / 2) {
            dy = -4;
            frame_actual_dk = 2;
        } else {
            frame_actual_dk = 1;
        }
    } else {
        frame_actual_dk = 0;
    }


    LCD_Sprite_Transparent(DK_X, DK_Y + dy, DK_W, DK_H, (const uint16_t*)DK_2, 3, frame_actual_dk, 0, 0, 0x0000);
}


/* ========== Peach: SIN ANIMACIÓN (Fija) ========== */
void dibujarPeach(void) {
    LCD_Sprite_Transparent(PEACH_X, PEACH_Y, PEACH_W, PEACH_H,
                           (const uint16_t*)peach, 4, 0,
                           0, 0, 0x0000);
}

/* ========== Jugador: escalera, muerte, normal ========== */
static void dibujarJugador(uint8_t p) {
    Jugador_t *j = &jugadores[p];
    int16_t jx = Jugador_PixelX(p);
    int16_t jy = Jugador_PixelY(p);

    /* Muerto permanentemente: último frame de muerte */
    if (!j->vivo) {
        const uint16_t *deathSprite = (p == J_MARIO)
            ? (const uint16_t*)mario_death : (const uint16_t*)luigi_death;
        LCD_Sprite_Transparent(jx, jy, SPRITE_W, SPRITE_H,
                               deathSprite, 5, 4,
                               (j->direccion < 0) ? 1 : 0, 0, 0x0000);
        return;
    }

    /* Animación de muerte en curso */
    if (j->muriendo > 0) {
        const uint16_t *deathSprite = (p == J_MARIO)
            ? (const uint16_t*)mario_death : (const uint16_t*)luigi_death;
        LCD_Sprite_Transparent(jx, jy, SPRITE_W, SPRITE_H,
                               deathSprite, 5, j->frameAnimMuerte,
                               (j->direccion < 0) ? 1 : 0, 0, 0x0000);
        return;
    }

    /* Parpadeo de invencibilidad */
    if (j->invencible > 0 && (j->invencible % 8) < 4) return;

    /* En escalera: sprite de escalera (7 frames) */
    if (j->enEscalera) {
        const uint16_t *stairSprite = (p == J_MARIO)
            ? (const uint16_t*)mario_stairs : (const uint16_t*)luigi_stairs;
        LCD_Sprite_Transparent(jx, jy, SPRITE_W, SPRITE_H,
                               stairSprite, 7, j->frameAnim,
                               0, 0, 0x0000);
        return;
    }

    /* Normal: caminar/parado (5 frames) */
    const uint16_t *sprite = (p == J_MARIO)
        ? (const uint16_t*)mario : (const uint16_t*)luigi;

    LCD_Sprite_Transparent(jx, jy, SPRITE_W, SPRITE_H,
                           sprite, 5, j->frameAnim,
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

    dibujarDK();
    dibujarPeach();
    dibujarHUD();
    estadoJuego = ESTADO_PLAYING;
}

/* ---------- GAME LOOP ---------- */
void gameLoop(void) {
    Fisicas_Update();
    actualizarHUD();

    // === JUGADORES ===
    int16_t nx[NUM_JUGADORES], ny[NUM_JUGADORES];
    uint8_t moved[NUM_JUGADORES];
    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        nx[p] = Jugador_PixelX(p);
        ny[p] = Jugador_PixelY(p);
        moved[p] = (prevX[p] != nx[p] || prevY[p] != ny[p]) ? 1 : 0;
        if (jugadores[p].invencible > 0) moved[p] = 1;
        if (jugadores[p].muriendo > 0) moved[p] = 1;
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

    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        if (!jugadores[p].vivo && prevX[p] >= 0) {
            dibujarJugador(p);
        }
    }

    {
        static uint8_t fase_dk_anterior = 0;
        uint8_t fase_actual_dk = 0;
        if (dkTirando > DK_THROW_FRAMES / 2) fase_actual_dk = 2;
        else if (dkTirando > 0)              fase_actual_dk = 1;

        if (fase_actual_dk != fase_dk_anterior) {
            borrarSprite(DK_X, DK_Y - 4, DK_W, DK_H + 4);
            repararTiles(DK_X, DK_Y - 4, DK_W, DK_H + 4);
            fase_dk_anterior = fase_actual_dk;
        }
        dibujarDK();
    }

    dibujarPeach();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_UART4_Init();

  /* USER CODE BEGIN 2 */
    transmit_uart("\r\n--- REINICIO DEL SISTEMA ---\r\n");

    HAL_TIM_Base_Start_IT(&htim6);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_Delay(10);

    LCD_Init();
    LCD_Clear(0x0000);

    mount_SD();
    unmount_SD();

    estadoJuego = ESTADO_START;
    pantalla_actualizada = 0;

    /*----------------- PANTALLA DE REPOSO (LOAD SCREEN) -----------------*/
    // Se muestra al encender la consola
    drawImageSD_Chunked("load_screen.bin", 0, 0, 240, 320, 1);
    HAL_Delay(1000);

    HAL_UART_Receive_IT(&huart4, &buffer_uart4, 1);
    HAL_TIM_Base_Start_IT(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      switch (estadoJuego) {

      case ESTADO_START:
          if (!pantalla_actualizada) {
              // --- AVISAMOS A SONIDO QUE INICIA EL START (Por huart2) ---
              uint8_t cmd_inicio = 'S';
              HAL_UART_Transmit(&huart2, &cmd_inicio, 1, 10);

              LCD_Clear(0x0000);
              if (fres == FR_OK) {
                  hspi1.Instance->CR1 &= ~SPI_BAUDRATEPRESCALER_256;
                  hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_2;
                  drawImageSD_Chunked("start_menu.bin", 0, 0, 240, 320, 1);
                  LCD_Sprite(146, 188, 39, 7, mario_start, 2, frame_inicial, 0, 0);
                  LCD_Sprite(146, 233, 39, 7, luigi_start, 2, frame_inicial, 0, 0);
              }
              transmit_uart("Estado: START - esperando P\r\n");
              pantalla_actualizada = 1;
          }

          frame_inicial = (contador_frames/2)%2;
          LCD_Sprite(76, 130, 88, 7, insert_coin, 2, frame_inicial, 0, 0);

          if (mario_listo) LCD_Sprite(146, 188, 39, 7, mario_start, 2, 0, 0, 0);
          else LCD_Sprite(146, 188, 39, 7, mario_start, 2, frame_inicial, 0, 0);

          if (luigi_listo) LCD_Sprite(146, 233, 39, 7, luigi_start, 2, 0, 0, 0);
          else LCD_Sprite(146, 233, 39, 7, luigi_start, 2, frame_inicial, 0, 0);

          if (revisar_boton_play()) {

              pantalla_actualizada = 0;
              estadoJuego = 5;
          }
          break;

      // --- NUEVO ESTADO: CINEMATICA (ESTADO_JUEGO = 5) ---
      case 5:
          if (!pantalla_actualizada) {
              LCD_Clear(0x0000);
              bandera_skip = 0; // Reiniciamos la bandera

              transmit_uart("Reproduciendo Cinematica (start_animation.bin)...\r\n");

              // Reproducimos la animacion, guardamos si el usuario la skipeo
              uint8_t se_salto = drawImageSD_Chunked_Skip("start_animation.bin", 0, 0, 240, 320, 22);

              if (se_salto) {
                  transmit_uart("Cinematica saltada con 'T'\r\n");

                  // Avisamos al sonido y entramos al juego
                  uint8_t cmd_jugar = 'P';
                  HAL_UART_Transmit(&huart2, &cmd_jugar, 1, 10);
                  iniciarJuego();
                  transmit_uart("Estado: PLAYING\r\n");
              } else {
                  transmit_uart("Cinematica terminada. Regresando a Menu.\r\n");
                  estadoJuego = ESTADO_START;
              }

              pantalla_actualizada = 1;
          }
          break;

      case ESTADO_PLAYING:
          gameLoop();
          if (estadoJuego == ESTADO_GAME_OVER || estadoJuego == ESTADO_WIN) {
              pantalla_actualizada = 0;
          }
          break;

      case ESTADO_GAME_OVER:
          if (!pantalla_actualizada) {
              // --- AVISAMOS A SONIDO QUE MURIERON (Por huart2) ---
              uint8_t cmd_muerte = 'M';
              HAL_UART_Transmit(&huart2, &cmd_muerte, 1, 10);

              LCD_Clear(0x0000);
              drawImageSD_Chunked("game_over.bin", 0, 0, 240, 320, 9);
              drawImageSD_Chunked("score.bin", 0, 0, 240, 320, 1);

              LCD_Print("LOSE", 140, 135, 2, 0xF800, 0x0000);
              LCD_Print("LOSE", 140, 195, 2, 0xF800, 0x0000);
              LCD_Print("PRESS P TO RESTART", 20, 280, 1, 0xFFFF, 0x0000);

              transmit_uart("Estado: GAME_OVER - esperando P\r\n");
              pantalla_actualizada = 1;
          }
          if (revisar_boton_play()) {
              // --- AVISAMOS DE UN SALTO PARA RESETEAR (Por huart2) ---
              uint8_t cmd_salto = 'B';
              HAL_UART_Transmit(&huart2, &cmd_salto, 1, 10);

              pantalla_actualizada = 0;
              estadoJuego = ESTADO_RESET;
              transmit_uart("Estado: RESET\r\n");
          }
          break;

      case ESTADO_WIN:
          if (!pantalla_actualizada) {
              // --- AVISAMOS A SONIDO DE LA VICTORIA (Por huart2) ---
              uint8_t cmd_victoria = 'W';
              HAL_UART_Transmit(&huart2, &cmd_victoria, 1, 10);

              LCD_Clear(0x0000);
              drawImageSD_Chunked("win.bin", 0, 0, 240, 320, 8);
              drawImageSD_Chunked("score.bin", 0, 0, 240, 320, 1);

              if (ganadorID == J_MARIO) {
                  LCD_Print("WIN", 140, 135, 2, 0x07E0, 0x0000);
                  LCD_Print("LOSE", 140, 195, 2, 0xF800, 0x0000);
              } else {
                  LCD_Print("LOSE", 140, 135, 2, 0xF800, 0x0000);
                  LCD_Print("WIN", 140, 195, 2, 0x07E0, 0x0000);
              }

              LCD_Print("PRESS P TO RESTART", 20, 280, 1, 0xFFFF, 0x0000);

              transmit_uart("Fin del juego. Ganador registrado.\r\n");
              pantalla_actualizada = 1;
          }

          if (revisar_boton_play()) {
              // --- AVISAMOS DE UN SALTO PARA RESETEAR (Por huart2) ---
              uint8_t cmd_salto_dos = 'B';
              HAL_UART_Transmit(&huart2, &cmd_salto_dos, 1, 10);

              pantalla_actualizada = 0;
              estadoJuego = ESTADO_RESET;
          }
          break;

      case ESTADO_RESET:
          pantalla_actualizada = 0;
          estadoJuego = ESTADO_START;
          // Reiniciamos al estado S de sonido si el usuario quiere
          uint8_t comando_reinicio = 'S';
          HAL_UART_Transmit(&huart2, &comando_reinicio, 1, 10);
          break;
      }

      HAL_Delay(16);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_SPI1_Init(void)
{
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
  if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8400-1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 2500-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) Error_Handler();
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_UART4_Init(void)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin|LCD_D1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SD_SS_GPIO_Port, SD_SS_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SD_SS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(SD_SS_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART4) {
        HAL_UART_Receive_IT(&huart4, &buffer_uart4, 1);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART4) {
        if (buffer_uart4 == CMD_PLAY || buffer_uart4 == 'p') {
            bandera_play = 1;
        }
        else if (buffer_uart4 == 'T' || buffer_uart4 == 't') {
            bandera_skip = 1;
        }
        else if (buffer_uart4 >= 'A' && buffer_uart4 <= 'Z') {
            estado_J1 = buffer_uart4;
        }
        else if (buffer_uart4 >= 'a' && buffer_uart4 <= 'z') {
            estado_J2 = buffer_uart4;
        }
        HAL_UART_Receive_IT(&huart4, &buffer_uart4, 1);
    }
}

static uint8_t revisar_boton_play(void) {
    if (bandera_play == 1) {
        bandera_play = 0;
        transmit_uart("UART4 RX -> Comando PLAY recibido!\r\n");
        return 1;
    }
    return 0;
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
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

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : modulo sonido arcade STM32F446
  *                   rev 3 - ya funciona el doble canal
  *                   NO TOCAR la parte del DMA stop antes del start.
  ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define TCLK       84000000
#define PSC_DAC    1
#define WAVE_SZ    128
#define M_PI_      3.1415926535

// notas en Hz - copiadas de https://pages.mtu.edu/~suits/notefreqs.html
#define _B0  31A
#define _C1  33
#define _CS1 35
#define _D1  37
#define _DS1 39
#define _E1  41
#define _F1  44
#define _FS1 46
#define _G1  49
#define _GS1 52
#define _A1  55
#define _AS1 58
#define _B1  62
#define _C2  65
#define _CS2 69
#define _D2  73
#define _DS2 78
#define _E2  82
#define _F2  87
#define _FS2 93
#define _G2  98
#define _GS2 104
#define _A2  110
#define _AS2 117
#define _B2  123
#define _C3  131
#define _CS3 139
#define _D3  147
#define _DS3 156
#define _E3  165
#define _F3  175
#define _FS3 185
#define _G3  196
#define _GS3 208
#define _A3  220
#define _AS3 233
#define _B3  247
#define _C4  262
#define _CS4 277
#define _D4  294
#define _DS4 311
#define _E4  330
#define _F4  349
#define _FS4 370
#define _G4  392
#define _GS4 415
#define _A4  440
#define _AS4 466
#define _B4  494
#define _C5  523
#define _CS5 554
#define _D5  587
#define _DS5 622
#define _E5  659
#define _F5  698
#define _FS5 740
#define _G5  784
#define _GS5 831
#define _A5  880
#define _AS5 932
#define _B5  988
#define _C6  1047
#define _CS6 1109
#define _D6  1175
#define _DS6 1245
#define _E6  1319
#define _F6  1397
#define _FS6 1480
#define _G6  1568
#define _GS6 1661
#define _A6  1760
#define _AS6 1865
#define _B6  1976

typedef struct { int freq; int dur; } nota_t;

typedef enum { OFF=0,
	ON, GAP } chst_t;

typedef struct {
    nota_t *seq;
    int len, idx;
    nota_t *saved;       // bgm backup
    int saved_len, saved_i;
    uint32_t tnext;
    chst_t st;
    uint8_t lp;
    TIM_HandleTypeDef *tim;
    uint32_t dac_ch;
} audch_t;
/* USER CODE END PTD */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;
DMA_HandleTypeDef hdma_dac2;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint32_t seno_lut[WAVE_SZ];
uint8_t rx_kb = 0;
uint8_t rx1 = 0, rx2 = 0, rx_st = 0;
volatile uint8_t fwd1 = 0, fwd2 = 0;
volatile uint8_t sfx_cmd = 0, sfx_pend = 0;

audch_t mel, bass, fx;
volatile char gstate = '0'; // S P M W

//uint8_t dbg_cnt = 0; // para debug con el logic analyzer, lo dejo por si acaso

/* efectos - ajustados a oido con el speaker de 8ohm */
nota_t sfx_walk[] = { {_E4,40},{0,20},{_G4,40},{0,20},{_E4,40},{0,30} };
nota_t sfx_jump[] = { {_C5,25},{_E5,35} };
nota_t sfx_fall[] = { {_E5,35},{_C5,35},{_A4,50} };
nota_t sfx_hit[] = { {_C6,25} };
nota_t sfx_special[] = { {_C4,20},{_E4,20},{_G4,20},{_C5,20},{_E5,30} };

// game over
nota_t death_tune[] = {
	{_C4,500},{0,100},{_C4,500},{0,100},{_C4,350},{_DS4,150},{_D4,800},{0,100},
	{_D4,500},{0,100},{_C4,300},{_G4,800},{0,100},
	{_GS4,400},{_G4,400},{_F4,400},{_DS4,400},{_D4,400},{_C4,1200}
};

nota_t win_tune[] = {
	{_E5,160},{_E5,80},{_D5,160},{_E5,80},{_FS5,160},{_E5,80},{_DS5,160},{_E5,80},
	{_D5,160},{_D5,80},{_CS5,160},{_D5,80},{_E5,160},{_D5,80},{_CS5,160},{_B4,80},
	{_C5,160},{_C5,80},{_B4,160},{_A4,80},{_GS4,160},{_A4,80},{_B4,240}
};

// melodia ppal - mano derecha

nota_t m_right[] = {
  {_AS5,200},{_A5,150},{_AS5,100},{_A5,150},{_AS5,100},{_E5,100},{_AS5,100},{_C5,100},
  {_AS5,100},{_E5,400},{_AS5,100},{_E4,50},{_AS5,100},{_A5,150},{_AS5,100},{_E4,50},
  {_AS5,100},{_E4,50},{_AS5,100},{_DS4,200},{_AS5,100},{_E4,75},{_AS5,200},{_E4,50},
  {_AS5,100},{_E4,50},{_AS5,100},{_A5,150},{_AS5,100},{_E4,50},{_AS5,100},{_E4,50},
  {_AS5,100},{_E4,150},{_AS5,100},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_GS5,150},
  {_AS5,100},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_E4,150},{_AS5,100},{_E4,50},
  {_AS5,100},{_E4,50},{_AS5,100},{_GS5,150},{_AS5,100},{_E4,50},{_AS5,100},{_C4,100},
  {_E4,50},{_AS5,100},{_E4,150},{_AS5,100},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},
  {_A5,150},{_AS5,100},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_DS4,200},{_AS5,100},
  {_E4,75},{_AS5,200},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_A5,150},{_AS5,100},
  {_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_E4,150},{_AS5,100},{_E4,50},{_AS5,100},
  {_E4,50},{_AS5,100},{_GS5,150},{_AS5,100},{_E4,50},{_AS5,100},{_E4,50},{_AS5,100},
  {_E4,50},{_AS5,100},{_E4,50},{_AS5,100},{_GS5,150},{_AS5,100},{_E4,50},{_AS5,100},
  {_E4,50},{_AS5,100},{_E4,150},{_AS5,100},{_E4,50},{_AS5,100},{_C4,150},{_AS5,100},
  {_C4,150},{_AS5,100},{_C4,150},{_AS5,100},{_C4,150},{_AS5,100},{_C4,150},{_AS5,100},
  {_C4,150},{_AS5,100},{_C4,150},{_AS5,100},{_C4,150},{_AS5,100},{_C4,150},{_AS5,100},
  {_D4,150},{_AS5,100},{_D4,50},{_AS5,100},{_D4,150},{_AS5,100},{_GS4,150},{_AS5,100},
  {_A4,150},{_AS5,100},{_GS4,100},{_AS5,200},{_A4,200},{_AS5,100},{_C5,100},{_AS5,200},
  {_DS5,200},{_A4,150},{_AS5,100},{_E5,100},{_AS5,200},{_E6,300},{_AS5,100},{_E6,150},
  {_AS5,100},{_E4,150},{_AS5,100},{_B5,300},{_AS5,100},{_B5,150},{_AS5,100},{_GS4,150},
  {_AS5,100},{_B5,100},{_AS5,200},{_GS4,150},{_AS5,100},{_D6,300},{_AS5,100},{_D6,150},
  {_AS5,100},{_E5,100},{_AS5,100},{_DS5,200},{_AS5,200},{_E5,100},{_AS5,100},{_F5,200},
  {_AS5,200},{_FS5,100},{_AS5,100},{_G5,200},{_AS5,200},{_GS5,100},{_AS5,100},{_A5,400},
  {0,200}
};

// melodia ppal - mano izquierda
nota_t m_left[] = {
  {_A3,80},{_A1,300},{_A3,60},{_A1,300},{_A3,60},{_E1,300},{_A3,55},{_E1,300},
  {_A3,57},{_F1,300},{_G1,300},{_A3,100},{_F1,150},{_A3,72},{_G1,300},{_A1,300},
  {_A3,60},{_A1,300},{_A3,60},{_E1,300},{_A3,55},{_E1,300},{_A3,57},{_F1,300},
  {_C2,150},{_B1,300},{_A3,100},{_A1,300},{_A3,72},{_E1,150},{_A1,300},{_A3,60},
  {_A1,300},{_A3,60},{_B1,300},{_A3,55},{_B1,300},{_A3,57},{_C2,300},{_A3,100},
  {_B1,300},{_A3,72},{_B1,300},{_A1,300},{_A3,60},{_A1,300},{_A3,60},{_B1,300},
  {_A3,55},{_B1,300},{_A3,57},{_C2,300},{_A3,100},{_B1,300},{_A3,72},{_B1,300},
  {_D2,100},{_A1,300},{_A3,60},{_B1,300},{_A3,55},{_B1,300},{_GS3,100},{_C2,300},
  {_A3,100},{_B1,300},{_A3,72},{_B1,300},{_A1,300},{_A3,60},{_B1,300},{_A3,55},
  {_B1,300},{_A3,57},{_D2,300},{_A3,100},{_B1,300},{_A3,72},{_B1,300},{_GS1,300},
  {_A3,60},{_GS1,300},{_B1,300},{_A3,55},{_B1,300},{_G3,100},{_A3,57},{_D2,300},
  {_A3,100},{_B1,300},{_A3,72},{_B1,300},{_GS1,300},{_A3,60},{_GS1,300},{_B1,300},
  {_A3,55},{_B1,300},{_B3,100},{_C2,300},{_A3,100},{_B1,300},{_B3,100},{_B1,300},
  {_A1,300},{_A3,60},{_B1,300},{_A3,55},{_B1,300},{_GS3,100},{_C2,300},{_A3,100},
  {_B1,300},{_A3,72},{_B1,300},{_A1,300},{_A3,60},{_B1,300},{_A3,55},{_B1,300},
  {_A3,57},{_D2,300},{_A3,100},{_B1,300},{_A3,72},{_B1,300},{_GS1,300},{_A3,60},
  {_GS1,300},{_B1,300},{_A3,55},{_B1,300},{_A3,57},{_D2,300},{_A3,100},{_B1,300},
  {_A3,72},{_B1,300},{_GS1,300},{_E3,100},{_DS3,200},{_A3,60},{_GS1,300},{_E3,100},
  {_F3,200},{_A3,60},{_B1,300},{_FS3,100},{_G3,200},{_A3,55},{_B1,300},{_GS3,100},
  {_A3,57},{_C2,300},{_A3,100},{_B1,300},{_A3,72},{_B1,300},{_FS2,300},{_A3,60},
  {0,200}
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART4_Init(void);
static void MX_UART5_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM7_Init(void);

/* USER CODE BEGIN PFP */
void init_lut(void);
void set_dac(audch_t *c, int f);
void start_seq(audch_t *c, nota_t *s, int n, uint8_t loop, TIM_HandleTypeDef *t, uint32_t dch);
void update(audch_t *c);
void bgm_start(void);
void play_sfx(nota_t *s, int n);
void handle_cmd(uint8_t cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void init_lut(void) {
	int i;
	for(i=0; i<WAVE_SZ; i++)
		seno_lut[i] = (uint32_t)(2048.0 + sin(i*2.0*M_PI_/WAVE_SZ)*600.0);
	// 600 de amplitud porque con mas se distorsiona en el ampli
}

void set_dac(audch_t *c, int f) {
	HAL_DAC_Stop_DMA(&hdac, c->dac_ch);
	if(f <= 0) return;
	// formula: 84MHz / ((1+1) * 128 * freq) - 1
	c->tim->Instance->ARR = (TCLK/((PSC_DAC+1)*WAVE_SZ*f))-1;
	HAL_DAC_Start_DMA(&hdac, c->dac_ch, (uint32_t*)seno_lut, WAVE_SZ, DAC_ALIGN_12B_R);
}

void start_seq(audch_t *c, nota_t *s, int n, uint8_t loop, TIM_HandleTypeDef *t, uint32_t dch) {
	c->seq=s; c->len=n; c->idx=0;
	c->lp=loop; c->tim=t; c->dac_ch=dch;
	c->st = ON;
	set_dac(c, s[0].freq);
	c->tnext = HAL_GetTick() + s[0].dur;
}

// state machine - cada canal corre independiente
// ON: suena la nota hasta que expire el duracion
// GAP: 10ms de silencio entre notas
// OFF: nada
void update(audch_t *c) {
	if(c->st == OFF) return;
	uint32_t t = HAL_GetTick();

	if(c->st==ON && t>=c->tnext) {
		HAL_DAC_Stop_DMA(&hdac, c->dac_ch);
		c->st=GAP;
		c->tnext = t+10;
		return;
	}
	if(c->st==GAP && t>=c->tnext) {
		c->idx++;
		if(c->idx >= c->len) {
			if(c->lp) { c->idx=0; }
			else if(c->saved) {

				c->seq=c->saved; c->len=c->saved_len;
				c->idx=c->saved_i; c->lp=1;
				c->saved=NULL;
			}
			else { c->st=OFF; return; }
		}
		set_dac(c, c->seq[c->idx].freq);
		c->st=ON;
		c->tnext = t + c->seq[c->idx].dur;
	}
}

void bgm_start(void) {
	HAL_TIM_Base_Start(&htim6);
	HAL_TIM_Base_Start(&htim7);
	start_seq(&mel, m_right, sizeof(m_right)/sizeof(nota_t), 1, &htim6, DAC_CHANNEL_1);
	start_seq(&bass, m_left, sizeof(m_left)/sizeof(nota_t), 1, &htim7, DAC_CHANNEL_2);
}

void play_sfx(nota_t *s, int n) {
	if(mel.st!=OFF && mel.lp) {
		// guardar donde va la musica
		mel.saved=mel.seq;
		mel.saved_len=mel.len;
		mel.saved_i=mel.idx;
	}
	start_seq(&fx, s, n, 0, &htim6, DAC_CHANNEL_1);
}

/*
 * protocolo de comandos (1 byte por UART):
 *  S = iniciar musica    M = game over    W = victoria    P = pausa/menu
 *  Mayusculas = player 1   minusculas = player 2
 *  R/L = caminar   U/D = saltar/bajar   A = accion   B = especial   N = idle
 */
void handle_cmd(uint8_t cmd)
{
	static uint8_t last1=0, last2=0; // anti-repeticion

	// ---- comandos de estado ----
	if(cmd=='S'||cmd=='P'||cmd=='M'||cmd=='W') {
		if(cmd==gstate)
			return; // ya estamos ahi
		gstate=cmd;
		switch(cmd) {
		case 'S': bgm_start();
			break;
		case 'M':
			bass.st=OFF;
			HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2); // IMPORTANTE - ver nota arriba
			play_sfx(death_tune, sizeof(death_tune)/sizeof(nota_t));
			break;
		case 'W':
			bass.st=OFF;
			HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2);
			play_sfx(win_tune, sizeof(win_tune)/sizeof(nota_t));
			break;
		// P: nada por ahora, la musica deberia pausarse pero no implementamos eso
		}
		return;
	}

	// sfx solo en pantalla de pausa, durante la musica no porque se enciman
	if(gstate != 'P') return;

	// player 1
	if(cmd>='A' && cmd<='Z') {
		if(cmd=='N'){last1=0; return;}
		if(cmd==last1) return;
		last1=cmd;
		if(cmd=='R'||cmd=='L')
			play_sfx(sfx_walk, sizeof(sfx_walk)/sizeof(nota_t));
		else if(cmd=='U'||cmd=='D'||cmd=='A')
			play_sfx(sfx_jump, sizeof(sfx_jump)/sizeof(nota_t));
		else if(cmd=='B')
			play_sfx(sfx_special, sizeof(sfx_special)/sizeof(nota_t));
	}
	// player 2 - mismo pero minuscula
	if(cmd>='a' && cmd<='z') {
		if(cmd=='n'){last2=0; return;}
		if(cmd==last2) return;
		last2=cmd;
		if(cmd=='r'||cmd=='l')
			play_sfx(sfx_walk, sizeof(sfx_walk)/sizeof(nota_t));
		else if(cmd=='u'||cmd=='d'||cmd=='a')
			play_sfx(sfx_jump, sizeof(sfx_jump)/sizeof(nota_t));
		else if(cmd=='b')
			play_sfx(sfx_special, sizeof(sfx_special)/sizeof(nota_t));
	}
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_I2C1_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  MX_USART1_UART_Init();
  MX_TIM7_Init();

  /* USER CODE BEGIN 2 */
  init_lut();
  HAL_UART_Receive_IT(&huart4, &rx1, 1);
  HAL_UART_Receive_IT(&huart5, &rx2, 1);
  HAL_UART_Receive_IT(&huart1, &rx_st, 1);
  HAL_UART_Receive_IT(&huart2, &rx_kb, 1);

  memset(&mel, 0, sizeof(audch_t));
  memset(&bass, 0, sizeof(audch_t));
  memset(&fx, 0, sizeof(audch_t));

  //bgm_start(); // descomentar para q suene al prender
  /* USER CODE END 2 */

  while(1)
  {
	  // forwarding joysticks -> modulo central
	  if(fwd1){ HAL_UART_Transmit(&huart1,(uint8_t*)&fwd1,1,5); fwd1=0; }
	  if(fwd2){ HAL_UART_Transmit(&huart1,(uint8_t*)&fwd2,1,5); fwd2=0; }

	  if(sfx_pend){ sfx_pend=0; handle_cmd(sfx_cmd); }

	  // fx tiene prioridad sobre mel (comparten timer6)
	  if(fx.st != OFF) update(&fx);
	  else update(&mel);

	  update(&bass);
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance==UART4){
		fwd1=rx1; sfx_cmd=rx1; sfx_pend=1;
		HAL_UART_Receive_IT(&huart4,&rx1,1);
	}
	else if(huart->Instance==UART5){
		fwd2=rx2; sfx_cmd=rx2; sfx_pend=1;
		HAL_UART_Receive_IT(&huart5,&rx2,1);
	}
	else if(huart->Instance==USART1){
		sfx_cmd=rx_st; sfx_pend=1;
		HAL_UART_Receive_IT(&huart1,&rx_st,1);
	}
	else if(huart->Instance==USART2){
		sfx_cmd=rx_kb; sfx_pend=1;
		HAL_UART_Receive_IT(&huart2,&rx_kb,1);
	}
}
/* USER CODE END 4 */

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
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT2 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T7_TRGO;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

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
  hi2c1.Init.OwnAddress1 = 64;
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 100-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

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
  htim6.Init.Prescaler = 1-1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1000-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 1-1;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 1000-1;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif /* USE_FULL_ASSERT */

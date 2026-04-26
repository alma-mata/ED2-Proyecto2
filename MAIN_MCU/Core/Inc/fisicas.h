/**
 ******************************************************************************
 * @file           : fisicas.h
 ******************************************************************************
 */

#ifndef FISICAS_H
#define FISICAS_H

#include "main.h"
#include <stdint.h>

// pantalla 240x320 en el ILI9341
#define SCREEN_W        240
#define SCREEN_H        320

// cada tile es 8x16, da un mapa de 30 columnas x 20 filas
#define TILE_W          8
#define TILE_H          16
#define MAP_COLS        (SCREEN_W / TILE_W)
#define MAP_ROWS        (SCREEN_H / TILE_H)

// tipos de bloque en el tilemap
#define BLOQUE_VACIO           0
#define BLOQUE_SOLIDO          1
#define BLOQUE_ESCALERA        2
#define BLOQUE_ESCALERA_SOLIDO 3
#define BLOQUE_PELIGRO         4

// fixed point 24.8 - para tener precision subpixel sin usar float (AYUDA DE CHAT_GPT)
#define FP_SHIFT        8
#define PX_TO_FP(px)    ((int32_t)(px) << FP_SHIFT)
#define FP_TO_PX(fp)    ((int32_t)(fp) >> FP_SHIFT)

// constantes de movimiento (en fixed point)

#define GRAVEDAD        50
#define VEL_SALTO       (-800)
#define VEL_CAMINAR     384
#define VEL_ESCALERA    256
#define VEL_Y_MAX       1024

#define SPRITE_W        18
#define SPRITE_H        18

#define NUM_JUGADORES   2
#define J_MARIO         0
#define J_LUIGI         1
#define MAX_ENEMIGOS    4
#define NUM_VIDAS       3
#define INVENCIBLE_FRAMES  90   // 1.5 seg a 60fps

#define BARRIL_W        16
#define BARRIL_H        16
#define BARRIL_RADIO    7
#define BARRIL_SPAWN_INTERVAL  60
#define MUERTE_FRAMES   30
#define DK_THROW_FRAMES 20

#define FILA_PISO_BAJO  19      // fila del piso mas abajo
#define VICTORIA_Y_LIMITE 20    // si el jugador sube de este pixel, gana

// estados del juego
typedef enum {
    ESTADO_START,
    ESTADO_PLAYING,
    ESTADO_GAME_OVER,
    ESTADO_WIN,
    ESTADO_RESET,
	SALTAR_CINE
} EstadoJuego_t;

// comandos UART (los mismos q usa el modulo de sonido)
#define CMD_RIGHT   'R'
#define CMD_LEFT    'L'
#define CMD_UP      'U'
#define CMD_DOWN    'D'
#define CMD_A       'A'
#define CMD_B       'B'
#define CMD_PAUSE   'T'
#define CMD_PLAY    'P'
#define CMD_DEATH   'M'
#define CMD_WIN     'W'
#define CMD_NONE    'N'

typedef struct {
    uint8_t izquierda, derecha, salto, arriba, abajo;
} Controles_t;

typedef struct {
    int32_t x, y, velX, velY;
    uint8_t w, h;
    uint8_t enSuelo, enEscalera;
    int8_t  direccion;
    uint8_t saltando, vivo;
    uint8_t frameAnim, contadorAnim;
    uint8_t id;
    uint8_t vidas;
    uint8_t invencible;
    uint8_t muriendo;
    uint8_t frameAnimMuerte;
} Jugador_t;

typedef struct {
    int32_t x, y, velX, velY;
    uint8_t radio, w, h, tipo, activo;
    int8_t  direccion;
} Enemigo_t;

// --- variables globales ---
extern Jugador_t jugadores[NUM_JUGADORES];
extern Enemigo_t enemigos[MAX_ENEMIGOS];
extern Controles_t controles[NUM_JUGADORES];
extern const uint8_t tilemap[MAP_ROWS][MAP_COLS];
extern uint16_t spawnTimer;
extern EstadoJuego_t estadoJuego;
extern uint8_t dk_anim;
extern uint8_t winner;

// el resto de funciones son static en el .c, no se exponen
void Fisicas_Init(void);
void Fisicas_Update(void);
int16_t Jugador_PixelX(uint8_t id);
int16_t Jugador_PixelY(uint8_t id);

// utilidades de mapa
int16_t calc_slope(uint8_t row, uint8_t col);
int16_t get_floor(int16_t px, int16_t py);
uint8_t hay_escalera(int16_t px, int16_t py);
uint8_t bloque_en(int16_t px, int16_t py);

#endif

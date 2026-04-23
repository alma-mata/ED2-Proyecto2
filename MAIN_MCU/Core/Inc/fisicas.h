/**
 ******************************************************************************
 * @file           : fisicas.h
 ******************************************************************************
 */

#ifndef FISICAS_H
#define FISICAS_H

#include "main.h"
#include <stdint.h>

#define SCREEN_W        240
#define SCREEN_H        320

#define TILE_W          8
#define TILE_H          16
#define MAP_COLS        (SCREEN_W / TILE_W)
#define MAP_ROWS        (SCREEN_H / TILE_H)

#define BLOQUE_VACIO           0
#define BLOQUE_SOLIDO          1
#define BLOQUE_ESCALERA        2
#define BLOQUE_ESCALERA_SOLIDO 3
#define BLOQUE_PELIGRO         4

#define FP_SHIFT        8
#define PX_TO_FP(px)    ((int32_t)(px) << FP_SHIFT)
#define FP_TO_PX(fp)    ((int32_t)(fp) >> FP_SHIFT)

#define GRAVEDAD        60
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

#define NUM_VIDAS           3
#define INVENCIBLE_FRAMES   90

#define BARRIL_W        16
#define BARRIL_H        16
#define BARRIL_RADIO    7
#define BARRIL_SPAWN_INTERVAL  60  // ~1 seg entre spawns

// Fila del piso mas bajo (row 19)
#define FILA_PISO_BAJO  19

// === ESTADOS DEL JUEGO ===
typedef enum {
    ESTADO_START,       // pantalla inicial, esperando P
    ESTADO_PLAYING,     // jugando
    ESTADO_GAME_OVER,   // ambos muertos, esperando P
    ESTADO_RESET        // transicion → vuelve a START
} EstadoJuego_t;

// Comandos ESP32
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
} Jugador_t;

typedef struct {
    int32_t x, y, velX, velY;
    uint8_t radio, w, h, tipo, activo;
    int8_t  direccion;
} Enemigo_t;

extern Jugador_t jugadores[NUM_JUGADORES];
extern Enemigo_t enemigos[MAX_ENEMIGOS];
extern Controles_t controles[NUM_JUGADORES];
extern const uint8_t tilemap[MAP_ROWS][MAP_COLS];
extern uint16_t barrilSpawnTimer;
extern EstadoJuego_t estadoJuego;

void Fisicas_Init(void);
uint8_t obtenerBloque(int16_t px, int16_t py);
void aplicarGravedad(Jugador_t *j);
void colisionVertical(Jugador_t *j);
void moverHorizontal(Jugador_t *j, Controles_t *ctrl);
void colisionHorizontal(Jugador_t *j);
void procesarSalto(Jugador_t *j, Controles_t *ctrl);
void manejarEscalera(Jugador_t *j, Controles_t *ctrl);
void iniciarEnemigo(uint8_t idx, int16_t px, int16_t py, uint8_t radio,
                    uint8_t w, uint8_t h, uint8_t tipo);
uint8_t colisionRadial(Jugador_t *j, Enemigo_t *e);
void actualizarEnemigos(void);
void spawnBarril(void);
void leerControles2P(void);
void Fisicas_Update(void);
int16_t Jugador_PixelX(uint8_t id);
int16_t Jugador_PixelY(uint8_t id);
int16_t calcularOffsetInclinacion(uint8_t row, uint8_t col);
int16_t obtenerSueloReal(int16_t px, int16_t py);

#endif

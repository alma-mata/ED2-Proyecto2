/**
 ******************************************************************************
 * @file           : fisicas.c
 ******************************************************************************
 */

#include "fisicas.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

Jugador_t   jugadores[NUM_JUGADORES];
Enemigo_t   enemigos[MAX_ENEMIGOS];
Controles_t controles[NUM_JUGADORES];
uint16_t    barrilSpawnTimer = 0;
EstadoJuego_t estadoJuego = ESTADO_START;

const uint8_t tilemap[20][30] = {
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,1,3,1,1,1,1,1,0,0,0,0,2,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,1,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0 },
    { 0,0,0,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }
};

/* ========================== MAPA ========================== */

int16_t calcularOffsetInclinacion(uint8_t row, uint8_t col) {
    if (row >= 18 || row <= 3) return 0;
    if (row >= 15 && row <= 17) return -(col / 4);
    if (row >= 12 && row <= 14) return  (col / 4);
    if (row >= 9  && row <= 11) return -(col / 4);
    if (row >= 6  && row <= 8)  return  (col / 4);
    return 0;
}

int16_t obtenerSueloReal(int16_t px, int16_t py) {
    int col = px / TILE_W;
    if (col < 0) col = 0;
    if (col >= MAP_COLS) col = MAP_COLS - 1;
    for (int r = 0; r < MAP_ROWS; r++) {
        uint8_t blk = tilemap[r][col];
        if (blk == BLOQUE_SOLIDO || blk == BLOQUE_ESCALERA_SOLIDO) {
            int16_t offset = calcularOffsetInclinacion(r, col);
            int16_t sueloY = (r * TILE_H) + offset;
            if (sueloY >= py - TILE_H) return sueloY;
        }
    }
    return 500;
}

uint8_t esEscalera(int16_t px, int16_t py) {
    int col = px / TILE_W;
    if (col < 0) col = 0;
    if (col >= MAP_COLS) col = MAP_COLS - 1;
    for (int r = 0; r < MAP_ROWS; r++) {
        uint8_t blk = tilemap[r][col];
        if (blk == BLOQUE_ESCALERA || blk == BLOQUE_ESCALERA_SOLIDO) {
            int16_t offset = calcularOffsetInclinacion(r, col);
            int16_t escTop = (r * TILE_H) + offset;
            int16_t escBot = escTop + TILE_H;
            if (py >= escTop && py <= escBot) return 1;
        }
    }
    return 0;
}

uint8_t obtenerBloque(int16_t px, int16_t py) {
    int col = px / TILE_W;
    int row = py / TILE_H;
    if (col < 0 || col >= MAP_COLS || row < 0 || row >= MAP_ROWS) return 0;
    return tilemap[row][col];
}

/* ========================== INIT ========================== */

void Fisicas_Init(void)
{
    memset(jugadores, 0, sizeof(jugadores));
    memset(controles, 0, sizeof(controles));
    memset(enemigos,  0, sizeof(enemigos));
    barrilSpawnTimer = 0;

    jugadores[J_MARIO].x         = PX_TO_FP(20);
    jugadores[J_MARIO].y         = PX_TO_FP(FILA_PISO_BAJO * TILE_H - SPRITE_H);
    jugadores[J_MARIO].w         = SPRITE_W;
    jugadores[J_MARIO].h         = SPRITE_H;
    jugadores[J_MARIO].direccion = 1;
    jugadores[J_MARIO].vivo      = 1;
    jugadores[J_MARIO].vidas     = NUM_VIDAS;
    jugadores[J_MARIO].invencible = 0;
    jugadores[J_MARIO].id        = J_MARIO;

    jugadores[J_LUIGI].x         = PX_TO_FP(50);
    jugadores[J_LUIGI].y         = PX_TO_FP(FILA_PISO_BAJO * TILE_H - SPRITE_H);
    jugadores[J_LUIGI].w         = SPRITE_W;
    jugadores[J_LUIGI].h         = SPRITE_H;
    jugadores[J_LUIGI].direccion = 1;
    jugadores[J_LUIGI].vivo      = 1;
    jugadores[J_LUIGI].vidas     = NUM_VIDAS;
    jugadores[J_LUIGI].invencible = 0;
    jugadores[J_LUIGI].id        = J_LUIGI;
}

/* ========================== FISICA JUGADOR ========================== */

void aplicarGravedad(Jugador_t *j) {
    if (j->enEscalera) return;
    j->velY += GRAVEDAD;
    if (j->velY > VEL_Y_MAX) j->velY = VEL_Y_MAX;
}

void colisionVertical(Jugador_t *j) {
    int16_t px = FP_TO_PX(j->x);
    j->enSuelo = 0;
    if (j->velY >= 0) {
        int16_t pieIzq   = px + 4;
        int16_t pieDer   = px + j->w - 5;
        int16_t curPieY  = FP_TO_PX(j->y) + j->h;
        int16_t nextPieY = FP_TO_PX(j->y + j->velY) + j->h;
        int16_t sIzq  = obtenerSueloReal(pieIzq, curPieY);
        int16_t sDer  = obtenerSueloReal(pieDer, curPieY);
        int16_t suelo = (sIzq < sDer) ? sIzq : sDer;
        if (suelo < 400 && nextPieY >= suelo) {
            j->y = PX_TO_FP(suelo - j->h);
            j->velY = 0; j->enSuelo = 1; j->saltando = 0;
        } else {
            j->y += j->velY;
        }
    } else {
        j->y += j->velY;
    }
    if (FP_TO_PX(j->y) < 0)               { j->y = 0;                          j->velY = 0; }
    if (FP_TO_PX(j->y) + j->h > SCREEN_H) { j->y = PX_TO_FP(SCREEN_H - j->h); j->velY = 0; j->enSuelo = 1; }
}

void moverHorizontal(Jugador_t *j, Controles_t *ctrl) {
    j->velX = 0;
    if (j->enEscalera) return;
    if (ctrl->izquierda)    { j->velX = -VEL_CAMINAR; j->direccion = -1; }
    else if (ctrl->derecha) { j->velX =  VEL_CAMINAR; j->direccion =  1; }
    j->x += j->velX;
}

void colisionHorizontal(Jugador_t *j) {
    int16_t px = FP_TO_PX(j->x);
    if (px < 0)               { j->x = 0;                          j->velX = 0; }
    if (px + j->w > SCREEN_W) { j->x = PX_TO_FP(SCREEN_W - j->w); j->velX = 0; }
}

void procesarSalto(Jugador_t *j, Controles_t *ctrl) {
    if (ctrl->salto && j->enSuelo && !j->enEscalera) {
        j->velY = VEL_SALTO; j->enSuelo = 0; j->saltando = 1;
    }
}

void manejarEscalera(Jugador_t *j, Controles_t *ctrl) {
    int16_t px      = FP_TO_PX(j->x);
    int16_t py      = FP_TO_PX(j->y);
    int16_t centroX = px + j->w / 2;
    int16_t pieY    = py + j->h - 2;
    uint8_t enEsc   = esEscalera(centroX, pieY);
    if (enEsc && (ctrl->arriba || ctrl->abajo)) {
        j->enEscalera = 1; j->velX = 0; j->velY = 0;
    }
    if (j->enEscalera) {
        j->velY = 0; j->velX = 0;
        if (ctrl->arriba) j->y -= VEL_ESCALERA;
        if (ctrl->abajo)  j->y += VEL_ESCALERA;
        if (!esEscalera(centroX, FP_TO_PX(j->y) + j->h - 2))
            j->enEscalera = 0;
        if (ctrl->salto) {
            j->enEscalera = 0; j->velY = VEL_SALTO; j->saltando = 1;
        }
    }
}

/* ========================== BARRILES ========================== */

void iniciarEnemigo(uint8_t idx, int16_t px, int16_t py, uint8_t radio,
                    uint8_t w, uint8_t h, uint8_t tipo)
{
    if (idx >= MAX_ENEMIGOS) return;
    Enemigo_t *e = &enemigos[idx];
    e->x = PX_TO_FP(px);  e->y = PX_TO_FP(py);
    e->radio = radio;  e->w = w;  e->h = h;
    e->tipo = tipo;  e->activo = 1;  e->direccion = -1;
    e->velX = PX_TO_FP(1);  e->velY = 0;
}

/*
 * Spawn un barril desde la izquierda, plataforma row 5.
 * Solo spawna si hay slots libres.
 */
void spawnBarril(void) {
    for (uint8_t i = 0; i < MAX_ENEMIGOS; i++) {
        if (!enemigos[i].activo) {
            int16_t spawnX = 5 * TILE_W;
            int16_t spawnY = 5 * TILE_H - BARRIL_H;
            int16_t offset = calcularOffsetInclinacion(5, 5);
            spawnY += offset;
            iniciarEnemigo(i, spawnX, spawnY, BARRIL_RADIO,
                          BARRIL_W, BARRIL_H, 0);
            break;  // solo 1 por llamada
        }
    }
}

/*
 * Barriles: gravedad, colision con plataformas, rebote en bordes.
 * NUEVO: si el barril llega al piso inferior (row 19), se DESACTIVA
 *        para ser reciclado por el spawner.
 */
void actualizarEnemigos(void) {
    for (uint8_t i = 0; i < MAX_ENEMIGOS; i++) {
        Enemigo_t *e = &enemigos[i];
        if (!e->activo) continue;

        // Gravedad
        e->velY += GRAVEDAD;
        if (e->velY > VEL_Y_MAX) e->velY = VEL_Y_MAX;

        // Movimiento horizontal
        e->x += e->velX * e->direccion;

        // Colision vertical
        int16_t px = FP_TO_PX(e->x);
        int16_t pieIzq   = px + 2;
        int16_t pieDer   = px + e->w - 3;
        int16_t curPieY  = FP_TO_PX(e->y) + e->h;
        int16_t nextPieY = FP_TO_PX(e->y + e->velY) + e->h;

        int16_t sIzq  = obtenerSueloReal(pieIzq, curPieY);
        int16_t sDer  = obtenerSueloReal(pieDer, curPieY);
        int16_t suelo = (sIzq < sDer) ? sIzq : sDer;

        if (suelo < 400 && nextPieY >= suelo) {
            e->y = PX_TO_FP(suelo - e->h);
            e->velY = 0;

            // === RECICLAR: si llego al piso inferior, desactivar ===
            int16_t pieRow = suelo / TILE_H;
            if (pieRow >= FILA_PISO_BAJO) {
                e->activo = 0;
                continue;
            }
        } else {
            e->y += e->velY;
        }

        // Rebote en bordes
        px = FP_TO_PX(e->x);
        if (px <= 0) {
            e->x = PX_TO_FP(1);
            e->direccion = 1;
        }
        if (px + e->w >= SCREEN_W) {
            e->x = PX_TO_FP(SCREEN_W - e->w - 1);
            e->direccion = -1;
        }
    }
}

uint8_t colisionRadial(Jugador_t *j, Enemigo_t *e) {
    if (!e->activo || !j->vivo) return 0;
    int16_t dx = (FP_TO_PX(j->x)+j->w/2) - (FP_TO_PX(e->x)+e->w/2);
    int16_t dy = (FP_TO_PX(j->y)+j->h/2) - (FP_TO_PX(e->y)+e->h/2);
    uint8_t rJ = (j->w < j->h ? j->w : j->h) / 2;
    uint16_t sumaR = rJ + e->radio;
    int32_t dist2 = (int32_t)dx*dx + (int32_t)dy*dy;
    return (dist2 <= (int32_t)sumaR * sumaR) ? 1 : 0;
}

/* ========================== CONTROLES I2C ========================== */

extern UART_HandleTypeDef huart2;

static void I2C_BusReset(I2C_HandleTypeDef *hi2c) {
    __HAL_I2C_DISABLE(hi2c);
    HAL_Delay(1);
    __HAL_I2C_ENABLE(hi2c);
    hi2c->State = HAL_I2C_STATE_READY;
    hi2c->Lock  = HAL_UNLOCKED;
}

void leerControles2P(I2C_HandleTypeDef *hi2c, uint8_t dirJ1, uint8_t dirJ2)
{
    uint8_t cmdJ1 = CMD_NONE, cmdJ2 = CMD_NONE;
    static uint16_t dbgCount = 0;
    memset(controles, 0, sizeof(controles));

    if (hi2c->State != HAL_I2C_STATE_READY) I2C_BusReset(hi2c);

    HAL_StatusTypeDef st1 = HAL_I2C_Master_Receive(hi2c, dirJ1, &cmdJ1, 1, 50);
    if (st1 == HAL_OK) {
        switch (cmdJ1) {
            case 'R': controles[J_MARIO].derecha   = 1; break;
            case 'L': controles[J_MARIO].izquierda = 1; break;
            case 'U': controles[J_MARIO].arriba    = 1; break;
            case 'D': controles[J_MARIO].abajo     = 1; break;
            case 'A': controles[J_MARIO].salto     = 1; break;
            default: break;
        }
    } else { I2C_BusReset(hi2c); }

    HAL_StatusTypeDef st2 = HAL_I2C_Master_Receive(hi2c, dirJ2, &cmdJ2, 1, 50);
    if (st2 == HAL_OK) {
        switch (cmdJ2) {
            case 'r': controles[J_LUIGI].derecha   = 1; break;
            case 'l': controles[J_LUIGI].izquierda = 1; break;
            case 'u': controles[J_LUIGI].arriba    = 1; break;
            case 'd': controles[J_LUIGI].abajo     = 1; break;
            case 'a': controles[J_LUIGI].salto     = 1; break;
            default: break;
        }
    } else { I2C_BusReset(hi2c); }

    dbgCount++;
    if (dbgCount >= 30) {
        dbgCount = 0;
        char dbg[80];
        sprintf(dbg, "I2C: st1=%d cmd1='%c'(0x%02X) st2=%d cmd2='%c'(0x%02X)\r\n",
                st1, cmdJ1, cmdJ1, st2, cmdJ2, cmdJ2);
        HAL_UART_Transmit(&huart2, (uint8_t*)dbg, strlen(dbg), 10);
    }
}

/* ========================== RESPAWN ========================== */

static void respawnJugador(Jugador_t *j) {
    int16_t spawnX = (j->id == J_MARIO) ? 20 : 50;
    j->x = PX_TO_FP(spawnX);
    j->y = PX_TO_FP(FILA_PISO_BAJO * TILE_H - SPRITE_H);
    j->velX = 0; j->velY = 0;
    j->enSuelo = 0; j->enEscalera = 0; j->saltando = 0;
    j->invencible = INVENCIBLE_FRAMES;
}

/* ========================== UPDATE ========================== */

void Fisicas_Update(I2C_HandleTypeDef *hi2c, uint8_t dirJ1, uint8_t dirJ2)
{
    leerControles2P(hi2c, dirJ1, dirJ2);

    // === Contar barriles activos, spawnar si faltan ===
    barrilSpawnTimer++;
    if (barrilSpawnTimer >= BARRIL_SPAWN_INTERVAL) {
        barrilSpawnTimer = 0;
        // Solo spawnar si hay al menos 1 slot libre
        uint8_t activos = 0;
        for (uint8_t i = 0; i < MAX_ENEMIGOS; i++)
            if (enemigos[i].activo) activos++;
        if (activos < MAX_ENEMIGOS) {
            spawnBarril();
        }
    }

    // === Update jugadores ===
    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        Jugador_t *j   = &jugadores[p];
        Controles_t *c = &controles[p];
        if (!j->vivo) continue;
        if (j->invencible > 0) j->invencible--;

        manejarEscalera(j, c);
        procesarSalto(j, c);
        aplicarGravedad(j);
        moverHorizontal(j, c);
        colisionHorizontal(j);
        colisionVertical(j);

        if (j->velX != 0) {
            j->contadorAnim++;
            if (j->contadorAnim >= 6) { j->contadorAnim = 0; j->frameAnim = (j->frameAnim+1)%4; }
        } else { j->frameAnim = 0; j->contadorAnim = 0; }
    }

    actualizarEnemigos();

    //COLISION DE JUGADORES
    for (uint8_t p = 0; p < NUM_JUGADORES; p++) {
        Jugador_t *j = &jugadores[p];
        if (!j->vivo || j->invencible > 0) continue;
        for (uint8_t i = 0; i < MAX_ENEMIGOS; i++) {
            if (!enemigos[i].activo) continue;
            if (colisionRadial(j, &enemigos[i])) {
                j->vidas--;
                if (j->vidas == 0) {
                    j->vivo = 0;
                } else {
                    respawnJugador(j);
                }
                break;
            }
        }
    }

    //GAME OVER
    if (!jugadores[J_MARIO].vivo && !jugadores[J_LUIGI].vivo) {
        estadoJuego = ESTADO_GAME_OVER;
    }
}

int16_t Jugador_PixelX(uint8_t id) { return (id < NUM_JUGADORES) ? FP_TO_PX(jugadores[id].x) : 0; }
int16_t Jugador_PixelY(uint8_t id) { return (id < NUM_JUGADORES) ? FP_TO_PX(jugadores[id].y) : 0; }

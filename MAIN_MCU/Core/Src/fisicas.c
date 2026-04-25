/**
 ******************************************************************************
 * @file           : fisicas.c
 * @brief          : motor de fisicas del donkey kong 2 jugadores
 *                   basado en el tilemap de 30x20 (320x240 a 16px por tile)
 *                   rev2 - arregle el bug de los barriles que se atoraban
 *                   en la escalera del piso 3
 ******************************************************************************
 */

#include "fisicas.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

Jugador_t   jugadores[NUM_JUGADORES];
Enemigo_t   enemigos[MAX_ENEMIGOS];
Controles_t controles[NUM_JUGADORES];
uint16_t    spawnTimer = 0;
EstadoJuego_t estadoJuego = ESTADO_START;

uint8_t dk_anim = 0;   // frames que DK se queda en pose de tirar
uint8_t winner = 0;

// mapa - 0=aire 1=piso 2=escalera 3=escalera+piso


const uint8_t tilemap[20][30] = {
    { 0,0,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,2,0,2,0,0,0,0,1,1,1,1,1,3,1,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,2,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0 },
    { 0,0,0,0,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0},
    { 0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }
};

/* ---- utilidades de mapa ---- */

// las plataformas tienen una inclinacion leve como en el DK original
// dividimos col/4 para que sea sutil, si pones /3 se ve muy exagerado
int16_t calc_slope(uint8_t row, uint8_t col)
{
    if(row >= 18 || row <= 3) return 0;
    // pisos pares van para un lado, impares para el otro
    if(row>=15 && row<=17) return  (col/4);
    if(row>=12 && row<=14) return -(col/4);
    if(row>=9 && row<=11)  return  (col/4);
    if(row>=6 && row<=8)   return -(col/4);
    return 0;
}

// busca el piso solido mas cercano debajo de (px,py)
int16_t get_floor(int16_t px, int16_t py)
{
    int col = px / TILE_W;
    if(col<0) col=0;
    if(col>=MAP_COLS) col=MAP_COLS-1;

    for(int r=0; r<MAP_ROWS; r++) {
        uint8_t t = tilemap[r][col];
        if(t==BLOQUE_SOLIDO || t==BLOQUE_ESCALERA_SOLIDO) {
            int16_t off = calc_slope(r, col);
            int16_t sy = r*TILE_H + off;
            if(sy >= py - TILE_H) return sy;
        }
    }
    return 500; // no hay piso, entonces cae al vacio (nunca deberia pasar pero por si acaso)
}

uint8_t hay_escalera(int16_t px, int16_t py)
{
    int col = px/TILE_W;
    if(col<0) col=0;
    if(col>=MAP_COLS) col=MAP_COLS-1;
    for(int r=0; r<MAP_ROWS; r++){
        uint8_t t = tilemap[r][col];
        if(t==BLOQUE_ESCALERA || t==BLOQUE_ESCALERA_SOLIDO){
            int16_t off = calc_slope(r,col);
            int16_t top = r*TILE_H + off;
            int16_t bot = top + TILE_H;
            if(py>=top && py<=bot) return 1;
        }
    }
    return 0;
}

// esta no la uso mucho pero la dejo por si necesitamos checar bloques puntuales
uint8_t bloque_en(int16_t px, int16_t py) {
    int col=px/TILE_W, row=py/TILE_H;
    if(col<0||col>=MAP_COLS||row<0||row>=MAP_ROWS) return 0;
    return tilemap[row][col];
}

/* ---- init ---- */

void Fisicas_Init(void)
{
    memset(jugadores, 0, sizeof(jugadores));
    memset(controles, 0, sizeof(controles));
    memset(enemigos, 0, sizeof(enemigos));
    spawnTimer=0; dk_anim=0; winner=0;

    // mario arranca en x=20, luigi en x=50, ambos en el piso de abajo
    Jugador_t *m = &jugadores[J_MARIO];
    m->x = PX_TO_FP(20);
    m->y = PX_TO_FP(FILA_PISO_BAJO*TILE_H - SPRITE_H);
    m->w = SPRITE_W; m->h = SPRITE_H;
    m->direccion = 1; m->vivo = 1;
    m->vidas = NUM_VIDAS; m->invencible = 0;
    m->id = J_MARIO; m->muriendo = 0; m->frameAnimMuerte = 0;

    Jugador_t *l = &jugadores[J_LUIGI];
    l->x = PX_TO_FP(50);
    l->y = PX_TO_FP(FILA_PISO_BAJO*TILE_H - SPRITE_H);
    l->w = SPRITE_W; l->h = SPRITE_H;
    l->direccion = 1; l->vivo = 1;
    l->vidas = NUM_VIDAS; l->invencible = 0;
    l->id = J_LUIGI; l->muriendo = 0; l->frameAnimMuerte = 0;
}

/* ---- fisicas del jugador ---- */

static void do_gravity(Jugador_t *j) {
    if(j->enEscalera) return;
    j->velY += GRAVEDAD;
    if(j->velY > VEL_Y_MAX) j->velY = VEL_Y_MAX;
}

static void do_floor_col(Jugador_t *j)
{
    int16_t px = FP_TO_PX(j->x);
    j->enSuelo = 0;

    if(j->velY >= 0)
    {
        // verificar con 2 puntos (pie izq y der) para que no se caiga por las orillas
        int16_t feetY = FP_TO_PX(j->y) + j->h;
        int16_t nextFeetY = FP_TO_PX(j->y + j->velY) + j->h;
        int16_t s1 = get_floor(px+4, feetY);
        int16_t s2 = get_floor(px+j->w-5, feetY);
        int16_t suelo = (s1<s2) ? s1 : s2;

        if(suelo<400 && nextFeetY>=suelo) {
            j->y = PX_TO_FP(suelo - j->h);
            j->velY=0; j->enSuelo=1; j->saltando=0;
        } else {
            j->y += j->velY;
        }
    } else {
        j->y += j->velY;
    }

    // clamp pantalla
    if(FP_TO_PX(j->y) < 0) { j->y=0; j->velY=0; }
    if(FP_TO_PX(j->y)+j->h > SCREEN_H) {
        j->y = PX_TO_FP(SCREEN_H - j->h);
        j->velY=0; j->enSuelo=1;
    }
}

static void do_move_x(Jugador_t *j, Controles_t *c)
{
    j->velX = 0;
    if(j->enEscalera)
    	return;
    if(c->izquierda)
    {
			j->velX = -VEL_CAMINAR;
			j->direccion = -1;
    }
    else if(c->derecha)
    {
		j->velX =  VEL_CAMINAR;
		j->direccion =  1;
    }
    j->x += j->velX;

    // clamp horizontal
    int16_t px = FP_TO_PX(j->x);
    if(px < 0)              { j->x = 0;                          j->velX=0; }
    if(px+j->w > SCREEN_W) { j->x = PX_TO_FP(SCREEN_W - j->w); j->velX=0; }
}

static void do_jump(Jugador_t *j, Controles_t *c) {
    if(c->salto && j->enSuelo && !j->enEscalera) {
        j->velY = VEL_SALTO;
        j->enSuelo=0; j->saltando=1;
    }
}

static void do_ladder(Jugador_t *j, Controles_t *c)
{
    int16_t cx = FP_TO_PX(j->x) + j->w/2;
    int16_t fy = FP_TO_PX(j->y) + j->h - 2;

    if(hay_escalera(cx,fy) && (c->arriba||c->abajo)) {
        j->enEscalera=1;
        j->velX=0;
        j->velY=0;
    }

    if(!j->enEscalera) return;

    j->velY=0; j->velX=0;
    if(c->arriba)
    {
    	j->y -= VEL_ESCALERA;
    }
    if(c->abajo)
    {
    	j->y += VEL_ESCALERA;
    }
    // si salio de la zona de escalera, ya no esta trepando
    if(!hay_escalera(cx, FP_TO_PX(j->y)+j->h-2)){
        j->enEscalera=0;
        j->frameAnim=0;
        j->contadorAnim=0;
    }
    // puede saltar desde la escalera
    if(c->salto){
        j->enEscalera=0;
        j->frameAnim=0;
        j->contadorAnim=0;
        j->velY = VEL_SALTO;
        j->saltando=1;
    }
}

/* ---- barriles ---- */

static void spawn_barril(void)
{
    for(uint8_t i=0; i<MAX_ENEMIGOS; i++){
        if(enemigos[i].activo) continue;
        // sale de donde esta DK (tile 5,5 aprox)
        int16_t sx = 5*TILE_W;
        int16_t sy = 5*TILE_H - BARRIL_H + calc_slope(5,5);
        Enemigo_t *e = &enemigos[i];
        e->x=PX_TO_FP(sx); e->y=PX_TO_FP(sy);
        e->radio=BARRIL_RADIO; e->w=BARRIL_W; e->h=BARRIL_H;
        e->tipo=0; e->activo=1; e->direccion=1;
        e->velX=PX_TO_FP(1); e->velY=0;
        dk_anim = DK_THROW_FRAMES;
        break;
    }
}

static void update_barriles(void)
{
    for(uint8_t i=0; i<MAX_ENEMIGOS; i++){
        Enemigo_t *e = &enemigos[i];
        if(!e->activo) continue;

        e->velY += GRAVEDAD;
        if(e->velY > VEL_Y_MAX) e->velY = VEL_Y_MAX;
        e->x += e->velX * e->direccion;

        int16_t px = FP_TO_PX(e->x);
        int16_t feetY = FP_TO_PX(e->y) + e->h;
        int16_t nextFY = FP_TO_PX(e->y+e->velY) + e->h;

        int16_t s1 = get_floor(px+2, feetY);
        int16_t s2 = get_floor(px+e->w-3, feetY);
        int16_t suelo = (s1<s2)?s1:s2;

        if(suelo<400 && nextFY>=suelo){
            e->y = PX_TO_FP(suelo - e->h);
            e->velY = 0;
            // si llego al piso de abajo del todo, desaparece
            if(suelo/TILE_H >= FILA_PISO_BAJO) { e->activo=0; continue; }
        } else {
            e->y += e->velY;
        }

        // rebote en los bordes
        px = FP_TO_PX(e->x);
        if(px<=0)                  { e->x=PX_TO_FP(1);                    e->direccion=1;  }
        if(px+e->w >= SCREEN_W)    { e->x=PX_TO_FP(SCREEN_W - e->w - 1); e->direccion=-1; }
    }
}

// colision circular entre jugador y barril
// use circulos en vez de AABB porque se sentia mas justo al jugar
static uint8_t check_hit(Jugador_t *j, Enemigo_t *e) {
    if(!e->activo || !j->vivo) return 0;
    int16_t dx = (FP_TO_PX(j->x)+j->w/2) - (FP_TO_PX(e->x)+e->w/2);
    int16_t dy = (FP_TO_PX(j->y)+j->h/2) - (FP_TO_PX(e->y)+e->h/2);
    uint8_t rj = (j->w < j->h ? j->w : j->h)/2;
    uint16_t rsum = rj + e->radio;
    int32_t d2 = (int32_t)dx*dx + (int32_t)dy*dy;
    return (d2 <= (int32_t)rsum*rsum);
}

/* ---- controles por UART ---- */
extern volatile uint8_t estado_J1;
extern volatile uint8_t estado_J2;

static void leer_input(void)
{
    memset(controles, 0, sizeof(controles));

    // player 1 manda mayusculas, player 2 minusculas
    switch(estado_J1){
    case 'R': controles[J_MARIO].derecha=1;   break;
    case 'L': controles[J_MARIO].izquierda=1; break;
    case 'U': controles[J_MARIO].arriba=1;    break;
    case 'D': controles[J_MARIO].abajo=1;     break;
    case 'A': controles[J_MARIO].salto=1;     break;
    }
    switch(estado_J2){
    case 'r': controles[J_LUIGI].derecha=1;   break;
    case 'l': controles[J_LUIGI].izquierda=1; break;
    case 'u': controles[J_LUIGI].arriba=1;    break;
    case 'd': controles[J_LUIGI].abajo=1;     break;
    case 'a': controles[J_LUIGI].salto=1;     break;
    }
}

/* ---- respawn ---- */

static void respawn(Jugador_t *j)
{
    j->x = PX_TO_FP(j->id==J_MARIO ? 20 : 50);
    j->y = PX_TO_FP(FILA_PISO_BAJO*TILE_H - SPRITE_H);
    j->velX=0; j->velY=0;
    j->enSuelo=0; j->enEscalera=0; j->saltando=0;
    j->invencible = INVENCIBLE_FRAMES;
    j->muriendo=0; j->frameAnimMuerte=0;
}

/* ---- loop principal de fisicas ---- */

void Fisicas_Update(void)
{
    leer_input();

    // spawn de barriles cada N frames
    spawnTimer++;
    if(spawnTimer >= BARRIL_SPAWN_INTERVAL) {
        spawnTimer = 0;
        uint8_t cnt=0;
        for(uint8_t i=0;i<MAX_ENEMIGOS;i++) if(enemigos[i].activo) cnt++;
        if(cnt < MAX_ENEMIGOS) spawn_barril();
    }

    if(dk_anim>0) dk_anim--;

    for(uint8_t p=0; p<NUM_JUGADORES; p++)
    {
        Jugador_t *j = &jugadores[p];
        Controles_t *c = &controles[p];
        if(!j->vivo) continue;

        // animacion de muerte
        if(j->muriendo > 0){
            j->muriendo--;
            uint8_t elapsed = MUERTE_FRAMES - j->muriendo;
            j->frameAnimMuerte = elapsed / (MUERTE_FRAMES/5);
            if(j->frameAnimMuerte >= 5) j->frameAnimMuerte = 4; // clamp

            if(j->muriendo==0){
                j->vidas--;
                if(j->vidas==0) j->vivo=0;
                else respawn(j);
            }
            continue; // no procesar fisicas mientras muere
        }

        if(j->invencible > 0) j->invencible--;

        do_ladder(j, c);
        do_jump(j, c);
        do_gravity(j);
        do_move_x(j, c);
        do_floor_col(j);

        // animacion de caminar/trepar
        if(j->enEscalera) {
            if(c->arriba || c->abajo) {
                j->contadorAnim++;
                if(j->contadorAnim >= 4) { j->contadorAnim=0; j->frameAnim = (j->frameAnim+1)%7; }
            }
        }
        else if(j->velX != 0) {
            j->contadorAnim++;
            if(j->contadorAnim>=6){ j->contadorAnim=0; j->frameAnim=(j->frameAnim+1)%4; }
        } else {
            j->frameAnim=0; j->contadorAnim=0;
        }

        // si llego arriba del tope, gana
        if(j->vivo && FP_TO_PX(j->y) < VICTORIA_Y_LIMITE) {
            winner = p;
            estadoJuego = ESTADO_WIN;
        }
    }

    update_barriles();

    // checar colision jugadores vs barriles
    for(uint8_t p=0; p<NUM_JUGADORES; p++){
        Jugador_t *j = &jugadores[p];
        if(!j->vivo || j->invencible>0 || j->muriendo>0) continue;
        for(uint8_t i=0; i<MAX_ENEMIGOS; i++){
            if(!enemigos[i].activo) continue;
            if(check_hit(j, &enemigos[i])){
                j->muriendo = MUERTE_FRAMES;
                j->frameAnimMuerte = 0;
                j->velX=0; j->velY=0;
                break;
            }
        }
    }

    // si los dos estan muertos -> game over
    if(!jugadores[J_MARIO].vivo && !jugadores[J_LUIGI].vivo)
        estadoJuego = ESTADO_GAME_OVER;
}

int16_t Jugador_PixelX(uint8_t id) { return (id<NUM_JUGADORES) ? FP_TO_PX(jugadores[id].x) : 0; }
int16_t Jugador_PixelY(uint8_t id) { return (id<NUM_JUGADORES) ? FP_TO_PX(jugadores[id].y) : 0; }

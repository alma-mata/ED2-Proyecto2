/*
 * fatfs_sd.c
 * Adaptación completa para STM32F446RE
 */

#define TRUE  1
#define FALSE 0
#define bool BYTE

#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "diskio.h"
#include "fatfs_sd.h"

#ifndef SD_SS_PORT
#define SD_SS_PORT SD_SS_GPIO_Port
#endif

#ifndef SD_SS_PIN
#define SD_SS_PIN SD_SS_Pin
#endif

#ifndef HSPI_SDCARD
#define HSPI_SDCARD &hspi1
#endif

/* Variables de temporización y estado */
volatile uint16_t Timer1, Timer2;
static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType;
static uint8_t PowerFlag = 0;

/* Prototipos estáticos */
static void SELECT(void);
static void DESELECT(void);
static void SPI_TxByte(uint8_t data);
static uint8_t SPI_RxByte(void);
static void SPI_RxBytePtr(uint8_t *buff);
static void SPI_TxBuffer(uint8_t *buffer, uint16_t len);
static uint8_t SD_ReadyWait(void);
static void SD_PowerOn(void);
static void SD_PowerOff(void);
static uint8_t SD_CheckPower(void);
static bool SD_RxDataBlock(BYTE *buff, UINT len);
#if _USE_WRITE == 1
static bool SD_TxDataBlock(const uint8_t *buff, BYTE token);
#endif
static BYTE SD_SendCmd(BYTE cmd, uint32_t arg);

/* Debug por UART */
extern UART_HandleTypeDef huart2;
static void sd_debug(char *s) {
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), 200);
}

/***************************************
 * Funciones de Control SPI
 **************************************/

static void SELECT(void)
{
    HAL_GPIO_WritePin(SD_SS_PORT, SD_SS_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void DESELECT(void)
{
    HAL_GPIO_WritePin(SD_SS_PORT, SD_SS_PIN, GPIO_PIN_SET);
    SPI_RxByte();
    HAL_Delay(1);
}

static void SPI_TxByte(uint8_t data)
{
    while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
    HAL_SPI_Transmit(HSPI_SDCARD, &data, 1, SPI_TIMEOUT);
}

static void SPI_TxBuffer(uint8_t *buffer, uint16_t len)
{
    while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
    HAL_SPI_Transmit(HSPI_SDCARD, buffer, len, SPI_TIMEOUT);
}

static uint8_t SPI_RxByte(void)
{
    uint8_t dummy, data;
    dummy = 0xFF;
    while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
    HAL_SPI_TransmitReceive(HSPI_SDCARD, &dummy, &data, 1, SPI_TIMEOUT);
    return data;
}

static void SPI_RxBytePtr(uint8_t *buff)
{
    *buff = SPI_RxByte();
}

/***************************************
 * Funciones de Control SD
 **************************************/

static uint8_t SD_ReadyWait(void)
{
    uint8_t res;
    Timer2 = 500;
    do {
        res = SPI_RxByte();
    } while ((res != 0xFF) && Timer2);
    return res;
}

static void SD_PowerOn(void)
{
    uint8_t args[6];
    uint32_t cnt = 0x1FFF;

    DESELECT();
    for(int i = 0; i < 10; i++)
    {
        SPI_TxByte(0xFF);
    }

    SELECT();
    args[0] = CMD0;
    args[1] = 0;
    args[2] = 0;
    args[3] = 0;
    args[4] = 0;
    args[5] = 0x95;

    SPI_TxBuffer(args, sizeof(args));

    while ((SPI_RxByte() != 0x01) && cnt)
    {
        cnt--;
    }

    DESELECT();
    SPI_TxByte(0xFF);
    PowerFlag = 1;
}

static void SD_PowerOff(void)
{
    PowerFlag = 0;
}

static uint8_t SD_CheckPower(void)
{
    return PowerFlag;
}

static bool SD_RxDataBlock(BYTE *buff, UINT len)
{
    uint8_t token;
    Timer1 = 200;

    do {
        token = SPI_RxByte();
    } while((token == 0xFF) && Timer1);

    if(token != 0xFE) return FALSE;

    while(len--) {
        SPI_RxBytePtr(buff++);
    }

    SPI_RxByte();
    SPI_RxByte();

    return TRUE;
}

#if _USE_WRITE == 1
static bool SD_TxDataBlock(const uint8_t *buff, BYTE token)
{
    uint8_t resp = 0xFF;
    uint8_t i = 0;

    if (SD_ReadyWait() != 0xFF) return FALSE;

    SPI_TxByte(token);

    if (token != 0xFD)
    {
        SPI_TxBuffer((uint8_t*)buff, 512);
        SPI_RxByte();
        SPI_RxByte();

        while (i <= 64)
        {
            resp = SPI_RxByte();
            if ((resp & 0x1F) == 0x05) break;
            i++;
        }

        Timer1 = 200;
        while ((SPI_RxByte() == 0) && Timer1);
    }

    if ((resp & 0x1F) == 0x05) return TRUE;
    return FALSE;
}
#endif

static BYTE SD_SendCmd(BYTE cmd, uint32_t arg)
{
    uint8_t crc, res;

    if (SD_ReadyWait() != 0xFF) return 0xFF;

    SPI_TxByte(cmd);
    SPI_TxByte((uint8_t)(arg >> 24));
    SPI_TxByte((uint8_t)(arg >> 16));
    SPI_TxByte((uint8_t)(arg >> 8));
    SPI_TxByte((uint8_t)arg);

    if(cmd == CMD0) crc = 0x95;
    else if(cmd == CMD8) crc = 0x87;
    else crc = 1;

    SPI_TxByte(crc);

    if (cmd == CMD12) SPI_RxByte();

    uint8_t n = 10;
    do {
        res = SPI_RxByte();
    } while ((res & 0x80) && --n);

    return res;
}

/***************************************
 * SD_disk_initialize — con debug
 **************************************/

DSTATUS SD_disk_initialize(BYTE drv)
{
    uint8_t n, type, ocr[4];
    char dbg[80];

    if (drv) return STA_NOINIT;
    if (Stat & STA_NODISK) return Stat;

    type = 0;

    /* 1. Enviar 80+ clocks con CS HIGH para despertar la tarjeta */
    DESELECT();
    for (n = 0; n < 10; n++) SPI_TxByte(0xFF);

    /* 2. CMD0 — con CS toggle y CRC correcto */
    SELECT();
    {
        BYTE res;
        uint32_t retry = 0x1FFF;

        SPI_TxByte(0x40);  // CMD0
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x95);  // CRC correcto para CMD0

        do { res = SPI_RxByte(); } while ((res & 0x80) && --retry);
        sprintf(dbg, "CMD0: 0x%02X\r\n", res); sd_debug(dbg);

        if (res != 0x01) {
            DESELECT();
            sd_debug("CMD0 FAIL\r\n");
            return STA_NOINIT;
        }
    }
    DESELECT();
    SPI_TxByte(0xFF);

    /* 3. CMD8 — verificar si es SDv2 */
    SELECT();
    {
        BYTE res;
        SPI_TxByte(0xFF); // dummy antes del comando

        SPI_TxByte(0x48);  // CMD8
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x01);
        SPI_TxByte(0xAA);
        SPI_TxByte(0x87);  // CRC correcto para CMD8(0x1AA)

        uint8_t retry = 10;
        do { res = SPI_RxByte(); } while ((res & 0x80) && --retry);
        sprintf(dbg, "CMD8: 0x%02X\r\n", res); sd_debug(dbg);

        if (res <= 0x01) {
            for (n = 0; n < 4; n++) ocr[n] = SPI_RxByte();
            sprintf(dbg, "CMD8 trail: %02X %02X %02X %02X\r\n", ocr[0], ocr[1], ocr[2], ocr[3]); sd_debug(dbg);
        }
    }
    DESELECT();
    SPI_TxByte(0xFF);

    /* 4. CMD59 — desactivar CRC check (por si la tarjeta lo tiene activo) */
    SELECT();
    {
        SPI_TxByte(0xFF);
        SPI_TxByte(0x7B);  // CMD59
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);  // arg=0 → CRC OFF
        SPI_TxByte(0x91);  // CRC para CMD59(0)

        BYTE res;
        uint8_t retry = 10;
        do { res = SPI_RxByte(); } while ((res & 0x80) && --retry);
        sprintf(dbg, "CMD59: 0x%02X\r\n", res); sd_debug(dbg);
    }
    DESELECT();
    SPI_TxByte(0xFF);

    /* 5. ACMD41 loop — sacar de idle */
    sd_debug("ACMD41 loop...\r\n");
    Timer1 = 2000;  // 2 segundos de timeout
    int attempts = 0;
    BYTE r41 = 0xFF;

    while (Timer1) {
        /* CMD55 */
        SELECT();
        SPI_TxByte(0xFF);
        SPI_TxByte(0x77);  // CMD55
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x01);
        {
            BYTE res; uint8_t retry = 10;
            do { res = SPI_RxByte(); } while ((res & 0x80) && --retry);
        }
        DESELECT();
        SPI_TxByte(0xFF);

        /* CMD41 con HCS */
        SELECT();
        SPI_TxByte(0xFF);
        SPI_TxByte(0x69);  // CMD41
        SPI_TxByte(0x40);  // HCS bit
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x01);
        {
            uint8_t retry = 10;
            do { r41 = SPI_RxByte(); } while ((r41 & 0x80) && --retry);
        }
        DESELECT();
        SPI_TxByte(0xFF);

        attempts++;
        if (attempts % 100 == 0) {
            sprintf(dbg, "ACMD41 #%d: 0x%02X T=%d\r\n", attempts, r41, Timer1);
            sd_debug(dbg);
        }

        if (r41 == 0x00) break;
    }

    sprintf(dbg, "ACMD41 done: 0x%02X att=%d T=%d\r\n", r41, attempts, Timer1);
    sd_debug(dbg);

    if (r41 != 0x00) {
        sd_debug("SD INIT FAILED at ACMD41\r\n");
        return STA_NOINIT;
    }

    /* 6. CMD58 — leer OCR para saber si es SDHC */
    SELECT();
    {
        SPI_TxByte(0xFF);
        SPI_TxByte(0x7A);  // CMD58
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x00);
        SPI_TxByte(0x01);

        BYTE res;
        uint8_t retry = 10;
        do { res = SPI_RxByte(); } while ((res & 0x80) && --retry);
        sprintf(dbg, "CMD58: 0x%02X\r\n", res); sd_debug(dbg);

        if (res == 0x00) {
            for (n = 0; n < 4; n++) ocr[n] = SPI_RxByte();
            sprintf(dbg, "OCR: %02X %02X %02X %02X\r\n", ocr[0], ocr[1], ocr[2], ocr[3]);
            sd_debug(dbg);
            type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
        }
    }
    DESELECT();
    SPI_TxByte(0xFF);

    sprintf(dbg, "type=%d\r\n", type); sd_debug(dbg);

    CardType = type;

    if (type) {
        Stat &= ~STA_NOINIT;
        sd_debug("SD OK!\r\n");
    } else {
        SD_PowerOff();
        sd_debug("SD INIT FAILED\r\n");
    }

    return Stat;
}

DSTATUS SD_disk_status(BYTE drv)
{
    if (drv) return STA_NOINIT;
    return Stat;
}

DRESULT SD_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (!(CardType & CT_BLOCK)) sector *= 512;

    SELECT();

    if (count == 1)
    {
        if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512)) count = 0;
    }
    else
    {
        if (SD_SendCmd(CMD18, sector) == 0)
        {
            do {
                if (!SD_RxDataBlock(buff, 512)) break;
                buff += 512;
            } while (--count);
            SD_SendCmd(CMD12, 0);
        }
    }

    DESELECT();
    SPI_RxByte();

    return count ? RES_ERROR : RES_OK;
}

#if _USE_WRITE == 1
DRESULT SD_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (Stat & STA_PROTECT) return RES_WRPRT;
    if (!(CardType & CT_BLOCK)) sector *= 512;

    SELECT();

    if (count == 1)
    {
        if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE)) count = 0;
    }
    else
    {
        if (CardType & CT_SD1)
        {
            SD_SendCmd(CMD55, 0);
            SD_SendCmd(CMD23, count);
        }

        if (SD_SendCmd(CMD25, sector) == 0)
        {
            do {
                if(!SD_TxDataBlock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);

            if(!SD_TxDataBlock(0, 0xFD)) count = 1;
        }
    }

    DESELECT();
    SPI_RxByte();

    return count ? RES_ERROR : RES_OK;
}
#endif

DRESULT SD_disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
    DRESULT res;
    uint8_t n, csd[16], *ptr = buff;

    if (drv) return RES_PARERR;
    res = RES_ERROR;

    if (ctrl == CTRL_POWER)
    {
        switch (*ptr)
        {
        case 0: SD_PowerOff(); res = RES_OK; break;
        case 1: SD_PowerOn();  res = RES_OK; break;
        case 2: *(ptr+1) = SD_CheckPower(); res = RES_OK; break;
        default: res = RES_PARERR;
        }
    }
    else
    {
        if (Stat & STA_NOINIT) return RES_NOTRDY;

        SELECT();

        switch (ctrl)
        {
        case GET_SECTOR_COUNT:
            if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
            {
                if ((csd[0] >> 6) == 1)
                {
                    DWORD c_size = (DWORD)(csd[7] & 0x3F) << 16 | (WORD)csd[8] << 8 | csd[9];
                    *(DWORD*)buff = (c_size + 1) << 10;
                }
                else
                {
                    WORD csize;
                    n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                    csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
                    *(DWORD*)buff = (DWORD)csize << (n - 9);
                }
                res = RES_OK;
            }
            break;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            res = RES_OK;
            break;
        case CTRL_SYNC:
            if (SD_ReadyWait() == 0xFF) res = RES_OK;
            break;
        case MMC_GET_CSD:
            if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(ptr, 16)) res = RES_OK;
            break;
        case MMC_GET_CID:
            if (SD_SendCmd(CMD10, 0) == 0 && SD_RxDataBlock(ptr, 16)) res = RES_OK;
            break;
        case MMC_GET_OCR:
            if (SD_SendCmd(CMD58, 0) == 0)
            {
                for (n = 0; n < 4; n++) *ptr++ = SPI_RxByte();
                res = RES_OK;
            }
            break;
        default:
            res = RES_PARERR;
        }

        DESELECT();
        SPI_RxByte();
    }

    return res;
}

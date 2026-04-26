#include "fatfs_sd.h"

static volatile DSTATUS Stat = STA_NOINIT;
static BYTE CardType;

static void SPI_Release(void) {
    BYTE b = 0xFF;
    HAL_SPI_Transmit(&SD_SPI_HANDLE, &b, 1, 100);
}

static void SD_Select(void) {
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

static void SD_Deselect(void) {
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    SPI_Release();
}

static BYTE SPI_Xchg(BYTE data) {
    BYTE res;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &res, 1, 100);
    return res;
}

static BYTE Wait_Ready(void) {
    BYTE res;
    UINT timeout = 5000;
    do {
        res = SPI_Xchg(0xFF);
    } while (res != 0xFF && --timeout);
    return res;
}

static BYTE Send_Cmd(BYTE cmd, DWORD arg) {
    BYTE res, n;
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = Send_Cmd(CMD55, 0);
        if (res > 1) return res;
    }

    SD_Deselect();
    SD_Select();
    if (Wait_Ready() != 0xFF) return 0xFF;

    SPI_Xchg(0x40 | cmd);
    SPI_Xchg((BYTE)(arg >> 24));
    SPI_Xchg((BYTE)(arg >> 16));
    SPI_Xchg((BYTE)(arg >> 8));
    SPI_Xchg((BYTE)arg);

    n = 0x01; // Dummy CRC
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    SPI_Xchg(n);

    n = 10;
    do {
        res = SPI_Xchg(0xFF);
    } while ((res & 0x80) && --n);

    return res;
}

DSTATUS SD_disk_initialize(BYTE pdrv) {
    BYTE n, ty, ocr[4];
    UINT tmr;

    if (pdrv) return STA_NOINIT;

    uint32_t temp_cr1 = SD_SPI_HANDLE.Instance->CR1;
    SD_SPI_HANDLE.Instance->CR1 = (temp_cr1 & ~SPI_BAUDRATEPRESCALER_256) | SPI_BAUDRATEPRESCALER_256;

    HAL_Delay(100);
    SD_Deselect();
    for (n = 10; n; n--) SPI_Release();

    ty = 0;
    if (Send_Cmd(CMD0, 0) == 1) {
        if (Send_Cmd(CMD8, 0x1AA) == 1) {
            for (n = 0; n < 4; n++) ocr[n] = SPI_Xchg(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                for (tmr = 1000; tmr && Send_Cmd(ACMD41, 1UL << 30); tmr--) HAL_Delay(1);

                if (tmr && Send_Cmd(CMD58, 0) == 0) {
                    for (n = 0; n < 4; n++) ocr[n] = SPI_Xchg(0xFF);
                    ty = 12;
                }
            }
        } else {
            BYTE cmd;
            if (Send_Cmd(ACMD41, 0) <= 1) {
                ty = 2; cmd = ACMD41;
            } else {
                ty = 1; cmd = CMD1;
            }
            for (tmr = 1000; tmr && Send_Cmd(cmd, 0); tmr--) HAL_Delay(1);
            if (!tmr) ty = 0;
        }
    }

    CardType = ty;
    SD_Deselect();

    if (ty) {
        Stat &= ~STA_NOINIT;
        SD_SPI_HANDLE.Instance->CR1 = (temp_cr1 & ~SPI_BAUDRATEPRESCALER_256) | SPI_BAUDRATEPRESCALER_8;
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
}

DSTATUS SD_disk_status(BYTE pdrv) {
    if (pdrv) return STA_NOINIT;
    return Stat;
}

DRESULT SD_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (!(CardType & 8)) sector *= 512;

    if (count == 1) {
        if ((Send_Cmd(CMD17, sector) == 0)) {
            UINT timeout = 1000;
            while (SPI_Xchg(0xFF) != 0xFE && --timeout);
            if (timeout) {
                for (int i = 0; i < 512; i++) *buff++ = SPI_Xchg(0xFF);
                SPI_Xchg(0xFF); SPI_Xchg(0xFF); // Skip CRC
                count = 0;
            }
        }
    }
    SD_Deselect();
    return count ? RES_ERROR : RES_OK;
}

DRESULT SD_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (!(CardType & 8)) sector *= 512;

    if (count == 1) {
        if (Send_Cmd(CMD24, sector) == 0) {
            SPI_Xchg(0xFF); SPI_Xchg(0xFE);
            for (int i = 0; i < 512; i++) SPI_Xchg(*buff++);
            SPI_Xchg(0xFF); SPI_Xchg(0xFF);
            if ((SPI_Xchg(0xFF) & 0x1F) == 0x05) count = 0;
        }
    }
    SD_Deselect();
    return count ? RES_ERROR : RES_OK;
}

DRESULT SD_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv) return RES_PARERR;
    if (cmd == GET_SECTOR_SIZE) { *(WORD*)buff = 512; return RES_OK; }
    if (cmd == CTRL_SYNC) return RES_OK;
    return RES_ERROR;
}

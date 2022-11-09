#ifndef __SPI_DFU_H__
#define __SPI_DFU_H__

#include <cyfx3error.h>

#ifdef __cplusplus
extern "C"
{
#endif

CyFx3BootErrorCode_t spi_dfu_init();
void spi_dfu_deinit();
CyFx3BootErrorCode_t spi_read_bytes(uint32_t address, uint32_t length, uint8_t* buffer);
CyFx3BootErrorCode_t spi_sector_erase(uint8_t sector);

CyFx3BootErrorCode_t dfu_read_sn(uint8_t* buffer);
CyFx3BootErrorCode_t dfu_download_sync(uint8_t* buufer, int32_t length);

CyFx3BootErrorCode_t dfu_load_firmware(uint32_t* jumpaddr);

#ifdef __cpluspuls
}
#endif

#endif//__BOOT_SPI_H__

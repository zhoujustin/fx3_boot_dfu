#include <cyfx3device.h>
#include <cyfx3spi.h>
#include <cyfx3utils.h>
#include "spi_dfu.h"
#include "fx3_ram_define.h"

#define M25P40_PAGE_SIZE		(256)
#define M25P40_SN_STORAGE_ADDR		(0x8000)	// In section 0
#define M25P40_BOOT_INDEX_STORAGE_ADDR	(0x70000)	// In section 7
#define M25P40_IMAGE_A_STORAGE_ADDR	(0x10000)	// In section 1
#define M25P40_IMAGE_B_STORAGE_ADDR	(0x40000)	// In section 4

#define SN_STORAGE_MAGIC_CODE		(0x03)

//uint8_t* sn = (uint8_t*) FX3_RAM_SN_ADDR;

CyFx3BootErrorCode_t spi_write_enable(void) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint8_t cmd[1] = { 0x06 }; 	// 

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    result = CyFx3BootSpiTransmitWords(cmd, 1);
    
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);

    return result;
}

CyFx3BootErrorCode_t spi_waitfor_write_enable(void) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    result = spi_write_enable();
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    do {
	uint8_t cmd[2] = { 0x05, 0x00 };
	
	result = CyFx3BootSpiTransmitWords(cmd, 1);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	
	result = CyFx3BootSpiReceiveWords(cmd, 2);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	if (0x02 == (cmd[0] & 0x03)) {	// b0(WIP bit) = 0 and b1(WEL bit) = 1
	    break;			// Set Write Enable Latch bit successful.
	}
    }while(1);
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);
    
    return result;
}

CyFx3BootErrorCode_t spi_waitfor_write_instruction_completion(void) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    
    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    do {
	uint8_t cmd[2] = { 0x05, 0x00 };
	
	result = CyFx3BootSpiTransmitWords(cmd, 1);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	
	result = CyFx3BootSpiReceiveWords(cmd, 2);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	if (0x00 == (cmd[0] & 0x03)) {
	    break;
	}
    }while(1);
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);

    return result;
}

CyFx3BootErrorCode_t spi_sector_erase(uint8_t section) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint32_t address = section << 16;
    uint8_t addr[4] = { 0xD8, (address >> 16) & 0xFF, (address >> 8) & 0xFF, (address) & 0xFF};
    result = spi_waitfor_write_enable();
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    
    result = CyFx3BootSpiTransmitWords(addr, 4);

    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);
    spi_waitfor_write_instruction_completion();
    
    return result;
}

CyFx3BootErrorCode_t spi_write_bytes(uint32_t address, uint16_t length, uint8_t* data) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    
    result = spi_waitfor_write_enable();
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    do {
	uint8_t addr[4] = { 0x02, (address >> 16) & 0xFF, (address >> 8) & 0xFF, (address) & 0xFF};

	result = CyFx3BootSpiTransmitWords(addr, 4);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	result = CyFx3BootSpiTransmitWords(data, length);
    }while(0);
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);
    spi_waitfor_write_instruction_completion();

    return result;
}

CyFx3BootErrorCode_t spi_write_continue(uint32_t address, int32_t length, uint8_t* data) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    int32_t left = length;
    int32_t offset = 0;
    do {
	int32_t translength = left > M25P40_PAGE_SIZE ? M25P40_PAGE_SIZE : left;
	result = spi_write_bytes(address + offset, translength, data + offset);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	left = left - translength;
	offset = offset + translength;
    }while (left > 0);

    return result;
}

CyFx3BootErrorCode_t spi_read_bytes(uint32_t address, uint32_t length, uint8_t* buffer) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    if (address + length > 0x80000) {
	return result;
    }

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    do {
	uint8_t addr[4] = {0x03, (address >> 16) & 0xFF, (address >> 8) & 0xFF, (address) & 0xFF};
	result = CyFx3BootSpiTransmitWords(addr, 4);
	if (CY_FX3_BOOT_SUCCESS != result) { 
	    break;
	}
	//CyFx3BootBusyWait(10);
	result = CyFx3BootSpiReceiveWords(buffer, length);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
    }while(0);
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);

    return result;
}

CyFx3BootErrorCode_t dfu_read_sn(uint8_t* buffer) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    do {
	uint8_t readbuf[M25P40_PAGE_SIZE] = { 0x00 };
	result = spi_read_bytes(M25P40_SN_STORAGE_ADDR, M25P40_PAGE_SIZE, readbuf);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	if (0xFF == readbuf[1]) {
	    result = CY_FX3_BOOT_ERROR_NOT_CONFIGURED;
	    break;
	}else if (SN_STORAGE_MAGIC_CODE != readbuf[1]) {
	    result = CY_FX3_BOOT_ERROR_BAD_DESCRIPTOR_TYPE;
	    break;
	}else {
	    uint32_t len = (uint32_t) readbuf[0];
	    CyFx3BootMemCopy(buffer, readbuf, len);
	    break;
	}
    }while(0);
    
    return result;
}

CyFx3BootErrorCode_t dfu_write_sn(uint8_t* buffer, uint8_t len) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint8_t readbuf[M25P40_PAGE_SIZE] = { 0x00 };
    
    result = dfu_read_sn(readbuf);
    if (CY_FX3_BOOT_ERROR_NOT_CONFIGURED == result) {
	buffer[0] = len;			// 0xFE -> len
	buffer[1] = SN_STORAGE_MAGIC_CODE;	// 0xFF -> 0x03
	result = spi_write_bytes(M25P40_SN_STORAGE_ADDR, len, buffer);
    }else if (CY_FX3_BOOT_SUCCESS == result) {
	result = CY_FX3_BOOT_ERROR_NOT_SUPPORTED;
    }

    return result;
}

uint16_t find_first_non_zero_bytes(uint8_t* bytes, uint16_t len) {
    uint16_t idx = 0;
    
    do {
	if (0x00 == bytes[idx]) {
	    idx = idx + 1;
	}else 
	{
	    break;
	}
    }while(idx < len);
    
    return idx;
}

uint8_t find_first_non_zero_bits(uint8_t byte) {
    int8_t idx = 7;
    do {
	if (0 < (byte >> idx)) {
	    break;
	}
	idx = idx - 1;
    }while(idx > 0);

    return 7 - idx;
}

CyFx3BootErrorCode_t dfu_get_current_firmware_index(uint8_t* indx) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    do {
	uint8_t readbuf[M25P40_PAGE_SIZE] = { 0x00 };
	result = spi_read_bytes(M25P40_BOOT_INDEX_STORAGE_ADDR, M25P40_PAGE_SIZE, readbuf);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	uint16_t byteidx = find_first_non_zero_bytes(readbuf, M25P40_PAGE_SIZE);
	uint8_t bitidx = find_first_non_zero_bits(readbuf[byteidx]);
	*indx = bitidx % 2;
    }while(0);
    
    return result;
}

CyFx3BootErrorCode_t spi_load_firmware_section(uint32_t spiaddr, uint32_t ramaddr, uint32_t len) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint8_t addr[4] = {0x03, (spiaddr >> 16) & 0xFF, (spiaddr >> 8) & 0xFF, (spiaddr) & 0xFF};

    CyFx3BootSpiSetSsnLine(CyFalse);
    CyFx3BootBusyWait(100);
    do {
	result = CyFx3BootSpiTransmitWords(addr, 4);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	CyFx3BootSpiSetBlockXfer(0, len);

	result = CyFx3BootSpiDmaXferData(CyTrue, ramaddr, len, 1000);
	
	CyFx3BootSpiDisableBlockXfer();
    }while(0);
    CyFx3BootSpiSetSsnLine(CyTrue);
    CyFx3BootBusyWait(100);

    return result;
}

CyFx3BootErrorCode_t spi_load_firmware_seg_head(uint32_t baseaddr, uint32_t* plen, uint32_t* pramaddr) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint32_t len = 0;
    do {
	result = spi_read_bytes(baseaddr, 4, (uint8_t*) &len);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	if (0 == len) {
	    *plen = 0;
	    *pramaddr = 0;
	}else {
	    result = spi_read_bytes(baseaddr + 4, 4, (uint8_t*) pramaddr);
	    *plen = len << 2;
	}
    }while(0);

    return result;
}

CyFx3BootErrorCode_t spi_load_firmware_baseaddr(uint32_t baseaddr, uint32_t* pjmpaddr) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint32_t checksum = 0;
    do {
	uint8_t head[4] = { 0 };
	result = spi_read_bytes(baseaddr, 4, head);
	if ('C' != head[0] || 'Y' != head[1]) {
	    result = CY_FX3_BOOT_ERROR_NOT_CONFIGURED;
	    break;
	}
	baseaddr += 4;

	uint32_t len = 0;
	uint32_t ramaddr = 0;
	do {
	    result = spi_load_firmware_seg_head(baseaddr, &len, &ramaddr);
	    if (0 == len) {
		break;
	    }else if (0x10000 < len) {
		result = CY_FX3_BOOT_ERROR_MEMORY_ERROR;
		break;
	    }

	    baseaddr += 8;
	    result = spi_read_bytes(baseaddr, len, (uint8_t*) ramaddr);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }
	    uint32_t* pram = (uint32_t*) ramaddr;
	    uint32_t i32len = len >> 2;
	    for (int i = 0; i < i32len; i++) {
		checksum += pram[i];
	    }

	    baseaddr += len;
	}while(1);

	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	baseaddr += 4;
	uint32_t jmpaddr = 0;
	result = spi_read_bytes(baseaddr, 4, (uint8_t*) &jmpaddr);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}

	baseaddr += 4;
	uint32_t rchecksum = 0;
	result = spi_read_bytes(baseaddr, 4, (uint8_t*) &rchecksum);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	
	if (rchecksum == checksum) {
	    *pjmpaddr = jmpaddr;
	}else {
	    *pjmpaddr = 0;
	    result = CY_FX3_BOOT_ERROR_MEMORY_ERROR;
	}
    }while(0);

    return result;
}

CyFx3BootErrorCode_t spi_load_firmware_index(uint8_t indx, uint32_t* jumpaddr) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint32_t spiaddr = indx ? M25P40_IMAGE_A_STORAGE_ADDR : M25P40_IMAGE_B_STORAGE_ADDR;
    result = spi_load_firmware_baseaddr(spiaddr, jumpaddr);
    if (CY_FX3_BOOT_SUCCESS != result) {
	spiaddr = indx ? M25P40_IMAGE_B_STORAGE_ADDR : M25P40_IMAGE_A_STORAGE_ADDR;
	
	result = spi_load_firmware_baseaddr(spiaddr, jumpaddr);
    }
    return result;
}

CyFx3BootErrorCode_t dfu_load_firmware(uint32_t* jumpaddr) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint8_t indx = 0;
    *jumpaddr = 0;
    do {
    	result = dfu_get_current_firmware_index(&indx);
    	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	
	result = spi_load_firmware_index(indx, jumpaddr);
	
    }while(0);

    return result;
}

CyFx3BootErrorCode_t dfu_set_current_firmware_index() {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    do {
	uint8_t readbuf[M25P40_PAGE_SIZE] = { 0x00 };
	result = spi_read_bytes(M25P40_BOOT_INDEX_STORAGE_ADDR, M25P40_PAGE_SIZE, readbuf);
	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
	}
	uint16_t byteidx = find_first_non_zero_bytes(readbuf, M25P40_PAGE_SIZE);
	if (0x00 == readbuf[byteidx]) {
	    byteidx = byteidx + 1;
	}
	if (byteidx >= M25P40_PAGE_SIZE) {
	    result = spi_sector_erase(0x07);
	}else {
	    readbuf[byteidx] = readbuf[byteidx] >> 1;
	}
	result = spi_write_bytes(M25P40_BOOT_INDEX_STORAGE_ADDR, M25P40_PAGE_SIZE, readbuf);
    }while(0);
    
    return result;
}

CyFx3BootErrorCode_t spi_write_firmware(uint8_t* buffer, int32_t length) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;

    uint8_t indx = 0;
    do {
    	result = dfu_get_current_firmware_index(&indx);
    	if (CY_FX3_BOOT_SUCCESS != result) {
	    break;
    	}

	if (0 == indx) {
	    result = spi_sector_erase(0x01);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }
	    result = spi_sector_erase(0x02);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }
	    result = spi_sector_erase(0x03);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }

	    result = spi_write_continue(M25P40_IMAGE_A_STORAGE_ADDR, length, buffer);
	}else if (1 == indx) {
	    result = spi_sector_erase(0x04);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }
	    result = spi_sector_erase(0x05);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }
	    result = spi_sector_erase(0x06);
	    if (CY_FX3_BOOT_SUCCESS != result) {
		break;
	    }

	    result = spi_write_continue(M25P40_IMAGE_B_STORAGE_ADDR, length, buffer);
	}

    }while(0);
    return result;
}

CyFx3BootErrorCode_t dfu_download_sync(uint8_t* buffer, int32_t length) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    if (length > 0) {
    	if (buffer[0] == 'C' && buffer[1] == 'Y') {		// Firmware image
	    result = spi_write_firmware(buffer, length);
	    if (CY_FX3_BOOT_SUCCESS == result) {
	    	dfu_set_current_firmware_index();
	    }
	}else if (buffer[0] == 0xFF && buffer[1] == 0xFE) {	// Serial number format win UTF-16 LE.
	    result = dfu_write_sn(buffer, (length & 0xFF));
	}
    }else {
	result = CY_FX3_BOOT_ERROR_BAD_ARGUMENT;
    }

    return result;
}

CyFx3BootErrorCode_t spi_dfu_init(void) {
    CyFx3BootErrorCode_t result = CyFx3BootSpiInit();
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }

    CyFx3BootSpiConfig_t spiCfg = {
        .isLsbFirst = CyFalse,  // Data shift mode: LSB first.
        .cpol = CyFalse,        // Clock polarity: SCK idles low.
        .cpha = CyFalse,        // Clock phase: Slave samples at idle-active edge.
        .ssnPol = CyFalse,      // Polarity of SSN line: SSN is active low.
        .ssnCtrl = CY_FX3_BOOT_SPI_SSN_CTRL_FW, //SSN is controlled by teh API.
        .leadTime = CY_FX3_BOOT_SPI_SSN_LAG_LEAD_HALF_CLK,      // SSN leads SCK by a half clock cycle.
        .lagTime = CY_FX3_BOOT_SPI_SSN_LAG_LEAD_HALF_CLK,       // SSN lags SCK by a half clock cycle.
        .clock = 10000000,      // Clock frequency in Hz.
        .wordLen = 8,           // Word length in bits.
    };
    result = CyFx3BootSpiSetConfig(&spiCfg);
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }
    
    /*
    spi_sector_erase(0x00);
    spi_sector_erase(0x01);
    spi_sector_erase(0x02);
    spi_sector_erase(0x03);
    spi_sector_erase(0x04);
    spi_sector_erase(0x05);
    spi_sector_erase(0x06);
    spi_sector_erase(0x07);
    */

    return result;
}

void spi_dfu_deinit(void) {
    CyFx3BootSpiDeInit();
}

#include <cyfx3device.h>
#include <cyfx3usb.h>
#include <cyfx3spi.h>

#include "usb_dfu.h"
#include "spi_dfu.h"
#include "fx3_ram_define.h"

int main (void) {
    // Initilizes the FX3 device.
    CyFx3BootDeviceInit(
	    CyTrue	// Set system clock faster than 400MHz.
	    );

    CyFx3BootIoMatrixConfig_t ioCfg = {
	.isDQ32Bit = CyFalse,	// GPIF bus width is 16 bit.
	.useUart = CyFalse,	// UART interface is not to be used.
	.useI2C = CyFalse,	// I2C interface is not to be used.
	.useI2S = CyFalse,	// I2S interface is not to be used.
	.useSpi = CyTrue,	// SPI interface is to be used.
	.gpioSimpleEn[0] = 0,	// Bitmap variable configured as simple GPIOs.
	.gpioSimpleEn[1] = 0,	// Bitmap variable configured as simple GPIOs.
    };
    
    // Configure the IO matrix for the device.
    CyFx3BootErrorCode_t status = CyFx3BootDeviceConfigureIOMatrix(&ioCfg);
    if (CY_FX3_BOOT_SUCCESS != status) {
	// Reset the FX3 device.
	CyFx3BootDeviceReset();
	return status;
    }

    status = spi_dfu_init();
    if (CY_FX3_BOOT_SUCCESS != status) {
	// Reset the FX3 device.
	CyFx3BootDeviceReset();
	return status;
    }

    do {
    	uint8_t* sn_addr = (uint8_t*) FX3_RAM_SN_ADDR;
    	status = dfu_read_sn(sn_addr);		
	if (CY_FX3_BOOT_SUCCESS != status) {
	    sn_addr[0] = 0x08;
	    sn_addr[1] = 0x03;
	    sn_addr[2] = 'E';
	    sn_addr[3] = 0x00;
	    sn_addr[4] = 'r';
	    sn_addr[5] = 0x00;
	    sn_addr[6] = 0x30 + status;
	    sn_addr[7] = 0x00;
	    break;
	}

    	uint32_t* boot_mask_addr = (uint32_t*) FX3_RAM_BOOT_MASK_ADDR;
    	if (FX3_RAM_BOOT_MASK_MAGIC == boot_mask_addr[0]) {
	    boot_mask_addr[0] = 0;
	    break;
	}else {
	    uint32_t jumpaddr = 0;
	    status = dfu_load_firmware(&jumpaddr);
	    if (CY_FX3_BOOT_SUCCESS == status && jumpaddr) {
	    	spi_dfu_deinit();
	    	CyFx3BootJumpToProgramEntry(jumpaddr);
	    	return status;
	    }
	}
    }while(0);


    status = usb_dfu_init();
    for (;;) {
	// Handler that manages USB link state changes.
	// Equivalent to calling the 
	// CyFx3BootUsbCheckUsb3Disconnect, (Check if the device state is in USB3.0 disconnect state.)
	// CyFx3BootUsbEp0StatusCheck, (Check if the status stage of a EP0 transfer is pending.)
	// CyFx3BootUsbSendCompliancePatterns. (Used to send the compliance patterns.)
	CyFx3BootUsbHandleEvents();
    }
    return 0;
}

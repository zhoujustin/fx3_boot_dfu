#include <string.h>

#include <cyfx3device.h>
#include <cyfx3usb.h>
#include <cyfx3spi.h>
#include <cyfx3utils.h>
#include "usb_dfu.h"
#include "spi_dfu.h"
#include "fx3_ram_define.h"

#define USB_SETUP_REQUESTTYPE_DIRECTION(bmRequestType)	((bmRequestType >> 7) & 0x01)
#define USB_SETUP_REQUESTTYPE_TYPE(bmRequestType)	((bmRequestType >> 5) & 0x03)
#define USB_SETUP_REQUESTTYPE_RECIPIENT(bmRequestType)	(bmRequestType & 0x1F)

#define USB_SETUP_REQUESTTYPE_DIRECTION_HOST_TO_DEVICE		(0)
#define USB_SETUP_REQUESTTYPE_DIRECTION_DEVICE_TO_HOST		(1)

#define USB_SETUP_REQUESTTYPE_TYPE_STANDARD			(0)
#define USB_SETUP_REQUESTTYPE_TYPE_CLASS			(1)
#define USB_SETUP_REQUESTTYPE_TYPE_VENDOR			(2)

#define USB_SETUP_REQUESTTYPE_RECIPIENT_DEVICE			(0)
#define USB_SETUP_REQUESTTYPE_RECIPIENT_INTERFACE		(1)
#define USB_SETUP_REQUESTTYPE_RECIPIENT_ENDPOINT		(2)
#define USB_SETUP_REQUESTTYPE_RECIPIENT_OTHER			(3)
#define USB_SETUP_REQUESTTYPE_RECIPIENT_VENDORSPECIFIC		(31)

#define USB_GET_STATUS						(0)
#define USB_CLEAR_FEATURE					(1)
#define USB_SET_FEATURE						(3)
#define USB_SET_ADDRESS						(5)
#define USB_GET_DESCRIPTOR					(6)
#define USB_SET_DESCRIPTOR					(7)
#define USB_GET_CONFIG						(8)
#define USB_SET_CONFIG						(9)
#define USB_GET_INTERFACE					(10)
#define USB_SET_INTERFACE					(11)

#define USB_CLASS_DFU_DETACH					(0x00)
#define USB_CLASS_DFU_DNLOAD					(0x01)
#define USB_CLASS_DFU_UPLOAD					(0x02)
#define USB_CLASS_DFU_GETSTATUS					(0x03)
#define USB_CLASS_DFU_CLRSTATUS					(0x04)
#define USB_CLASS_DFU_GETSTATE					(0x05)
#define USB_CLASS_DFU_ABORT					(0x06)

#define M25P40_PAGE_SIZE		(256)
#define M25P40_SERIANLNUM_STORAGE_ADDR	(0x8000)

typedef enum _usb_setup_status {
    USB_SETUP_STATUS_STALL,
    USB_SETUP_STATUS_ACK,
    USB_SETUP_STATUS_IN,
    USB_SETUP_STATUS_OUT,
}usb_setup_status;

typedef enum _usb_class_status {
    USB_CLASS_STATUS_STALL,
    USB_CLASS_STATUS_ACK,
    USB_CLASS_STATUS_IN,
    USB_CLASS_STATUS_OUT,
    USB_CLASS_STATUS_EMPTY,
}usb_class_status;

typedef enum _usb_class_dfu_status {
    USB_CLASS_DFU_STATUS_OK = 0,
    USB_CLASS_DFU_STATUS_TARGET,
    USB_CLASS_DFU_STATUS_FILE,
    USB_CLASS_DFU_STATUS_WRITE,
    USB_CLASS_DFU_STATUS_ERASE,
    USB_CLASS_DFU_STATUS_CHECK_ERASED,
    USB_CLASS_DFU_STATUS_PROG,
    USB_CLASS_DFU_STATUS_VERIFY,
    USB_CLASS_DFU_STATUS_ADDRESS,
    USB_CLASS_DFU_STATUS_NOTDONE,
    USB_CLASS_DFU_STATUS_FIRMWARE,
    USB_CLASS_DFU_STATUS_VENDOR,
    USB_CLASS_DFU_STATUS_USBR,
    USB_CLASS_DFU_STATUS_POR,
    USB_CLASS_DFU_STATUS_UNKNOWN,
    USB_CLASS_DFU_STATUS_STALLEDPKT,
}usb_class_dfu_status;

typedef enum _usb_class_dfu_state {
    USB_CLASS_DFU_STATE_APP_IDLE = 0,
    USB_CLASS_DFU_STATE_APP_DETACH,
    USB_CLASS_DFU_STATE_DFU_IDLE,
    USB_CLASS_DFU_STATE_DFU_DNLOAD_SYNC,
    USB_CLASS_DFU_STATE_DFU_DNBUSY,
    USB_CLASS_DFU_STATE_DFU_DNLOAD_IDLE,
    USB_CLASS_DFU_STATE_DFU_MANIFEST_SYNC,
    USB_CLASS_DFU_STATE_DFU_MANIFEST,
    USB_CLASS_DFU_STATE_DFU_MANIFEST_WAIT_RESET,
    USB_CLASS_DFU_STATE_DFU_UPLOAD_IDLE,
    USB_CLASS_DFU_STATE_DFU_ERROR,
}usb_class_dfu_state;

extern uint8_t g_hs_device_desc[];
extern uint8_t g_ss_device_desc[];
extern uint8_t g_qual_device_desc[];
extern uint8_t g_bos_device_desc[];
extern uint8_t g_configuration_desc[];
extern uint8_t g_alternate_configuration_desc[];

extern uint8_t g_langid_string_desc[];
extern uint8_t g_manufacture_string_desc[];
extern uint8_t g_product_string_desc[];
uint8_t* g_serialnum_string_desc = (uint8_t*) (FX3_RAM_SN_ADDR);

uint8_t* g_usb_dma_buf = (uint8_t*)(USB_DMA_BUF_ADDR);
uint8_t g_usb_config = 0;
uint8_t g_usb_interface = 0;
uint8_t* g_dfu_buf = (uint8_t*) (USB_BUF_HEAP_BASE);
int32_t g_dfu_buf_offset = 0;

usb_class_dfu_status volatile g_dfu_status = USB_CLASS_DFU_STATUS_POR;
usb_class_dfu_state volatile g_dfu_state = USB_CLASS_DFU_STATE_DFU_ERROR;

void usb_event_callback(CyFx3BootUsbEventType_t event) {
    switch (event)
    {
	case CY_FX3_BOOT_USB_RESET:
	    g_usb_config = 0;
	    g_usb_interface = 0;
	    break;
	case CY_FX3_BOOT_USB_CONNECT:
	case CY_FX3_BOOT_USB_DISCONNECT:
	    g_usb_config = 0;
	    g_usb_interface = 0;
	    break;
	case CY_FX3_BOOT_USB_IN_SS_DISCONNECT:
	    break;
	case CY_FX3_BOOT_USB_COMPLIANCE:
	    break;
	default:
	    break;
    }
    return;
}

usb_setup_status usb_setup_standard_request_get_status(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    usb_setup_status status = USB_SETUP_STATUS_STALL;
    CyBool_t bIsStall = CyFalse;
    CyBool_t bIsNak = CyFalse;
    
    switch (USB_SETUP_REQUESTTYPE_RECIPIENT(pktEp0->bmReqType))
    {
	case USB_SETUP_REQUESTTYPE_RECIPIENT_DEVICE:
	    pktEp0->pData[0] = 0x01;	// Bus powered.
	    pktEp0->pData[1] = 0x00;
	    status = USB_SETUP_STATUS_IN;
	    break;
	case USB_SETUP_REQUESTTYPE_RECIPIENT_INTERFACE:
	    if (g_usb_config == 0) {
		status = USB_SETUP_STATUS_STALL;
	    }else {
		pktEp0->pData[0] = 0x00;
		pktEp0->pData[1] = 0x00;
	    	status = USB_SETUP_STATUS_IN;
	    }
	    break;
	case USB_SETUP_REQUESTTYPE_RECIPIENT_ENDPOINT:
	    result = CyFx3BootUsbGetEpCfg(pktEp0->bIdx0, &bIsNak, &bIsStall);
	    if (CY_FX3_BOOT_SUCCESS == result)
	    {
		if (bIsStall) {
		    pktEp0->pData[0] = 0x01;
		    pktEp0->pData[1] = 0x00;
		}else {
		    pktEp0->pData[0] = 0x00;
		    pktEp0->pData[1] = 0x00;
		}
		status = USB_SETUP_STATUS_IN;
	    }else {
		status = USB_SETUP_STATUS_STALL;
	    }
	    break;
	default:
	    status = USB_SETUP_STATUS_STALL;
	    break;
    }
    return status;
}

usb_setup_status usb_setup_standard_request_clear_feature(CyFx3BootUsbEp0Pkt_t pktEp0) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    usb_setup_status status = USB_SETUP_STATUS_STALL;

    result = CyFx3BootUsbSetClrFeature(0, (CyBool_t) g_usb_config, &pktEp0);
    if (CY_FX3_BOOT_SUCCESS != result) {
	status = USB_SETUP_STATUS_STALL;
    }else {
	status = USB_SETUP_STATUS_ACK;
    }

    return status;
}

usb_setup_status usb_setup_standard_request_set_feature(CyFx3BootUsbEp0Pkt_t pktEp0) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    usb_setup_status status = USB_SETUP_STATUS_STALL;

    result = CyFx3BootUsbSetClrFeature(1, (CyBool_t) g_usb_config, &pktEp0);
    if (CY_FX3_BOOT_SUCCESS != result) {
	status = USB_SETUP_STATUS_STALL;
    }else {
	status = USB_SETUP_STATUS_ACK;
    }

    return status;
}

usb_setup_status usb_setup_standard_request_get_config(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    pktEp0->pData[0] = g_usb_config;
    pktEp0->pData[1] = 0x00;

    return USB_SETUP_STATUS_IN;
}

usb_setup_status usb_setup_standard_request_get_descriptor(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_setup_status status = USB_SETUP_STATUS_STALL; 
    CyFx3BootUsbSpeed_t usbspeed = CyFx3BootUsbGetSpeed();
    
    uint32_t len = 0;
    uint8_t* desc_p = 0;

    switch (pktEp0->bVal1)
    {
	case CY_U3P_USB_DEVICE_DESCR:
	    desc_p = g_hs_device_desc; //g_p_usb_desc->usbDevDesc_p;
	    len = desc_p[0];
	    break;
	case CY_U3P_USB_CONFIG_DESCR:
	    if (CY_FX3_BOOT_HIGH_SPEED == usbspeed) {
		desc_p = g_configuration_desc; /// g_hs_configuration_desc;
		desc_p[1] = CY_U3P_USB_CONFIG_DESCR;
	    }else if (CY_FX3_BOOT_FULL_SPEED == usbspeed) {
		desc_p = g_configuration_desc; /// g_fs_configuration_desc;
		desc_p[1] = CY_U3P_USB_CONFIG_DESCR;
	    }
	    len = (desc_p[3] << 8) | desc_p[2];
	    break;
	case CY_U3P_USB_STRING_DESCR:
	    if (0 == pktEp0->bVal0) {
		desc_p = g_langid_string_desc;
	    }else if (1 == pktEp0->bVal0) {
		desc_p = g_manufacture_string_desc;
	    }else if (2 == pktEp0->bVal0) {
		desc_p = g_product_string_desc;
	    }else if (3 == pktEp0->bVal0) {
		desc_p = g_serialnum_string_desc;
	    }
	    
	    //desc_p = g_p_usb_desc->usbStringDesc_p[pktEp0.bVal0];
	    len = desc_p[0];
	    break;
	case CY_U3P_USB_DEVQUAL_DESCR:
	    desc_p = g_qual_device_desc;
	    len = desc_p[0];
	    break;
	case CY_U3P_USB_OTHERSPEED_DESCR:
	    if (CY_FX3_BOOT_HIGH_SPEED == usbspeed) {
		desc_p = g_alternate_configuration_desc; /// g_fs_configuration_desc;
	    }else if (CY_FX3_BOOT_FULL_SPEED == usbspeed) {
		desc_p = g_alternate_configuration_desc; /// g_hs_configuration_desc;
	    }
	    len = (desc_p[3] << 8) | desc_p[2];
	    break;
	case CY_U3P_BOS_DESCR:
	    desc_p = g_bos_device_desc;
	    len = desc_p[3] << 8 | desc_p[2];
	    break;
	default:
	    break;
    }
    if (0 == desc_p) {
	status = USB_SETUP_STATUS_STALL;
    }else {
    	CyFx3BootMemCopy(pktEp0->pData, desc_p, len);
	pktEp0->wLen = len < pktEp0->wLen? len : pktEp0->wLen;
	status = USB_SETUP_STATUS_IN;
    }

    return status;
}

usb_setup_status usb_setup_standard_request_set_config(CyFx3BootUsbEp0Pkt_t pktEp0) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    
    CyFx3BootUsbSpeed_t usbspeed = CyFx3BootUsbGetSpeed();
    uint16_t bulkpcktsize = 0x40;
    uint16_t intrpcktsize = 0x40;
    
    if (CY_FX3_BOOT_HIGH_SPEED == usbspeed) {
	bulkpcktsize = 0x0200;
	intrpcktsize = 0x0400;
    }else if (CY_FX3_BOOT_SUPER_SPEED == usbspeed) {
	bulkpcktsize = 0x0400;
	intrpcktsize = 0x0400;
    }else if (CY_FX3_BOOT_FULL_SPEED == usbspeed) {
	bulkpcktsize = 0x0040;
	intrpcktsize = 0x0040;
    }
    
    g_usb_config = pktEp0.bVal0;

    CyFx3BootUsbEpConfig_t epctrlCfg = {
	.enable = CyTrue,
	.epType = CY_FX3_BOOT_USB_EP_INTR,
	.streams = 0,
	.pcktSize = intrpcktsize,
	.burstLen = 1,
	.isoPkts = 0
    };

    CyFx3BootUsbEpConfig_t epprodCfg = {
	.enable = CyTrue,
	.epType = CY_FX3_BOOT_USB_EP_BULK,
	.streams = 0,
	.pcktSize = bulkpcktsize,
	.burstLen = 1,
	.isoPkts = 0
    };

    CyFx3BootUsbEpConfig_t epconsCfg = {
	.enable = CyTrue,
	.epType = CY_FX3_BOOT_USB_EP_BULK,
	.streams = 0,
	.pcktSize = bulkpcktsize,
	.burstLen = 1,
	.isoPkts = 0
    };

    result = CyFx3BootUsbSetEpConfig(0x01, &epctrlCfg);
    if (CY_FX3_BOOT_SUCCESS != result) {
	return USB_SETUP_STATUS_STALL;
    }

    result = CyFx3BootUsbSetEpConfig(0x02, &epprodCfg);
    if (CY_FX3_BOOT_SUCCESS != result) {
	return USB_SETUP_STATUS_STALL;
    }

    result = CyFx3BootUsbSetEpConfig(0x82, &epconsCfg);
    if (CY_FX3_BOOT_SUCCESS != result) {
	return USB_SETUP_STATUS_STALL;
    }
    return USB_SETUP_STATUS_ACK;
}

usb_setup_status usb_setup_standard_request_get_interface(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    if (0 == g_usb_config) {
	return USB_SETUP_STATUS_STALL;
    }else {
	pktEp0->pData[0] = g_usb_interface;
	pktEp0->pData[1] = 0x00;

	return USB_SETUP_STATUS_IN;
    }
}

usb_setup_status usb_setup_standard_request_set_interface(CyFx3BootUsbEp0Pkt_t pktEp0) {
    g_usb_interface = pktEp0.bVal0;
    return USB_SETUP_STATUS_ACK;
}

usb_setup_status usb_setup_standard_request_set_address(CyFx3BootUsbEp0Pkt_t pktEp0) {
    return USB_SETUP_STATUS_ACK;
}

void usb_setup_request_answer(CyFx3BootUsbEp0Pkt_t pktEp0, usb_setup_status status) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    CyFx3BootUsbAckSetup();
    switch (status)
    {	
	case USB_SETUP_STATUS_ACK:
	    result = CY_FX3_BOOT_SUCCESS;
	    break;
	case USB_SETUP_STATUS_IN:
	    result = CyFx3BootUsbDmaXferData(0x80, (uint32_t)pktEp0.pData, pktEp0.wLen, 1000); 
	    break;
	case USB_SETUP_STATUS_OUT:
	    result = CyFx3BootUsbDmaXferData(0x00, (uint32_t)pktEp0.pData, pktEp0.wLen, 1000); 
	    break;
	case USB_SETUP_STATUS_STALL: 
	default:
	    break;
    }
    if (CY_FX3_BOOT_SUCCESS != result) {
	CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
    }
    return;
}

void usb_setup_standard_request(CyFx3BootUsbEp0Pkt_t pktEp0) {
    usb_setup_status result = USB_SETUP_STATUS_STALL;
    switch (pktEp0.bReq)
    {
	case USB_GET_STATUS:		// 0x00
	    result = usb_setup_standard_request_get_status(&pktEp0);
	    break;
	case USB_CLEAR_FEATURE:		// 0x01
	    result = usb_setup_standard_request_clear_feature(pktEp0);
	    break;
	case USB_SET_FEATURE:		// 0x03
	    result = usb_setup_standard_request_set_feature(pktEp0);
	    break;
	case USB_SET_ADDRESS:		// 0x05
	    result = usb_setup_standard_request_set_address(pktEp0);
	    break;
	case USB_GET_DESCRIPTOR:	// 0x06
	    result = usb_setup_standard_request_get_descriptor(&pktEp0);
	    break;
	case USB_SET_DESCRIPTOR:	// 0x07
	    result = USB_SETUP_STATUS_STALL;
	    break;
	case USB_GET_CONFIG:		// 0x08
	    result = usb_setup_standard_request_get_config(&pktEp0);
	    break;
	case USB_SET_CONFIG:		// 0x09
	    result = usb_setup_standard_request_set_config(pktEp0);
	    break;
	case USB_GET_INTERFACE:		// 0x0A (10)
	    result = usb_setup_standard_request_get_interface(&pktEp0);
	    break;
	case USB_SET_INTERFACE:		// 0x0B (11)
	    result = usb_setup_standard_request_set_interface(pktEp0);
	    break;
	default:
	    result = USB_SETUP_STATUS_STALL;
	    break;
    }

    usb_setup_request_answer(pktEp0, result);
    return ;
}

usb_class_status usb_class_dfu_detach(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;
    CyFx3BootUsbAckSetup();

    return result;
}

usb_class_status usb_class_dfu_dnload(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_EMPTY;
    CyFx3BootUsbAckSetup();
    if (pktEp0->wLen > 0) {
	g_dfu_status = USB_CLASS_DFU_STATUS_OK;
	g_dfu_state = USB_CLASS_DFU_STATE_DFU_DNLOAD_IDLE;
	CyFx3BootErrorCode_t status = CyFx3BootUsbDmaXferData(0x00, (uint32_t)pktEp0->pData, pktEp0->wLen, 1000);
       	if (CY_FX3_BOOT_SUCCESS != status) {
	    CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
	    g_dfu_status = USB_CLASS_DFU_STATUS_STALLEDPKT;
	    g_dfu_state = USB_CLASS_DFU_STATE_DFU_ERROR;
	    g_dfu_buf_offset = 0;
	}else {
	    CyFx3BootMemCopy(g_dfu_buf + g_dfu_buf_offset, pktEp0->pData, pktEp0->wLen);
	    g_dfu_buf_offset += pktEp0->wLen;
	}
	return result;
    }else {
	result = USB_CLASS_STATUS_STALL;
	if (USB_CLASS_DFU_STATE_DFU_DNLOAD_IDLE == g_dfu_state) { 
	    CyFx3BootErrorCode_t status =
	    dfu_download_sync(g_dfu_buf, g_dfu_buf_offset);
	    if (CY_FX3_BOOT_SUCCESS != status) {
		g_dfu_status = USB_CLASS_DFU_STATUS_PROG;
		g_dfu_state = USB_CLASS_DFU_STATE_DFU_ERROR;
	    }
	    g_dfu_status = USB_CLASS_DFU_STATUS_OK;
	    g_dfu_state = USB_CLASS_DFU_STATE_DFU_MANIFEST_WAIT_RESET;
	}
	g_dfu_buf_offset = 0;
    }
    
    return result;
}

usb_class_status usb_class_dfu_upload(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;
    uint32_t address = ((pktEp0->bVal1 << 8) + pktEp0->bVal0) * 0x40;

    CyFx3BootErrorCode_t status = spi_read_bytes(address, 0x40, pktEp0->pData);
    if (CY_FX3_BOOT_SUCCESS != status) {
	pktEp0->wLen = 0;
    }
    result = USB_CLASS_STATUS_IN; 
    return result;
}

usb_class_status usb_class_dfu_getstatus(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;
    pktEp0->pData[0] = g_dfu_status;
    pktEp0->pData[1] = 0x00;
    pktEp0->pData[2] = 0x10;
    pktEp0->pData[3] = 0x00;
    pktEp0->pData[4] = g_dfu_state;
    pktEp0->pData[5] = 0x00;

    pktEp0->wLen = 6;
    
    result = USB_CLASS_STATUS_IN;
    return result;
}

usb_class_status usb_class_dfu_clrstatus(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_ACK;
    g_dfu_buf_offset = 0;
    g_dfu_status = USB_CLASS_DFU_STATUS_OK;
    g_dfu_state = USB_CLASS_DFU_STATE_DFU_IDLE;
    
    return result;
}

usb_class_status usb_class_dfu_getstate(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;

    pktEp0->pData[0] = g_dfu_state;
    result = USB_CLASS_STATUS_IN;

    g_dfu_buf_offset = 0;
    g_dfu_state = USB_CLASS_DFU_STATE_DFU_IDLE;
    g_dfu_status = USB_CLASS_DFU_STATUS_OK;

    return result;
}

usb_class_status usb_class_dfu_abort(CyFx3BootUsbEp0Pkt_t* pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;
    
    g_dfu_buf_offset = 0;
    g_dfu_state = USB_CLASS_DFU_STATE_DFU_IDLE;
    g_dfu_status = USB_CLASS_DFU_STATUS_OK;
    
    return result;
}

void usb_class_request_answer(CyFx3BootUsbEp0Pkt_t pktEp0, usb_class_status status) {
    CyFx3BootErrorCode_t result = CY_FX3_BOOT_ERROR_FAILURE;
    switch (status)
    {	
	case USB_CLASS_STATUS_ACK:
	    CyFx3BootUsbAckSetup();
	    result = CY_FX3_BOOT_SUCCESS;
	    break;
	case USB_CLASS_STATUS_IN:
	    CyFx3BootUsbAckSetup();
	    result = CyFx3BootUsbDmaXferData(0x80, (uint32_t)pktEp0.pData, pktEp0.wLen, 1000); 
	    break;		
	case USB_CLASS_STATUS_STALL: 
	default:
	    CyFx3BootUsbAckSetup();
	case USB_CLASS_STATUS_EMPTY:
	    break;
    }
    if (USB_CLASS_STATUS_EMPTY == status || USB_CLASS_STATUS_OUT == status) {
	return;
    }else if (CY_FX3_BOOT_SUCCESS != result) {
	CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
    }
    return;
}

void usb_setup_class_request(CyFx3BootUsbEp0Pkt_t pktEp0) {
    usb_class_status result = USB_CLASS_STATUS_STALL;
    switch (pktEp0.bReq)
    {
	case USB_CLASS_DFU_DETACH:		// 0x00
	    result = usb_class_dfu_detach(&pktEp0);
	    break;
	case USB_CLASS_DFU_DNLOAD:		// 0x01
	    result = usb_class_dfu_dnload(&pktEp0);
	    break;
	case USB_CLASS_DFU_UPLOAD:		// 0x02
	    result = usb_class_dfu_upload(&pktEp0);
	    break;
	case USB_CLASS_DFU_GETSTATUS:		// 0x03
	    result = usb_class_dfu_getstatus(&pktEp0);
	    break;
	case USB_CLASS_DFU_CLRSTATUS:		// 0x04
	    result = usb_class_dfu_clrstatus(&pktEp0);
	    break;
	case USB_CLASS_DFU_GETSTATE:		// 0x05
	    result = usb_class_dfu_getstate(&pktEp0);
	    break;
	case USB_CLASS_DFU_ABORT:		// 0x06
	    result = usb_class_dfu_abort(&pktEp0);
	    break;
	default:
	    CyFx3BootUsbAckSetup();
	    CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
	    break;
    }
    usb_class_request_answer(pktEp0, result);
    return ;
}

void usb_setup_data_handler(uint32_t setupDat0, uint32_t setupDat1) {
   CyFx3BootUsbEp0Pkt_t pktEp0;
   uint32_t* pPktEp0 = (uint32_t*) &pktEp0;
   pPktEp0[0] = setupDat0;
   pPktEp0[1] = setupDat1;
   pktEp0.pData = g_usb_dma_buf;

   switch (USB_SETUP_REQUESTTYPE_TYPE(pktEp0.bmReqType))
   {
        case USB_SETUP_REQUESTTYPE_TYPE_STANDARD:
	    usb_setup_standard_request(pktEp0);
	    break;
        case USB_SETUP_REQUESTTYPE_TYPE_CLASS:
	    usb_setup_class_request(pktEp0);
	    break;
	case USB_SETUP_REQUESTTYPE_TYPE_VENDOR:
	    pktEp0.pData[0] = 0x02;
	    CyFx3BootUsbDmaXferData(0x80, (uint32_t)pktEp0.pData, 1, 1000);
	    CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
	    break;
        default:
	    pktEp0.pData[0] = 0x03;
	    CyFx3BootUsbDmaXferData(0x80, (uint32_t)pktEp0.pData, 1, 1000); 
	    CyFx3BootUsbStall(0x00, CyTrue, CyFalse);
	    break;
   }

   return ;
}

CyFx3BootErrorCode_t usb_dfu_init(void) {
    CyFx3BootErrorCode_t result;

    // Start the USB driver.
    result = CyFx3BootUsbStart(
	    CyFalse, 	// Force re-enumeration.
	    usb_event_callback	// USB events notification callback.
	    );
    if (CY_FX3_BOOT_SUCCESS != result) {
	return result;
    }
    
    CyFx3BootRegisterSetupCallback(
	    usb_setup_data_handler
	    );

    // Enable the USB PHYs.
    CyFx3BootUsbConnect(
	    CyTrue,	// Enable USB connection.
	    CyFalse	// HS enumeration.
	    );
    return CY_FX3_BOOT_SUCCESS;
}

#ifndef __USB_DFU_H__
#define __USB_DFU_H__

#define USB_CDC_NULL_STRING_INDEX		(0x00)
#define USB_CDC_LANGID_STRING_INDEX		(0x00)
#define USB_CDC_MANUFACTURE_STRING_INDEX	(0x01)
#define USB_CDC_PRODUCT_STRING_INDEX		(0x02)

#include <cyfx3error.h>
#ifdef __cplusplus
extern "C"
{
#endif

    CyFx3BootErrorCode_t usb_dfu_init(void);

#ifdef __cplusplus
}
#endif

#endif//__USB_DFU_H__

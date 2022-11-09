.text

.global g_langid_string_desc,g_manufacture_string_desc,g_product_string_desc,g_hs_device_desc,g_ss_device_desc,g_qual_device_desc,g_bos_device_desc,g_configuration_desc,g_alternate_configuration_desc,g_bootversion_string_desc

// String Index 0
g_langid_string_desc:
    .byte g_langid_string_desc_end - g_langid_string_desc
    .byte 0x03					// String Descriptor Type
    .hword 0x0409				// EN language id
g_langid_string_desc_end:

// String Index 1
g_manufacture_string_desc:
    .byte g_manufacture_string_desc_end - g_manufacture_string_desc
    .byte 0x03					// String Descriptor Type
    .hword 'J','u','s','t','i','n','-','S','t','u','d','i','o'
    .hword 0
g_manufacture_string_desc_end:

// String Index 2
g_product_string_desc:
    .byte g_product_string_desc_end - g_product_string_desc
    .byte 0x03					// String Descriptor Type
    .hword 'D','F','U'
    .hword 0
g_product_string_desc_end:

// String Index 3 for Serial Number

g_hs_device_desc:
    .byte g_hs_device_desc_end - g_hs_device_desc
    .byte 0x01					// Device Descriptor Type
    .hword 0x0210				// bcdUSB 2.10
    .byte 0x00					// Device Class
    .byte 0x00					// Device Sub-class
    .byte 0x00					// Device Protocol
    .byte 0x40					// Max Package Size for EP0: 64bytes
    .hword 0xF055				// bcdVID
    .hword 0x06C1				// bcdPID
    .hword 0x0001				// bcdRelease
    .byte 0x01					// Manufacture Srting Index
    .byte 0x02					// Product String Index
    .byte 0x03					// Serinal Number String Index
    .byte 0x01					// Number of the configuration(s)
g_hs_device_desc_end:

g_ss_device_desc:
    .byte g_ss_device_desc_end - g_ss_device_desc
    .byte 0x01					// Device Descriptor Type
    .byte 0x310					// bcdUSB 3.10
    .byte 0x00					// Device Class
    .byte 0x00					// Device Sub-class
    .byte 0x00					// Device Protocol
    .byte 0x09					// Max Package Size for EP0: 2^9=512bytes
    .byte 0xF055				// bcdVID
    .byte 0x06C1				// bcdPID
    .byte 0x0001				// bcdRelease
    .byte 0x01					// Manufacture String Index
    .byte 0x02					// Product String Index
    .byte 0x03					// Serial Number String Index
    .byte 0x01					// Number of the configuration(s)
g_ss_device_desc_end:

g_qual_device_desc:
    .byte g_qual_device_desc_end - g_qual_device_desc
    .byte 0x06					// Device Qualifier Descriptor Type
    .hword 0x0200				// bcdUSB 2.0
    .byte 0xFF					// Device Class
    .byte 0xFF					// Device Sub-class
    .byte 0xFF					// Device Protocol
g_qual_device_desc_end:

g_bos_device_desc:
g_bos_device_desc_head:
    .byte g_bos_device_desc_head_end - g_bos_device_desc_head
    .byte 0x0F					// BOS Descriptor Type
    .hword g_bos_device_desc_end - g_bos_device_desc
    .byte 0x01					// Number of device capability descriptor(s)
g_bos_device_desc_head_end:
g_bos_device_usb2_capability_desc:
    .byte g_bos_device_usb2_capability_desc_end - g_bos_device_usb2_capability_desc
    .byte 0x10					// Device Capability Descriptor Type
    .byte 0x02					// USB 2.0 Extension Capacility Type
    .word 0x00000002				// Link Power Management
g_bos_device_usb2_capability_desc_end:
g_bos_device_desc_end:

g_configuration_desc:
g_configuration_desc_head:
    .byte g_configuration_desc_head_end - g_configuration_desc_head
    .byte 0x02					// Configuration Type
    .hword g_configuration_desc_end - g_configuration_desc
    .byte 0x01					// Number of the interface
    .byte 0x01					// Configuration number
    .byte 0x00					// Configuration String Index
    .byte 0x40					// Configuration characteristics: D6-Self Power
    .byte 0x32					// Max power consumption of device (in 2mA unit) 100mA
g_configuration_desc_head_end:
g_configuration_interface_desc:
    .byte g_configuration_interface_desc_end - g_configuration_interface_desc
    .byte 0x04					// Interface Descriptor Type
    .byte 0x00					// Interface number
    .byte 0x00					// Alternate setting number
    .byte 0x00					// Number of endpoint(s)
    .byte 0xFE					// Interface Class
    .byte 0x01					// Interface Sub-class
    .byte 0x02					// Interface Protocol
    .byte 0x00					// Interface Descriptor String Index
g_configuration_interface_desc_end:
g_configuration_function_desc:
    .byte g_configuration_function_desc_end - g_configuration_function_desc
    .byte 0x21					// DFU Function Descriptor Type
    .byte 0x03					// DFU attributes: Download & Upload
    .hword 0x0800				// Detach Timeout: 4096ms
    .hword 0x0040				// Maximum number of bytes per transaction: 64bytesi
    .hword 0x0110				// bcdDFUVersion
g_configuration_function_desc_end:
g_configuration_desc_end:

g_alternate_configuration_desc:
g_alternate_configuration_desc_head:
    .byte g_alternate_configuration_desc_head_end - g_alternate_configuration_desc_head
    .byte 0x07					// Otherspeed Descriptor Type
    .hword g_alternate_configuration_desc_end - g_alternate_configuration_desc
    .byte 0x01					// Number of the interface
    .byte 0x01					// Configuration number
    .byte 0x00					// Configuration String Index
    .byte 0x40					// Configuration characteristics: D6-Self Power
    .byte 0x32					// Max power consumption of device (in 2mA unit) 100mA
g_alternate_configuration_desc_head_end:
g_alternate_configuration_interface_desc:
    .byte g_alternate_configuration_interface_desc_end - g_alternate_configuration_interface_desc
    .byte 0x04					// Interface Descriptor Type
    .byte 0x00					// Interface number
    .byte 0x00					// Alternate setting number
    .byte 0x00					// Number of endpoint(s)
    .byte 0xFE					// Interface Class
    .byte 0x01					// Interface Sub-class
    .byte 0x02					// Interface Protocol
    .byte 0x00					// Interface Descriptor String Index
g_alternate_configuration_interface_desc_end:
g_alternate_configuration_function_desc:
    .byte g_alternate_configuration_function_desc_end - g_alternate_configuration_function_desc
    .byte 0x21					// DFU Function Descriptor Type
    .byte 0x03					// DFU attributes: Download & Upload
    .hword 0x0800				// Detach Timeout: 4096ms
    .hword 0x0040				// Maximum number of bytes per transaction: 64bytesi
    .hword 0x0001				// bcdDFUVersion
g_alternate_configuration_function_desc_end:
g_alternate_configuration_desc_end:


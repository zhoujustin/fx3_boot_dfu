
OPTION (CYFX3_512K "CYUSB3014" ON)
OPTION (BOOT_LIB "" ON)

SET (cyfx3version 1_3_4)
SET (CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/Generic-GNU-ARM926EJS.cmake")

if (CYFX3_512K)
    SET (cyfx3sdkldfile cyfx3_512k.ld)
    SET (cymem -DCYMEM_512K)
else()
    SET (cyfx3sdkldfile cyfx3_256k.ld)
    SET (cymem -DCYMEM_256K)
endif()

if (BOOT_LIB)
    SET (cyfx3sdkinc ${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/boot_lib/include)
    
    LINK_DIRECTORIES (${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/boot_lib/lib)
    
    SET (cyfx3sdkld ${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/boot_lib/${cyfx3sdkldfile})
    
    LINK_LIBRARIES (cyfx3boot)
else()
    SET (cyfx3sdkinc ${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/fw_lib/inc)
    
    LINK_DIRECTORIES (${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/fw_lib/fx3_release)
    
    SET (cyfx3sdkld ${CMAKE_CURRENT_LIST_DIR}/${cyfx3version}/fw_lib/${cyfx3sdkldfile})
    
    LINK_LIBRARIES (cyfxapi cyu3lpp cyu3sport cyu3threadx)
endif()

#SET (CMAKE_C_FLAGS_INIT "-std=c18 -g -Os -Wall -mcpu=arm926ej-s -mthumb-interwork -fmessage-length=0 -ffunction-sections -fdata-sections -Wno-write-strings -Wcast-align --specs=nosys.specs ${cymem} -I${cyfx3sdkinc}")
SET (CMAKE_C_FLAGS_INIT "-std=gnu11 -g -Os -Wall -mcpu=arm926ej-s -marm -mthumb-interwork -fmessage-length=0 -ffunction-sections -fdata-sections -fsigned-char -Wno-write-strings -Wcast-align --specs=nosys.specs --specs=nano.specs ${cymem} -I${cyfx3sdkinc}")
SET (CMAKE_CXX_FLAGS_INIT "-std=c++17 -g -Os -Wall -mcpu=arm926ej-s -marm -mthumb-interwork -fmessage-length=0 -ffunction-sections -fdata-sections -Wno-write-Strings -Wcast-align -fno-exceptions -fno-unwind-tables -Wno-write-strings --specs=nosys.specs --specs=nano.specs ${cymem} -I${cyfx3sdkinc}")
SET (CMAKE_ASM_FLAGS_INIT "-g -Wall -mcpu=arm926ej-s -mthumb-interwork")

ADD_LINK_OPTIONS (-T ${cyfx3sdkld} -nostartfiles -Xlinker --gc-sections -Wl,-d -Wl,-elf -Wl,--no-wchar-size-warning -Wl,--entry,Reset_Handler -Wl,-z,max-page-size=0x8000)

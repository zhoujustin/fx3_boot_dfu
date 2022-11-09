
CMAKE_MINIMUM_REQUIRED (VERSION 3.18)

INCLUDE (cyfx3sdk/cyfx3sdk.cmake)

PROJECT (fx3_boot_dfu LANGUAGES C ASM CXX)

FILE (GLOB SRCS src/*.c src/*.S src/*.cpp)

ADD_EXECUTABLE (fx3_boot_dfu ${SRCS})

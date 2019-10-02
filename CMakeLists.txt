cmake_minimum_required(VERSION 3.6)
project(canokey C ASM)
set(CMAKE_C_STANDARD 11)
# set(CMAKE_BUILD_TYPE Release CACHE STRING "select release build")
# set(ENABLE_TESTING OFF CACHE BOOL "disable tests")

aux_source_directory(Src SOURCE_CODE_COMMON)

aux_source_directory(Drivers/STM32L4xx_HAL_Driver/Src SOURCE_CODE_BOARD)
set(STARTUP_ASM_FILE "startup_stm32l432xx.s")
set(CMAKE_SYSTEM_PROCESSOR "cortex-m4")
set(MCU_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32L432KBUx_FLASH.ld")

set(CMAKE_C_FLAGS "-mcpu=${CMAKE_SYSTEM_PROCESSOR} -mthumb -Wall -fdata-sections -ffunction-sections ${CMAKE_C_FLAGS}")
set(CMAKE_ASM_FLAGS "-mcpu=${CMAKE_SYSTEM_PROCESSOR} -mthumb -Wall -fdata-sections -ffunction-sections ${CMAKE_ASM_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-specs=nano.specs -T${MCU_LINKER_SCRIPT} -Wl,--gc-sections ${CMAKE_EXE_LINKER_FLAGS}")

add_definitions(-DMBEDTLS_CONFIG_FILE="${CMAKE_SOURCE_DIR}/Inc/mbedtls-config.h")
add_subdirectory(canokey-core)
set(PRJ_EXTRA_LIB canokey-core)

add_executable(canokey ${SOURCE_CODE_COMMON} ${SOURCE_CODE_BOARD} ${STARTUP_ASM_FILE})
target_link_libraries(canokey PRIVATE ${PRJ_EXTRA_LIB} c m nosys)

target_compile_definitions(canokey PRIVATE USE_HAL_DRIVER USE_FULL_LL_DRIVER STM32L432xx)

target_include_directories(canokey PRIVATE Board/YutrikeyHeadless)
target_include_directories(canokey PRIVATE Drivers/STM32L4xx_HAL_Driver/Inc)
target_include_directories(canokey PRIVATE Drivers/STM32L4xx_HAL_Driver/Inc/Legacy)
target_include_directories(canokey PRIVATE Drivers/CMSIS/Device/ST/STM32L4xx/Include)

target_include_directories(canokey PRIVATE Drivers/CMSIS/Include)
target_include_directories(canokey PRIVATE Inc)


add_custom_command(TARGET canokey POST_BUILD
    COMMAND "${CROSS_COMPILE}size" canokey)

add_custom_command(OUTPUT canokey.bin
    COMMAND "${CROSS_COMPILE}objcopy" -O binary -S canokey canokey.bin
    DEPENDS canokey
)

add_custom_target(dfu
    DEPENDS canokey.bin
    COMMAND dfu-util -a 0 -s 0x08000000 -D canokey.bin -R)

add_custom_target(flash
    DEPENDS canokey.bin
    COMMAND st-flash --reset write canokey.bin 0x08000000)

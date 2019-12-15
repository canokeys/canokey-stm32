cmake_minimum_required(VERSION 3.6)
project(canokey C ASM)
set(CMAKE_C_STANDARD 11)
option(ENABLE_DEBUG_OUTPUT "Print debug messages" ON)

set(STARTUP_ASM_FILE "startup_stm32l432xx.s")
set(CMAKE_SYSTEM_PROCESSOR "cortex-m4")
set(MCU_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32L432KBUx_FLASH.ld")

set(CMAKE_C_FLAGS "-mcpu=${CMAKE_SYSTEM_PROCESSOR} -mthumb -Wall -fno-strict-aliasing -fdata-sections -ffunction-sections ${CMAKE_C_FLAGS}")
set(CMAKE_ASM_FLAGS "-mcpu=${CMAKE_SYSTEM_PROCESSOR} -mthumb -Wall -fdata-sections -ffunction-sections ${CMAKE_ASM_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-specs=nano.specs -T${MCU_LINKER_SCRIPT} -Wl,-Map=layout.map,--cref -Wl,--gc-sections ${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "-Os -g3")

add_definitions(-DMBEDTLS_CONFIG_FILE="${CMAKE_SOURCE_DIR}/Inc/mbedtls-config.h")
add_subdirectory(canokey-core)

aux_source_directory(Src SOURCE_CODE_COMMON)
aux_source_directory(Drivers/STM32L4xx_HAL_Driver/Src SOURCE_CODE_BOARD)
add_executable(canokey ${SOURCE_CODE_COMMON} ${SOURCE_CODE_BOARD} ${STARTUP_ASM_FILE})
target_link_libraries(canokey PRIVATE canokey-core c m nosys)

target_compile_definitions(canokey PRIVATE USE_HAL_DRIVER USE_FULL_LL_DRIVER STM32L432xx)
if (ENABLE_DEBUG_OUTPUT)
    target_compile_definitions(canokey PRIVATE DEBUG_OUTPUT)
endif (ENABLE_DEBUG_OUTPUT)

target_include_directories(canokey PRIVATE Drivers/STM32L4xx_HAL_Driver/Inc)
target_include_directories(canokey PRIVATE Drivers/STM32L4xx_HAL_Driver/Inc/Legacy)
target_include_directories(canokey PRIVATE Drivers/CMSIS/Device/ST/STM32L4xx/Include)
target_include_directories(canokey PRIVATE Drivers/CMSIS/Include)
target_include_directories(canokey PRIVATE Inc)

add_custom_target(git_hash
    DEPENDS "${CMAKE_SOURCE_DIR}/Gen/gen-git-hash.sh"
    COMMAND "./gen-git-hash.sh"
    BYPRODUCTS "${CMAKE_SOURCE_DIR}/Gen/git_hash.c"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Gen")
add_dependencies(canokey git_hash)
target_sources(canokey PRIVATE "${CMAKE_SOURCE_DIR}/Gen/git_hash.c")
# To make sure "git_hash" is built every time we make
add_custom_command(TARGET canokey POST_BUILD
    COMMAND touch "${CMAKE_SOURCE_DIR}/Gen/gen-git-hash.sh")

add_custom_command(TARGET canokey POST_BUILD
    COMMAND "${CROSS_COMPILE}size" canokey)

add_custom_target(canokey.bin
    DEPENDS canokey
    COMMAND "${CROSS_COMPILE}objcopy" -O binary -S canokey canokey.bin)

add_custom_target(dfu
    DEPENDS canokey.bin
    COMMAND sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000 -D canokey.bin -t 1024)

add_custom_target(flash
    DEPENDS canokey.bin
    COMMAND st-flash --reset write canokey.bin 0x08000000)

add_custom_target(cubeflash
    DEPENDS canokey.bin
    COMMAND STM32_Programmer_CLI -c port=SWD -w canokey.bin 0x08000000 -rst)

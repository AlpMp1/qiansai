include_guard(GLOBAL)

set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(ARM_NONE_EABI_TOOLCHAIN_PATH "" CACHE PATH "Arm GNU Toolchain install or bin directory")

set(_ARM_NONE_EABI_HINTS)
foreach(_hint IN ITEMS
    "${ARM_NONE_EABI_TOOLCHAIN_PATH}"
    "$ENV{ARM_NONE_EABI_TOOLCHAIN_PATH}"
    "$ENV{ARM_GNU_TOOLCHAIN_PATH}"
)
    if(_hint)
        list(APPEND _ARM_NONE_EABI_HINTS "${_hint}" "${_hint}/bin")
    endif()
endforeach()

if(WIN32)
    file(GLOB _ARM_NONE_EABI_WINDOWS_BINS LIST_DIRECTORIES true
        "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/*/bin"
        "C:/Program Files/Arm GNU Toolchain arm-none-eabi/*/bin"
    )
    if(_ARM_NONE_EABI_WINDOWS_BINS)
        list(SORT _ARM_NONE_EABI_WINDOWS_BINS ORDER DESCENDING)
        list(APPEND _ARM_NONE_EABI_HINTS ${_ARM_NONE_EABI_WINDOWS_BINS})
    endif()
endif()

find_program(ARM_NONE_EABI_GCC NAMES ${TOOLCHAIN_PREFIX}gcc HINTS ${_ARM_NONE_EABI_HINTS})
if(NOT ARM_NONE_EABI_GCC)
    message(FATAL_ERROR "Could not find ${TOOLCHAIN_PREFIX}gcc. Add the Arm GNU Toolchain bin directory to PATH, or set ARM_NONE_EABI_TOOLCHAIN_PATH.")
endif()

get_filename_component(ARM_NONE_EABI_BIN_DIR "${ARM_NONE_EABI_GCC}" DIRECTORY)
find_program(ARM_NONE_EABI_GXX     NAMES ${TOOLCHAIN_PREFIX}g++     HINTS "${ARM_NONE_EABI_BIN_DIR}")
find_program(ARM_NONE_EABI_AR      NAMES ${TOOLCHAIN_PREFIX}ar      HINTS "${ARM_NONE_EABI_BIN_DIR}")
find_program(ARM_NONE_EABI_RANLIB  NAMES ${TOOLCHAIN_PREFIX}ranlib  HINTS "${ARM_NONE_EABI_BIN_DIR}")
find_program(ARM_NONE_EABI_OBJCOPY NAMES ${TOOLCHAIN_PREFIX}objcopy HINTS "${ARM_NONE_EABI_BIN_DIR}")
find_program(ARM_NONE_EABI_SIZE    NAMES ${TOOLCHAIN_PREFIX}size    HINTS "${ARM_NONE_EABI_BIN_DIR}")

foreach(_tool IN ITEMS ARM_NONE_EABI_GXX ARM_NONE_EABI_AR ARM_NONE_EABI_RANLIB ARM_NONE_EABI_OBJCOPY ARM_NONE_EABI_SIZE)
    if(NOT ${_tool})
        message(FATAL_ERROR "Could not find required Arm GNU tool: ${_tool}")
    endif()
endforeach()

set(CMAKE_C_COMPILER                "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_ASM_COMPILER              "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_CXX_COMPILER              "${ARM_NONE_EABI_GXX}" CACHE FILEPATH "CXX compiler" FORCE)
set(CMAKE_LINKER                    "${ARM_NONE_EABI_GXX}" CACHE FILEPATH "Linker" FORCE)
set(CMAKE_AR                        "${ARM_NONE_EABI_AR}" CACHE FILEPATH "Archiver" FORCE)
set(CMAKE_RANLIB                    "${ARM_NONE_EABI_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
set(CMAKE_OBJCOPY                   "${ARM_NONE_EABI_OBJCOPY}" CACHE FILEPATH "Objcopy" FORCE)
set(CMAKE_SIZE                      "${ARM_NONE_EABI_SIZE}" CACHE FILEPATH "Size" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -g0")
endif()

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_C_LINK_FLAGS "${TARGET_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F407VETx_FLASH.ld\"")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --specs=nano.specs")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")

set(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lstdc++ -lsupc++ -Wl,--end-group")

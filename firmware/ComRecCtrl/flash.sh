#!/bin/bash

# ============================================
# STM32F407VET 烧录脚本
# 依赖: openocd, cmake, ninja
# ============================================

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目配置
PROJECT_NAME="ComRecCtrl"
BUILD_DIR="build/Debug"
ELF_FILE="${BUILD_DIR}/${PROJECT_NAME}.elf"
TOOLCHAIN_FILE="cmake/gcc-arm-none-eabi.cmake"

# OpenOCD 配置 (ST-Link)
OPENOCD_INTERFACE="interface/stlink.cfg"
OPENOCD_TARGET="target/stm32f4x.cfg"

# ============================================
# 函数定义
# ============================================

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 检查并修复失效的构建缓存（如旧 snap cmake 路径）
sanitize_build_dir() {
    local cache_file="${BUILD_DIR}/CMakeCache.txt"

    if [ ! -f "${cache_file}" ]; then
        return
    fi

    local cached_cmake
    local cached_ninja

    cached_cmake="$(grep '^CMAKE_COMMAND:INTERNAL=' "${cache_file}" | cut -d= -f2-)"
    cached_ninja="$(grep '^CMAKE_MAKE_PROGRAM:FILEPATH=' "${cache_file}" | cut -d= -f2-)"

    if [ -n "${cached_cmake}" ] && [ ! -x "${cached_cmake}" ]; then
        log_warn "检测到失效 CMake 路径: ${cached_cmake}，将重建 ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
        return
    fi

    if [ -n "${cached_ninja}" ] && [ ! -x "${cached_ninja}" ]; then
        log_warn "检测到失效 Ninja 路径: ${cached_ninja}，将重建 ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
        return
    fi
}

# 检查依赖工具
check_deps() {
    log_info "检查依赖工具..."
    for tool in cmake ninja openocd arm-none-eabi-gcc; do
        if ! command -v "$tool" &> /dev/null; then
            log_error "未找到工具: $tool，请先安装"
            exit 1
        fi
    done
    log_info "依赖工具检查通过"
}

# 编译项目
build_project() {
    log_info "开始编译项目..."

    sanitize_build_dir

    # CMake 配置
    log_info "配置 CMake..."
    cmake -S . -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
    if [ $? -ne 0 ]; then
        log_error "CMake 配置失败"
        exit 1
    fi

    # 编译
    cmake --build "${BUILD_DIR}"
    if [ $? -ne 0 ]; then
        log_error "编译失败"
        exit 1
    fi

    log_info "编译成功: ${ELF_FILE}"
}

# 烧录固件
flash_firmware() {
    log_info "开始烧录固件..."

    if [ ! -f "${ELF_FILE}" ]; then
        log_error "未找到固件文件: ${ELF_FILE}"
        exit 1
    fi

    openocd \
        -f "${OPENOCD_INTERFACE}" \
        -f "${OPENOCD_TARGET}" \
        -c "program ${ELF_FILE} verify reset exit"

    if [ $? -ne 0 ]; then
        log_error "烧录失败，请检查:"
        log_error "  1. ST-Link 是否正确连接"
        log_error "  2. 目标板是否上电"
        log_error "  3. OpenOCD 配置是否正确"
        exit 1
    fi

    log_info "烧录成功！"
}

# 仅擦除 Flash
erase_flash() {
    log_warn "擦除 Flash..."
    openocd \
        -f "${OPENOCD_INTERFACE}" \
        -f "${OPENOCD_TARGET}" \
        -c "init; halt; stm32f4x mass_erase 0; exit"

    if [ $? -eq 0 ]; then
        log_info "擦除完成"
    else
        log_error "擦除失败"
        exit 1
    fi
}

# 复位目标板
reset_target() {
    log_info "复位目标板..."
    openocd \
        -f "${OPENOCD_INTERFACE}" \
        -f "${OPENOCD_TARGET}" \
        -c "init; reset run; exit"

    log_info "复位完成"
}

# 显示帮助
show_help() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  (无参数)   编译并烧录"
    echo "  -b         仅编译"
    echo "  -f         仅烧录 (需已编译)"
    echo "  -e         擦除 Flash"
    echo "  -r         复位目标板"
    echo "  -h         显示帮助"
}

# ============================================
# 主逻辑
# ============================================
check_deps

case "$1" in
    -b)
        build_project
        ;;
    -f)
        flash_firmware
        ;;
    -e)
        erase_flash
        ;;
    -r)
        reset_target
        ;;
    -h)
        show_help
        ;;
    *)
        build_project
        flash_firmware
        ;;
esac
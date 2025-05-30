# Default to latest Ubuntu LTS.
# (See https://hub.docker.com/r/microsoft/devcontainers-base)
ARG VARIANT="ubuntu"
FROM --platform=linux/amd64 mcr.microsoft.com/devcontainers/base:${VARIANT}

# From https://github.com/raspberrypi/openocd/branches
ARG OPENOCD_VERSION="sdk-2.0.0"

#
# Install apt package dependencies.
#
# For VS Code "xaver.clang-format" extension:
# 
#   clang-format
#
# RP2040 binaries toolchain, plus native toolchain to build 'pioasm' and 'elf2uf2' tools.
# (See Appendix C: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
#
#   build-essential
#   check
#   cmake
#   gcc-arm-none-eabi
#   gdb-multiarch
#   libnewlib-arm-none-eabi
#   libstdc++-arm-none-eabi-newlib
#   ninja-build
#
# OpenOCD build dependencies:
#
#   autoconf
#   automake
#   build-essential
#   libftdi-dev
#   libtool
#   libusb-1.0-0-dev
#   pkg-config
#   texinfo
#
# To fix warnings from Cortex-Debug extension:
# (See https://forums.raspberrypi.com/viewtopic.php?t=333146)
#
#   binutils-multiarch
#

# Use unattended mode for 'apt'
ARG DEBIAN_FRONTEND="noninteractive"
RUN apt-get update && apt-get -y install --no-install-recommends \
    autoconf \
    automake \
    binutils-multiarch \
    bison \
    build-essential \
    check \
    clang-format \
    cmake \
    flex \
    g++ \
    gcc \
    gcc-arm-none-eabi \
    gdb-multiarch \
    gperf \
    libftdi-dev \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    libtool \
    libusb-1.0-0-dev \
    make \
    ninja-build \
    pkg-config \
    texinfo

# Clean apt cache and temporary files
RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Clone fork of OpenOCD for debugging RP2040
ARG PICO_PATH="/opt/pico"
RUN cd ${PICO_PATH} && \
    git clone --branch "${OPENOCD_VERSION}" --depth 1 --single-branch https://github.com/raspberrypi/openocd.git

# Fix warnings from Cortex-Debug extension
# (See https://forums.raspberrypi.com/viewtopic.php?t=333146)
RUN cd /usr/bin && \
    ln -s /usr/bin/objdump objdump-multiarch && \
    ln -s /usr/bin/nm nm-multiarch

# Install OpenOCD (built from source for RP2040)
# (See: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
RUN cd "${PICO_PATH}/openocd" && \
    ./bootstrap && \
    ./configure && \
    make -j$(nproc) && \
    sudo make install

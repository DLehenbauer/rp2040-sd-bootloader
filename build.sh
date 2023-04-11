#!/bin/sh

BUILD_DIR="build"
DIST_DIR="dist"
GCC="/usr/bin/arm-none-eabi-gcc"
GPP="/usr/bin/arm-none-eabi-g++"

# Force the elf and uf2 binary files to always be regenerated on build
# (this is so old uf2 files don't pile up in dist directory)
rm ${BUILD_DIR}/*/*/*.elf
rm ${BUILD_DIR}/*/*/*.uf2

/usr/bin/cmake \
    --no-warn-unused-cli \
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
    -DCMAKE_BUILD_TYPE:STRING=Release \
    -DCMAKE_C_COMPILER:FILEPATH=${GCC} \
    -DCMAKE_CXX_COMPILER:FILEPATH=${GPP} \
    -S. \
    -B./${BUILD_DIR} \
    -G "Unix Makefiles" \

STATUS=$?
if [ $STATUS -ne 0 ]; then
    echo "CMake returned error exit code: ${STATUS}"
    echo "Exiting"
    exit $STATUS
fi

/usr/bin/cmake \
    --build ${BUILD_DIR} \
    --config Release \
    --target all \
    -j 10 \

STATUS=$?
if [ $STATUS -ne 0 ]; then
    echo "CMake returned error exit code: ${STATUS}"
    echo "Exiting"
    exit $STATUS
fi

mkdir -p ${DIST_DIR}
rm -rf ${DIST_DIR}/*
cp ${BUILD_DIR}/src/bootloader/bootloader.uf2 ${DIST_DIR}
cp ${BUILD_DIR}/src/application/blink_noboot2_w_header.uf2 ${DIST_DIR}
cp ${BUILD_DIR}/src/application/bootloader_blink_noboot2_combined.uf2 ${DIST_DIR}

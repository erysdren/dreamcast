#!/bin/sh
cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake $@

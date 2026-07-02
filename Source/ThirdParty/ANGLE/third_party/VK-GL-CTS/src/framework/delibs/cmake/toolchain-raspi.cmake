#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

# Platform defines.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CROSSCOMPILING 1)
set(DE_CPU "DE_CPU_ARM")

# Toolchain/compiler base.
set(CC_PATH			"/opt/raspi/tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi"	CACHE STRING "Cross compiler path")
set(CROSS_COMPILE	"${CC_PATH}/bin/arm-bcm2708hardfp-linux-gnueabi-"				CACHE STRING "Cross compiler prefix")
set(SYSROOT_PATH	"${CC_PATH}/arm-bcm2708hardfp-linux-gnueabi/sysroot"			CACHE STRING "Raspbian sysroot path")

set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")

# Search libs and include files (but not programs) from toolchain dir.
set(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(TARGET_C_FLAGS	"")
set(CMAKE_C_FLAGS	"${TARGET_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS	"${TARGET_C_FLAGS}" CACHE STRING "" FORCE)

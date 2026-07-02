#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2014 The Android Open Source Project
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

set(DE_COVERAGE_BUILD "OFF" CACHE STRING "Build with coverage instrumentation with GCC (ON/OFF)")

if (NOT DE_DEFS)
	message(FATAL_ERROR "Defs.cmake is not included")
endif ()

if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	# Compiler flags for GCC/Clang

	set(TARGET_FLAGS "")

	if (DE_COVERAGE_BUILD)
		if (NOT DE_COMPILER_IS_GCC)
			message(FATAL_ERROR "Coverage build requires GCC")
		endif ()

		add_definitions("-DDE_COVERAGE_BUILD")
		set(TARGET_FLAGS	"-fprofile-arcs -ftest-coverage")
		set(LINK_FLAGS		"${LINK_FLAGS} -lgcov")
	endif ()

	# \note Remove -Wno-sign-conversion for more warnings
	set(WARNING_FLAGS			"-Wall -Wextra -Wno-long-long -Wshadow -Wundef -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers")

	# Need to specify c++ standard version through CMAKE_C_STANDARD and CMAKE_CXX_STANDARD
	# Avoids incorrect addition of argument -std=gnu++XX that may result in build failure to usage of features in
	# greater standard version than the one specified
	set(CMAKE_C_STANDARD 99)
	if (DEQP_LOG_NODE_SOURCE)
		if (NOT DE_COMPILER_IS_MSC)
			set(CMAKE_CXX_STANDARD 23)
		endif ()
	else ()
		set(CMAKE_CXX_STANDARD 17)
	endif ()
	set(CMAKE_C_FLAGS			"${TARGET_FLAGS} ${WARNING_FLAGS} ${CMAKE_C_FLAGS} -pedantic ")
	set(CMAKE_CXX_FLAGS			"${TARGET_FLAGS} ${WARNING_FLAGS} ${CMAKE_CXX_FLAGS}")

	# Set _FILE_OFFSET_BITS=64 on 32-bit build on Linux to enable output log files to exceed 2GB
	if ((DE_CPU_X86) AND (DE_OS_UNIX))
		set(CMAKE_C_FLAGS		"${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
		set(CMAKE_CXX_FLAGS		"${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
	endif ()

	# Force compiler to generate code where integers have well defined overflow
	# Turn on -Wstrict-overflow=5 and check all warnings before removing
	set(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} -fwrapv")
	set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} -fwrapv")

	# Force compiler to not export any symbols.
	# Any static libraries build are linked into the standalone executable binaries.
	set(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} -fvisibility=hidden")
	set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS	"${CMAKE_C_FLAGS} ${TARGET_FLAGS} -w")
	set(DE_3RD_PARTY_CXX_FLAGS	"${CMAKE_CXX_FLAGS} ${TARGET_FLAGS} -w")
elseif (DE_COMPILER_IS_MSC)
	# Compiler flags for msc

	# \note Following unnecessary nagging warnings are disabled:
	# 4820: automatic padding added after data
	# 4255: no function prototype given (from system headers)
	# 4668: undefined identifier in preprocessor expression (from system headers)
	# 4738: storing 32-bit float result in memory
	# 4711: automatic inline expansion
	set(MSC_BASE_FLAGS "/DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS")
	set(MSC_WARNING_FLAGS "/W3 /wd4820 /wd4255 /wd4668 /wd4738 /wd4711")

	set(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} ${MSC_BASE_FLAGS} ${MSC_WARNING_FLAGS}")
	set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} ${MSC_BASE_FLAGS} /EHsc ${MSC_WARNING_FLAGS} /std:c++17")

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS	"${CMAKE_C_FLAGS} ${MSC_BASE_FLAGS} /W0")
	set(DE_3RD_PARTY_CXX_FLAGS	"${CMAKE_CXX_FLAGS} ${MSC_BASE_FLAGS} /EHsc /W0")
else ()
	message(FATAL_ERROR "DE_COMPILER is not valid")
endif ()

if (DE_MINGW AND DE_PTR_SIZE EQUAL 8)
	# Pass -mbig-obj to mingw gas on Win64. COFF has a 2**16 section limit, and
	# on Win64, every COMDAT function creates at least 3 sections: .text, .pdata,
	# and .xdata.
	# Enable static libgcc and libstdc++ also to avoid needing to have
	# Windows builds of the standard libraries distributed.
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wa,-mbig-obj -static -static-libgcc")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj -static -static-libgcc -static-libstdc++")
endif()

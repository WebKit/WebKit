# WebKit dropped MSVC support. This file is only for clang-cl.

# Detect Windows cross-compilation from Linux
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WEBKIT_WIN_CROSS_COMPILE ON)
    message(STATUS "Cross-compiling Windows port from Linux host")

    # Define the sysroot location
    set(WEBKIT_WIN_SYSROOT "${CMAKE_SOURCE_DIR}/WebKitLibraries/windows")

    # Verify SDK exists
    if (NOT EXISTS "${WEBKIT_WIN_SYSROOT}/sdk/include/um/windows.h")
        message(FATAL_ERROR "Windows SDK not found at ${WEBKIT_WIN_SYSROOT}. "
                           "Run Tools/Scripts/check-win-cross-build-deps to download it.")
    endif ()

    # Define paths
    set(WIN_SDK_INC "${WEBKIT_WIN_SYSROOT}/sdk/include")
    set(WIN_CRT_INC "${WEBKIT_WIN_SYSROOT}/crt/include")
    set(WIN_SDK_LIB "${WEBKIT_WIN_SYSROOT}/sdk/lib")
    set(WIN_CRT_LIB "${WEBKIT_WIN_SYSROOT}/crt/lib")

    # Set compiler include flags (-imsvc for clang-cl)
    string(APPEND CMAKE_C_FLAGS " -imsvc ${WIN_CRT_INC} -imsvc ${WIN_SDK_INC}/ucrt -imsvc ${WIN_SDK_INC}/um -imsvc ${WIN_SDK_INC}/shared -imsvc ${WIN_SDK_INC}/winrt")
    string(APPEND CMAKE_CXX_FLAGS " -imsvc ${WIN_CRT_INC} -imsvc ${WIN_SDK_INC}/ucrt -imsvc ${WIN_SDK_INC}/um -imsvc ${WIN_SDK_INC}/shared -imsvc ${WIN_SDK_INC}/winrt")

    # Set linker library path flags
    set(WIN_CROSS_LINK_FLAGS "-libpath:${WIN_CRT_LIB}/x64 -libpath:${WIN_SDK_LIB}/ucrt/x64 -libpath:${WIN_SDK_LIB}/um/x64")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " ${WIN_CROSS_LINK_FLAGS}")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${WIN_CROSS_LINK_FLAGS}")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${WIN_CROSS_LINK_FLAGS}")

    # Set RC compiler include flags
    set(CMAKE_RC_FLAGS "-I ${WIN_SDK_INC}/um -I ${WIN_SDK_INC}/ucrt -I ${WIN_SDK_INC}/shared")

    # Auto-detect LLVM tools (version 20 suffix as installed by deps script)
    if (NOT CMAKE_AR)
        find_program(CMAKE_AR NAMES llvm-lib-20 llvm-lib REQUIRED)
    endif ()
    if (NOT CMAKE_RC_COMPILER)
        find_program(CMAKE_RC_COMPILER NAMES llvm-rc-20 llvm-rc REQUIRED)
    endif ()
    if (NOT CMAKE_RANLIB)
        find_program(CMAKE_RANLIB NAMES llvm-ranlib-20 llvm-ranlib REQUIRED)
    endif ()

    message(STATUS "Cross-compile sysroot: ${WEBKIT_WIN_SYSROOT}")
    message(STATUS "Cross-compile AR: ${CMAKE_AR}")
    message(STATUS "Cross-compile RC: ${CMAKE_RC_COMPILER}")
endif ()

function(MSVC_ADD_COMPILE_OPTIONS)
    foreach (_option ${ARGV})
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:${_option}>)
    endforeach ()
endfunction()

# Use AT&T syntax for inline asm
MSVC_ADD_COMPILE_OPTIONS(/clang:-masm=att)

# Create pdb files for debugging purposes, also for Release builds
MSVC_ADD_COMPILE_OPTIONS(/Zi /GS)

# Disable ICF (identical code folding) optimization,
# as it makes it unsafe to pointer-compare functions with identical definitions.
add_link_options(/DEBUG /OPT:NOICF /OPT:REF)

# We do not use exceptions
add_definitions(-D_HAS_EXCEPTIONS=0)
string(REGEX REPLACE "/EH[-a-z]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
MSVC_ADD_COMPILE_OPTIONS(/EHa- /EHc- /EHs- /fp:except-)

# Disable RTTI
string(REGEX REPLACE "/GR-?" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
MSVC_ADD_COMPILE_OPTIONS(/GR-)

# We have some very large object files that have to be linked
MSVC_ADD_COMPILE_OPTIONS(/analyze- /bigobj)

# Use CRT security features
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)

# Specify the source code encoding
MSVC_ADD_COMPILE_OPTIONS(/utf-8 /validate-charset)

if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
    MSVC_ADD_COMPILE_OPTIONS(/MP)
endif ()

WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(/Wmicrosoft-include)

# More warnings. /W4 should be specified before -Wno-* options for clang-cl.
string(REGEX REPLACE "/W3" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(/W4)

# Not apply class-level dllexport and dllimport attributes to inline member functions
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(/Zc:dllexportInlines-)

# Make sure incremental linking is turned off, as it creates unacceptably long link times.
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
add_link_options(/INCREMENTAL:NO)

# Link clang runtime builtins library
string(REGEX MATCH "^[0-9]+" CLANG_CL_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
cmake_path(REMOVE_FILENAME CMAKE_CXX_COMPILER OUTPUT_VARIABLE CLANG_CL_DIR)
cmake_path(APPEND CLANG_CL_DIR "../lib/clang" ${CLANG_CL_MAJOR_VERSION} "lib/windows")
if (WTF_CPU_ARM64)
    set(CLANG_BUILTINS_ARCH "aarch64")
else ()
    set(CLANG_BUILTINS_ARCH "x86_64")
endif ()

find_library(CLANG_BUILTINS_LIBRARY clang_rt.builtins-${CLANG_BUILTINS_ARCH}
    PATHS ${CLANG_CL_DIR} ${CMAKE_SOURCE_DIR}/WebKitLibraries/windows
    REQUIRED NO_DEFAULT_PATH)
link_libraries(${CLANG_BUILTINS_LIBRARY})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")
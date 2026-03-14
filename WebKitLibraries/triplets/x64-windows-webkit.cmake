set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    # Chainload vcpkg's windows toolchain so the platform is recognized as
    # "windows" (VCPKG_CMAKE_SYSTEM_NAME stays empty) while bypassing the
    # Visual Studio Developer Prompt check that fails on non-Windows hosts.
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/../toolchains/windows-cross.cmake)

    # Only build release configuration; Wine provides builtin release CRT DLLs
    # (msvcp140.dll, vcruntime140.dll, ucrtbase.dll) but not the debug variants.
    set(VCPKG_BUILD_TYPE release)

    set(ENV{CC} clang-cl-20)
    set(ENV{CXX} clang-cl-20)

    # Compute Windows SDK/CRT paths (vcpkg lives one level below WebKitLibraries/windows/)
    set(WEBKIT_LIBRARIES_WINDOWS "${VCPKG_ROOT_DIR}/..")
    cmake_path(NORMAL_PATH WEBKIT_LIBRARIES_WINDOWS)
    set(SDK_INC "${WEBKIT_LIBRARIES_WINDOWS}/sdk/include")
    set(CRT_INC "${WEBKIT_LIBRARIES_WINDOWS}/crt/include")
    set(SDK_LIB "${WEBKIT_LIBRARIES_WINDOWS}/sdk/lib")
    set(CRT_LIB "${WEBKIT_LIBRARIES_WINDOWS}/crt/lib")

    # Pass compiler/linker flags so vcpkg port builds find the Windows SDK/CRT
    set(VCPKG_C_FLAGS "-imsvc ${CRT_INC} -imsvc ${SDK_INC}/ucrt -imsvc ${SDK_INC}/um -imsvc ${SDK_INC}/shared -imsvc ${SDK_INC}/winrt")
    set(VCPKG_CXX_FLAGS "${VCPKG_C_FLAGS}")
    set(VCPKG_LINKER_FLAGS "/LIBPATH:${CRT_LIB}/x64 /LIBPATH:${SDK_LIB}/ucrt/x64 /LIBPATH:${SDK_LIB}/um/x64")

    # Also set INCLUDE/LIB env vars (used by some ports' build systems directly)
    set(ENV{INCLUDE} "${CRT_INC};${SDK_INC}/ucrt;${SDK_INC}/um;${SDK_INC}/shared;${SDK_INC}/winrt")
    set(ENV{LIB} "${CRT_LIB}/x64;${SDK_LIB}/ucrt/x64;${SDK_LIB}/um/x64")

    # Prevent pkg-config from finding host system packages during cross-compilation.
    # set(ENV{...}) in the triplet only affects cmake's own process, not port build
    # subprocesses. VCPKG_ENV_PASSTHROUGH_UNTRACKED tells vcpkg to pass through the
    # caller's PKG_CONFIG_LIBDIR to port builds — build-webkit sets this in the shell.
    set(VCPKG_ENV_PASSTHROUGH_UNTRACKED PKG_CONFIG_LIBDIR)

    # Pass cross-compilation tool paths and RC include flags to vcpkg port cmake builds.
    # CMAKE_RC_FLAGS provides include paths for llvm-rc's clang-cl preprocessing step.
    find_program(HOST_NINJA_EXECUTABLE NAMES ninja ninja-build REQUIRED)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_MAKE_PROGRAM=${HOST_NINJA_EXECUTABLE} -DCMAKE_AR=/usr/bin/llvm-lib-20 -DCMAKE_LINKER=/usr/bin/lld-link-20 -DCMAKE_MT=/usr/bin/llvm-mt-20 -DCMAKE_RC_COMPILER=/usr/bin/llvm-rc-20 "-DCMAKE_RC_FLAGS=-I ${CRT_INC} -I ${SDK_INC}/ucrt -I ${SDK_INC}/um -I ${SDK_INC}/shared" -DCMAKE_CROSSCOMPILING_EMULATOR=wine)

    # Use Wine to run cross-compiled .exe tools (e.g. ICU data generators)
    set(CMAKE_CROSSCOMPILING_EMULATOR wine)
else ()
    set(ENV{CC} cl.exe)
    set(ENV{CXX} cl.exe)
endif ()

set(CMAKE_EXECUTABLE_SUFFIX_C ".exe")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".exe")

# The following libraries should always be static
if (PORT STREQUAL "highway")
    set(VCPKG_LIBRARY_LINKAGE static)
elseif (PORT STREQUAL "pixman")
    set(VCPKG_LIBRARY_LINKAGE static)
endif ()

# Turn on zlib compatibility
if (PORT STREQUAL "zlib-ng")
    set(ZLIB_COMPAT ON)
endif ()

# build icu as editable
# if (PORT MATCHES "icu")
#   set(_VCPKG_EDITABLE ON)
# endif()

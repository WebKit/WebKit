# Standalone wrapper to cross-compile the in-tree libwebrtc for the Wii U
# (PowerPC) in CI. Source/ThirdParty/libwebrtc/CMakeLists.txt has no project()
# of its own -- it is meant to be add_subdirectory'd from the full WebKit build
# -- so this wrapper supplies project()/toolchain context and pulls it in.
#
# This is a bring-up harness: it exists to surface libwebrtc's Wii U porting
# errors (endianness, sockets, platform branches, deps) as a build scoreboard,
# the same way the WKC port library was brought up. It does NOT produce the
# final browser; that link happens elsewhere with full WebCore.

cmake_minimum_required(VERSION 3.16)
project(libwebrtc_wiiu CXX C ASM)

# Wii U/newlib compatibility headers for WebRTC dependencies that assume
# glibc-only headers. These are target-specific and must precede system headers.
include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# Path to the vendored libwebrtc (overridable from the CI invocation).
if (NOT DEFINED LIBWEBRTC_DIR)
    set(LIBWEBRTC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../Source/ThirdParty/libwebrtc")
endif ()

# Wii U has no AV1 codec path; keep it off (also avoids the dav1d PAL include
# forwarding, which belongs to the full WebCore build).
if (NOT DEFINED ENABLE_AV1)
    set(ENABLE_AV1 OFF CACHE BOOL "" FORCE)
endif ()

# Minimal stand-in for WebKit's PCH macro. The real WEBKIT_ADD_PREFIX_HEADER
# (Source/cmake/WebKitMacros.cmake) needs the full WebKit CMake stack, which
# this standalone harness does not load. Replicate the essential effect --
# force-include libwebrtc's prefix header so its global defines apply -- without
# the PCH codegen machinery. CMAKE_CURRENT_SOURCE_DIR resolves to libwebrtc's
# dir when this is called from its CMakeLists via add_subdirectory.
function(WEBKIT_ADD_PREFIX_HEADER _target _header)
    # No Objective-C on Wii U; skip the ObjC prefix header (it pulls ObjC-only
    # content and must not be force-included into C/C++ TUs).
    if (_header MATCHES "ObjC")
        return ()
    endif ()
    set(_path "${CMAKE_CURRENT_SOURCE_DIR}/${_header}")
    if (NOT EXISTS "${_path}")
        set(_path "${_header}")
    endif ()
    if (EXISTS "${_path}")
        # Use SHELL: so CMake does NOT de-duplicate the repeated '-include'
        # token across multiple prefix headers -- otherwise the second header
        # loses its '-include' and becomes a bare input file, tripping
        # "cannot specify -o with -c ... with multiple files".
        target_compile_options(${_target} PRIVATE "SHELL:-include ${_path}")
    endif ()
endfunction()

add_subdirectory(${LIBWEBRTC_DIR} libwebrtc-build)

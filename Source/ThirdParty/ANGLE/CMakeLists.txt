set_property(DIRECTORY . PROPERTY FOLDER "ANGLE")

if (APPLE)
    set(is_apple TRUE)
    if (WEBKIT_SDK_IS_IOS_FAMILY)
        set(is_ios TRUE)
    else ()
        set(is_mac TRUE)
    endif ()
elseif (WIN32)
    set(is_win TRUE)
    set(angle_enable_d3d9 TRUE)
    set(angle_enable_d3d11 TRUE)
    set(angle_is_winuwp FALSE)
elseif (ANDROID)
    set(is_android TRUE)
elseif (UNIX)
    set(is_linux TRUE)
endif ()

include(Compiler.cmake)
include(GLESv2.cmake)

# ANGLE Renderer backends
include(D3D.cmake)
include(GL.cmake)
include(Metal.cmake)
include(Wgpu.cmake)

option(ENABLE_WEBGL_WEBGPU "Build ANGLE's WebGPU backend" OFF)

set(no_gl_prototypes
    GL_GLES_PROTOTYPES=0
    EGL_EGL_PROTOTYPES=0
)

set(gl_prototypes
    GL_GLES_PROTOTYPES=1
    EGL_EGL_PROTOTYPES=1
    GL_GLEXT_PROTOTYPES
    EGL_EGLEXT_PROTOTYPES
)

# Default library types for ANGLE
# Override these in Platform*.cmake for your port as needed.
set(ANGLE_LIBRARY_TYPE STATIC)
set(GLESv2_LIBRARY_TYPE STATIC)
set(EGL_LIBRARY_TYPE STATIC)

# Apple ships one libANGLE-shared.dylib, so the pieces are object libraries
# combined below. GL_GLES_PROTOTYPES=0 would strip the visibility("default")
# attribute the Khronos headers put on the entry points, leaving them unexported.
set(_angle_prototypes ${no_gl_prototypes})
set(_angle_egl_api_definition EGLAPI=)
if (APPLE)
    set(ANGLE_LIBRARY_TYPE OBJECT)
    set(GLESv2_LIBRARY_TYPE OBJECT)
    set(EGL_LIBRARY_TYPE OBJECT)
    set(_angle_prototypes ${gl_prototypes})
    set(_angle_egl_api_definition "")
endif ()

# ANGLE makes a number of small static libraries that are then joined into a
# bigger library that is built shared. Rather than making the small libraries
# there will be a ANGLE whose sources are dependent on whether the library
# is being used as a compiler or as a GLES implementation.
#
# The corresponding gn targets are described below
#
# ANGLE (Compiler only)
# + angle_common
# + preprocessor
# + translator
#
# ANGLE (GLES)
# + ANGLE (Compiler only)
# + xxhash
# + angle_image_util
# + angle_system_utils (OS specific)
# + angle_(renderer) (Backend and OS specific)

set(ANGLE_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/WebKit"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/KHR"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/base"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/third_party/xxhash"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/google"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(ANGLE_DEFINITIONS
    ANGLE_ENABLE_ESSL
    ANGLE_ENABLE_GLSL
)

set(ANGLE_SOURCES
    ${libangle_common_sources}
    ${libangle_common_shader_state_sources}
    ${angle_preprocessor_sources}
    ${angle_translator_glsl_base_sources}
    ${angle_translator_essl_sources}
    ${angle_translator_essl_symbol_table_sources}
    ${angle_translator_glsl_and_vulkan_base_sources}
    ${angle_translator_glsl_sources}
    $<$<BOOL:${is_apple}>:${angle_translator_glsl_apple_sources}>
    ${angle_translator_sources}
    src/common/angle_version_info.cpp
    src/libANGLE/capture/FrameCapture_mock.cpp
    src/libANGLE/capture/serialize_mock.cpp
)

if (WIN32)
    # FIXME: DX11 support will not compile if this preprocessor definition is set
    # DirectX Documentation is setting that version to 0x700 but there is no
    # corresponding value in sdkddkver.h
    remove_definitions(-D_WIN32_WINNT=0x601 -DWINVER=0x601)

    list(APPEND ANGLE_SOURCES
        "src/libANGLE/renderer/dxgi_format_map.h"
        "src/libANGLE/renderer/dxgi_format_map_autogen.cpp"
        "src/libANGLE/renderer/dxgi_support_table.h"
        "src/libANGLE/renderer/dxgi_support_table_autogen.cpp"
    )
endif ()

if (ENABLE_WEBGL_WEBGPU)
    list(APPEND ANGLE_SOURCES
        ${wgpu_backend_sources}

        ${angle_translator_lib_wgsl_sources}

        WebKit/dawn/native/DawnNativeWebGPUFramework.cpp
        WebKit/dawn/dawn_proc.h
        WebKit/dawn/dawn_proc_table.h
        WebKit/dawn/native/DawnNative.h
        WebKit/dawn/webgpu.h
        WebKit/webgpu/webgpu.h
    )

    list(APPEND ANGLE_DEFINITIONS
        ANGLE_ENABLE_WGPU
    )
endif ()

set(ANGLEGLESv2_LIBRARIES
    ANGLE
)

set(zlib_wrapper_sources
    "third_party/zlib/google/compression_utils_portable.h"
    "third_party/zlib/google/compression_utils_portable.cc"
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

add_subdirectory(include)

add_library(ANGLEFramework INTERFACE)
add_dependencies(ANGLEFramework GLSLANGHeaders ANGLEHeaders)

if (USE_ANGLE_EGL OR ENABLE_WEBGL)
    add_library(ANGLE ${ANGLE_LIBRARY_TYPE}
        ${ANGLE_SOURCES}
        ${libangle_sources}
        ${libangle_headers}
        ${libangle_image_util_sources}
        ${libangle_image_util_headers}
        ${xxhash_sources}
        ${zlib_wrapper_sources}
    )
    target_include_directories(ANGLE PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(ANGLE PRIVATE
        ${_angle_prototypes}
        LIBANGLE_IMPLEMENTATION
    )
    target_compile_definitions(ANGLE PUBLIC ${ANGLE_DEFINITIONS})
    target_link_libraries(ANGLE PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})

    target_compile_definitions(ANGLEFramework INTERFACE ${gl_prototypes})

    add_library(GLESv2 ${GLESv2_LIBRARY_TYPE}
        ${libglesv2_entry_point_sources}
        ${libglesv2_sources}
    )
    target_include_directories(GLESv2 PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(GLESv2 PRIVATE
        ${_angle_prototypes}
        LIBGLESV2_IMPLEMENTATION
    )

    target_link_libraries(GLESv2 PRIVATE ${ANGLEGLESv2_LIBRARIES} ${CMAKE_DL_LIBS})
    WEBKIT_REUSE_PREFIX_HEADER(GLESv2 ANGLE ANGLEPrefix.h PREFIX_LANGUAGES CXX)

    if (WIN32)
        # Output library name according to the .def
        target_sources(GLESv2 PRIVATE src/libGLESv2/libGLESv2_autogen.def)
        set_target_properties(GLESv2 PROPERTIES OUTPUT_NAME libGLESv2)
    endif ()

    add_library(GLESv2Framework INTERFACE)
    target_link_libraries(GLESv2Framework INTERFACE GLESv2)
    target_include_directories(GLESv2Framework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR} ${ANGLE_HEADERS_DIR})
    target_compile_definitions(GLESv2Framework INTERFACE USE_SYSTEM_EGL)
    add_dependencies(GLESv2Framework GLSLANGHeaders ANGLEHeaders)
    add_library(ANGLE::GLES ALIAS GLESv2Framework)

    if (USE_ANGLE_EGL)
        add_library(EGL ${EGL_LIBRARY_TYPE}
            ${libegl_sources}
        )
        target_include_directories(EGL PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
        target_compile_definitions(EGL PRIVATE
            ${ANGLE_DEFINITIONS}
            ${gl_prototypes}
            ${_angle_egl_api_definition}
        )

        target_link_libraries(EGL PRIVATE GLESv2 ${CMAKE_DL_LIBS})

        set_target_properties(EGL PROPERTIES LINKER_LANGUAGE CXX)
        if (WIN32)
            # Output library names according to the .def
            target_sources(EGL PRIVATE src/libEGL/libEGL_autogen.def)
            set_target_properties(EGL PROPERTIES OUTPUT_NAME libEGL)
        endif ()

        add_library(EGLFramework INTERFACE)
        target_include_directories(EGLFramework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR}/ ${ANGLE_HEADERS_DIR})
        target_compile_definitions(EGLFramework INTERFACE ${gl_prototypes})
        target_link_libraries(EGLFramework INTERFACE EGL)
        add_dependencies(EGLFramework GLSLANGHeaders ANGLEHeaders)
        add_library(ANGLE::EGL ALIAS EGLFramework)
    endif ()

    if (APPLE)
        # OptionsCocoa.cmake applies -fvisibility=hidden only to C and CXX, so the
        # ObjC++ Metal backend would export all of rx::*Mtl from the dylib.
        foreach (_angle_object_target IN ITEMS ANGLE GLESv2 EGL)
            if (TARGET ${_angle_object_target})
                target_compile_options(${_angle_object_target} PRIVATE
                    "$<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fvisibility=hidden>")
            endif ()
        endforeach ()

        set(_angle_shared_objects $<TARGET_OBJECTS:ANGLE> $<TARGET_OBJECTS:GLESv2>)
        if (USE_ANGLE_EGL)
            list(APPEND _angle_shared_objects $<TARGET_OBJECTS:EGL>)
        endif ()

        add_library(ANGLE-shared SHARED ${_angle_shared_objects})
        target_link_libraries(ANGLE-shared PRIVATE
            ZLIB::ZLIB
            ${CMAKE_DL_LIBS}
            "-framework CoreGraphics"
            "-framework Foundation"
            "-framework IOSurface"
            "-framework Metal"
            "-framework QuartzCore"
        )
        if (WEBKIT_SDK_IS_MACOS)
            target_link_libraries(ANGLE-shared PRIVATE "-framework IOKit")
        endif ()

        # FIXME: Only correct for iOS-family platforms. On macOS the Xcode build
        # resolves NORMAL_UMBRELLA_FRAMEWORKS_DIR to
        # /System/Library/Frameworks/WebKit.framework/Versions/A/Frameworks.
        set_target_properties(ANGLE-shared PROPERTIES
            LINKER_LANGUAGE CXX
            MACOSX_RPATH OFF
            BUILD_WITH_INSTALL_NAME_DIR ON
            INSTALL_NAME_DIR "/System/Library/PrivateFrameworks/WebCore.framework/${WEBKIT_FRAMEWORK_VERSION_PATH}Frameworks"
        )

        set_property(TARGET GLESv2Framework PROPERTY INTERFACE_LINK_LIBRARIES ANGLE-shared)
        if (USE_ANGLE_EGL)
            set_property(TARGET EGLFramework PROPERTY INTERFACE_LINK_LIBRARIES ANGLE-shared)
        endif ()

        _WEBKIT_ADD_CODE_SIGN(ANGLE-shared)
    endif ()
else ()
    add_library(ANGLE ${ANGLE_LIBRARY_TYPE} ${ANGLE_SOURCES})
    target_include_directories(ANGLE PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(ANGLE PRIVATE ${CMAKE_DL_LIBS})
    target_compile_definitions(ANGLE PRIVATE
        ${ANGLE_DEFINITIONS}
        ${no_gl_prototypes}
        LIBANGLE_IMPLEMENTATION
    )
endif ()

WEBKIT_ADD_PREFIX_HEADER(ANGLE ANGLEPrefix.h PREFIX_LANGUAGES CXX OBJCXX)

if (ENABLE_WEBGL)
    set(libglesv2_entry_points_headers
        src/libGLESv2/entry_points_egl_autogen.h
        src/libGLESv2/entry_points_egl_ext_autogen.h
        src/libGLESv2/entry_points_gles_2_0_autogen.h
        src/libGLESv2/entry_points_gles_3_0_autogen.h
        src/libGLESv2/entry_points_gles_ext_autogen.h
    )

    WEBKIT_COPY_FILES(LibGLESv2EntryPointsHeaders
        DESTINATION ${ANGLE_HEADERS_DIR}
        FILES ${libglesv2_entry_points_headers}
        FLATTENED NO_SYMLINK  # adjust-angle-include-paths.py rewrites these in-place
    )
    if (WIN32 AND TARGET GLESv2)
        # GLESv2 needs to have a direct or indirect dependency to
        # LibGLESv2EntryPointsHeaders for CMake Visual Studio generator
        # to eliminate duplicated custom commands. Otherwise,
        # entry_points_*.h will be copied twice in both projects.
        add_dependencies(GLESv2 LibGLESv2EntryPointsHeaders)
    endif ()

    set(_angle_webgl_stamp ${CMAKE_CURRENT_BINARY_DIR}/ANGLE-webgl-headers.stamp)
    add_custom_command(
        OUTPUT ${_angle_webgl_stamp}
        COMMAND ${CMAKE_COMMAND} -E env
            BUILT_PRODUCTS_DIR=${ANGLE_FRAMEWORK_HEADERS_DIR}
            PUBLIC_HEADERS_FOLDER_PATH=/ANGLE
            ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/adjust-angle-include-paths.py
        COMMAND ${CMAKE_COMMAND} -E touch ${_angle_webgl_stamp}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/adjust-angle-include-paths.py
            ${libglesv2_entry_points_headers}
        VERBATIM
    )
    add_custom_target(ANGLE-webgl-headers DEPENDS ${_angle_webgl_stamp})
    add_dependencies(ANGLE-webgl-headers LibGLESv2EntryPointsHeaders ANGLEWebGLHeaders)
    add_dependencies(GLESv2Framework ANGLE-webgl-headers)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    foreach (angle_target ANGLE EGL GLESv2)
        if (TARGET ${angle_target})
            WEBKIT_ADD_TARGET_C_FLAGS(${angle_target} -w)
            WEBKIT_ADD_TARGET_CXX_FLAGS(${angle_target} -w)
        endif ()
    endforeach ()
endif ()

if (TARGET ANGLE-shared)
    target_link_libraries(ANGLEFramework INTERFACE ANGLE-shared)
else ()
    target_link_libraries(ANGLEFramework INTERFACE ANGLE)
endif ()
target_include_directories(ANGLEFramework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR} ${ANGLE_HEADERS_DIR})

add_library(ANGLE::ANGLE ALIAS ANGLEFramework)

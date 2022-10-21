set_property(DIRECTORY . PROPERTY FOLDER "ANGLE")

if (APPLE)
    set(is_mac TRUE)
elseif (WIN32)
    set(is_win TRUE)
    set(angle_enable_d3d9 TRUE)
    set(angle_enable_d3d11 TRUE)
    set(angle_is_winuwp FALSE)
elseif (UNIX)
    set(is_linux TRUE)
endif ()

include(Compiler.cmake)
include(GLESv2.cmake)

# ANGLE Renderer backends
include(D3D.cmake)
include(GL.cmake)
include(Metal.cmake)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/KHR"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/third_party/base"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/google"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(ANGLE_DEFINITIONS
    ANGLE_ENABLE_ESSL
    ANGLE_ENABLE_GLSL
)

set(ANGLE_SOURCES
    ${libangle_common_sources}
    ${angle_preprocessor_sources}
    ${angle_translator_glsl_base_sources}
    ${angle_translator_essl_sources}
    ${angle_translator_essl_symbol_table_sources}
    ${angle_translator_glsl_and_vulkan_base_sources}
    ${angle_translator_glsl_sources}
    ${angle_translator_sources}
    ${angle_system_utils_sources}
    src/common/angle_version_info.cpp
    src/libANGLE/capture/FrameCapture_mock.cpp
    src/libANGLE/capture/frame_capture_utils_mock.cpp
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
    if (NOT angle_is_winuwp)
        list(APPEND ANGLE_SOURCES
            "src/libANGLE/renderer/d3d_format.cpp"
            "src/libANGLE/renderer/d3d_format.h"
        )
    endif()
endif ()

set(ANGLEGLESv2_LIBRARIES
    ANGLE
)

set(zlib_wrapper_sources
    "third_party/zlib/google/compression_utils_portable.h"
    "third_party/zlib/google/compression_utils_portable.cc"
)

set(angle_gl_enum_utils
    "src/libANGLE/gl_enum_utils.cpp"
    "src/libANGLE/gl_enum_utils_autogen.cpp"
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
        ${no_gl_prototypes}
        LIBANGLE_IMPLEMENTATION
    )
    target_compile_definitions(ANGLE PUBLIC ${ANGLE_DEFINITIONS})
    target_link_libraries(ANGLE PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})

    target_compile_definitions(ANGLEFramework INTERFACE ${gl_prototypes})

    add_library(GLESv2 ${GLESv2_LIBRARY_TYPE}
        ${libglesv2_sources}
    )
    target_include_directories(GLESv2 PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(GLESv2 PRIVATE 
        ${no_gl_prototypes}
        LIBGLESV2_IMPLEMENTATION
    )

    target_link_libraries(GLESv2 PRIVATE ${ANGLEGLESv2_LIBRARIES} ${CMAKE_DL_LIBS})

    if (WIN32)
        # Output library name according to the .def
        target_sources(GLESv2 PRIVATE src/libGLESv2/libGLESv2_autogen.def)
        set_target_properties(GLESv2 PROPERTIES OUTPUT_NAME libGLESv2)
    endif ()

    add_library(GLESv2Framework INTERFACE)
    target_link_libraries(GLESv2Framework INTERFACE GLESv2)
    target_include_directories(GLESv2Framework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR})
    target_compile_definitions(GLESv2Framework INTERFACE USE_SYSTEM_EGL)
    add_library(ANGLE::GLES ALIAS GLESv2Framework)

    if (USE_ANGLE_EGL)
        add_library(EGL ${EGL_LIBRARY_TYPE}
            ${libegl_sources}
        )
        target_include_directories(EGL PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
        target_compile_definitions(EGL PRIVATE
            ${ANGLE_DEFINITIONS}
            ${gl_prototypes}
            EGLAPI=
        )

        target_link_libraries(EGL PRIVATE GLESv2 ${CMAKE_DL_LIBS})

        set_target_properties(EGL PROPERTIES LINKER_LANGUAGE CXX)
        if (WIN32)
            # Output library names according to the .def
            target_sources(EGL PRIVATE src/libEGL/libEGL_autogen.def)
            set_target_properties(EGL PROPERTIES OUTPUT_NAME libEGL)
        endif ()

        add_library(EGLFramework INTERFACE)
        target_include_directories(EGLFramework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR}/)
        target_compile_definitions(EGLFramework INTERFACE ${gl_prototypes})
        target_link_libraries(EGLFramework INTERFACE EGL)
        add_library(ANGLE::EGL ALIAS EGLFramework)
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

if (ENABLE_WEBGL)
    set(libglesv2_entry_points_headers
        src/libGLESv2/entry_points_egl_autogen.h
        src/libGLESv2/entry_points_egl_ext_autogen.h
        src/libGLESv2/entry_points_gles_2_0_autogen.h
        src/libGLESv2/entry_points_gles_3_0_autogen.h
        src/libGLESv2/entry_points_gles_ext_autogen.h
    )

    WEBKIT_COPY_FILES(LibGLESv2EntryPointsHeaders
        DESTINATION ${ANGLE_FRAMEWORK_HEADERS_DIR}/ANGLE
        FILES ${libglesv2_entry_points_headers}
        FLATTENED
    )
    if (WIN32 AND TARGET GLESv2)
        # GLESv2 needs to have a direct or indirect dependency to
        # LibGLESv2EntryPointsHeaders for CMake Visual Studio generator
        # to eliminate duplicated custom commands. Otherwise,
        # entry_points_*.h will be copied twice in both projects.
        add_dependencies(GLESv2 LibGLESv2EntryPointsHeaders)
    endif ()

    add_custom_target(ANGLE-webgl-headers
        DEPENDS LibGLESv2EntryPointsHeaders ANGLEWebGLHeaders
        COMMAND ${CMAKE_COMMAND} -E env
            BUILT_PRODUCTS_DIR=${ANGLE_FRAMEWORK_HEADERS_DIR}
            PUBLIC_HEADERS_FOLDER_PATH=/ANGLE
            ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/adjust-angle-include-paths.py
        VERBATIM
    )
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

target_link_libraries(ANGLEFramework INTERFACE ANGLE)
target_include_directories(ANGLEFramework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR})

add_library(ANGLE::ANGLE ALIAS ANGLEFramework)

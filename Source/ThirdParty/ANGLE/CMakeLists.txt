include(Compiler.cmake)
include(GLESv2.cmake)

if (WIN32)
    # FIXME: DX11 support will not compile if this preprocessor definition is set
    # DirectX Documentation is setting that version to 0x700 but there is no
    # corresponding value in sdkddkver.h
    remove_definitions(-D_WIN32_WINNT=0x601 -DWINVER=0x601)
endif ()

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
)

set(ANGLE_DEFINITIONS
    ANGLE_ENABLE_ESSL
)

set(ANGLE_SOURCES
    ${libangle_common_sources}
    ${angle_preprocessor_sources}
    ${angle_translator_essl_sources}
    ${angle_translator_glsl_sources}
    ${angle_translator_sources}
)

set(ANGLEGLESv2_LIBRARIES
    ANGLE
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

add_subdirectory(include)

add_library(ANGLEFramework INTERFACE)
add_dependencies(ANGLEFramework GLSLANGHeaders ANGLEHeaders)

if (USE_ANGLE_EGL)
    add_library(ANGLE STATIC
        ${ANGLE_SOURCES}
        ${libangle_sources}
        ${libangle_image_util_sources}
        ${xxhash_sources}
    )
    target_include_directories(ANGLE PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(ANGLE PRIVATE
        ${no_gl_prototypes}
        LIBANGLE_IMPLEMENTATION
    )
    target_compile_definitions(ANGLE PUBLIC ${ANGLE_DEFINITIONS})

    add_library(GLESv2 SHARED ${libglesv2_sources})
    target_include_directories(GLESv2 PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(GLESv2 PRIVATE LIBGLESV2_IMPLEMENTATION)
    target_link_libraries(GLESv2 ${ANGLEGLESv2_LIBRARIES})

    add_library(EGL SHARED ${libegl_sources})
    target_include_directories(EGL PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(EGL PRIVATE
        ${gl_prototypes}
        EGLAPI=
    )
    target_link_libraries(EGL GLESv2)

    if (WIN32)
        target_sources(GLESv2 PRIVATE src/libGLESv2/libGLESv2_autogen.def)
        target_sources(EGL PRIVATE src/libEGL/libEGL.def)

        # Output library names according to the .def
        set_target_properties(GLESv2 PROPERTIES OUTPUT_NAME libGLESv2)
        set_target_properties(EGL PROPERTIES OUTPUT_NAME libEGL)
    endif ()

    target_link_libraries(ANGLEFramework INTERFACE EGL GLESv2)
    target_compile_definitions(ANGLEFramework INTERFACE ${gl_prototypes})
else ()
    add_library(ANGLE STATIC ${ANGLE_SOURCES})
    target_include_directories(ANGLE PRIVATE ${ANGLE_PRIVATE_INCLUDE_DIRECTORIES})
    target_compile_definitions(ANGLE PRIVATE
        ${ANGLE_DEFINITIONS}
        ${no_gl_prototypes}
        LIBANGLE_IMPLEMENTATION
    )
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(ANGLE -Wno-cast-align
                                      -Wno-suggest-attribute=format
                                      -Wno-type-limits
                                      -Wno-undef
                                      -Wno-unused-parameter)
endif ()

target_link_libraries(ANGLEFramework INTERFACE ANGLE)
target_include_directories(ANGLEFramework INTERFACE ${ANGLE_FRAMEWORK_HEADERS_DIR})

set_property(DIRECTORY . PROPERTY FOLDER "ANGLE")

set(egl_headers
    EGL/egl.h
    EGL/eglext.h
    EGL/eglext_angle.h
    EGL/eglplatform.h
)

set(gles_headers
    GLES/egl.h
    GLES/gl.h
    GLES/glext.h
    GLES/glext_explicit_context_autogen.inc
    GLES/glext_angle.h
    GLES/glplatform.h
)

set(gles2_headers
    GLES2/gl2.h
    GLES2/gl2ext.h
    GLES2/gl2ext_explicit_context_autogen.inc
    GLES2/gl2ext_angle.h
    GLES2/gl2platform.h
)

set(gles3_headers
    GLES3/gl3.h
    GLES3/gl3ext_explicit_context_autogen.inc
    GLES3/gl31.h
    GLES3/gl31ext_explicit_context_autogen.inc
    GLES3/gl32.h
    GLES3/gl3platform.h
)

set(glslang_headers
    GLSLANG/ShaderLang.h
    GLSLANG/ShaderVars.h
)

set(khr_headers KHR/khrplatform.h)

set(ANGLE_PUBLIC_HEADERS ${khr_headers})

if (USE_ANGLE_EGL)
    list(APPEND ANGLE_PUBLIC_HEADERS
        ${egl_headers}
        ${gles_headers}
        ${gles2_headers}
        ${gles3_headers}
    )
endif ()

# Cocoa expects headers in an ANGLE directory
WEBKIT_COPY_FILES(GLSLANGHeaders
    DESTINATION ${ANGLE_FRAMEWORK_HEADERS_DIR}/ANGLE
    FILES ${glslang_headers}
    FLATTENED
)

WEBKIT_COPY_FILES(ANGLEHeaders
    DESTINATION ${ANGLE_FRAMEWORK_HEADERS_DIR}
    FILES ${ANGLE_PUBLIC_HEADERS}
)

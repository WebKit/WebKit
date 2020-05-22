//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WebGLCompatibilityTest.cpp : Tests of the GL_ANGLE_webgl_compatibility extension.

#include "test_utils/ANGLETest.h"

#include "common/mathutil.h"
#include "test_utils/gl_raii.h"

namespace
{

bool ConstantColorAndAlphaBlendFunctions(GLenum first, GLenum second)
{
    return (first == GL_CONSTANT_COLOR || first == GL_ONE_MINUS_CONSTANT_COLOR) &&
           (second == GL_CONSTANT_ALPHA || second == GL_ONE_MINUS_CONSTANT_ALPHA);
}

void CheckBlendFunctions(GLenum src, GLenum dst)
{
    if (ConstantColorAndAlphaBlendFunctions(src, dst) ||
        ConstantColorAndAlphaBlendFunctions(dst, src))
    {
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        ASSERT_GL_NO_ERROR();
    }
}

// Extensions that affect the ability to use floating point textures
constexpr const char *FloatingPointTextureExtensions[] = {
    "",
    "GL_EXT_texture_storage",
    "GL_OES_texture_half_float",
    "GL_OES_texture_half_float_linear",
    "GL_EXT_color_buffer_half_float",
    "GL_OES_texture_float",
    "GL_OES_texture_float_linear",
    "GL_EXT_color_buffer_float",
    "GL_EXT_float_blend",
    "GL_CHROMIUM_color_buffer_float_rgba",
    "GL_CHROMIUM_color_buffer_float_rgb",
};

}  // namespace

namespace angle
{

class WebGLCompatibilityTest : public ANGLETest
{
  protected:
    WebGLCompatibilityTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setWebGLCompatibilityEnabled(true);
    }

    template <typename T>
    void TestFloatTextureFormat(GLenum internalFormat,
                                GLenum format,
                                GLenum type,
                                bool texturingEnabled,
                                bool linearSamplingEnabled,
                                bool renderingEnabled,
                                const T textureData[4],
                                const float floatData[4])
    {
        ASSERT_GL_NO_ERROR();

        constexpr char kVS[] =
            R"(attribute vec4 position;
varying vec2 texcoord;
void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    texcoord = (position.xy * 0.5) + 0.5;
})";

        constexpr char kFS[] =
            R"(precision mediump float;
uniform sampler2D tex;
uniform vec4 subtractor;
varying vec2 texcoord;
void main()
{
    vec4 color = texture2D(tex, texcoord);
    if (abs(color.r - subtractor.r) +
        abs(color.g - subtractor.g) +
        abs(color.b - subtractor.b) +
        abs(color.a - subtractor.a) < 8.0)
    {
        gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
    else
    {
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
})";

        ANGLE_GL_PROGRAM(samplingProgram, kVS, kFS);
        glUseProgram(samplingProgram.get());

        // Need RGBA8 renderbuffers for enough precision on the readback
        if (IsGLExtensionRequestable("GL_OES_rgb8_rgba8"))
        {
            glRequestExtensionANGLE("GL_OES_rgb8_rgba8");
        }
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") &&
                           getClientMajorVersion() < 3);
        ASSERT_GL_NO_ERROR();

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo.get());
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo.get());

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture.get());

        if (internalFormat == format)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, format, type, textureData);
        }
        else
        {
            if (getClientMajorVersion() >= 3)
            {
                glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
            }
            else
            {
                ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_texture_storage"));
                glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
            }
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, format, type, textureData);
        }

        if (!texturingEnabled)
        {
            // Depending on the entry point and client version, different errors may be generated
            ASSERT_GLENUM_NE(GL_NO_ERROR, glGetError());

            // Two errors may be generated in the glTexStorage + glTexSubImage case, clear the
            // second error
            glGetError();

            return;
        }
        ASSERT_GL_NO_ERROR();

        glUniform1i(glGetUniformLocation(samplingProgram.get(), "tex"), 0);
        glUniform4fv(glGetUniformLocation(samplingProgram.get(), "subtractor"), 1, floatData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        drawQuad(samplingProgram.get(), "position", 0.5f, 1.0f, true);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        drawQuad(samplingProgram.get(), "position", 0.5f, 1.0f, true);

        if (linearSamplingEnabled)
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(),
                               0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (!renderingEnabled)
        {
            EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                             glCheckFramebufferStatus(GL_FRAMEBUFFER));
            return;
        }

        GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebufferStatus == GL_FRAMEBUFFER_UNSUPPORTED)
        {
            std::cout << "Framebuffer returned GL_FRAMEBUFFER_UNSUPPORTED, this is legal."
                      << std::endl;
            return;
        }
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, framebufferStatus);

        ANGLE_GL_PROGRAM(renderingProgram, essl1_shaders::vs::Simple(),
                         essl1_shaders::fs::UniformColor());
        glUseProgram(renderingProgram.get());

        glUniform4fv(glGetUniformLocation(renderingProgram.get(), essl1_shaders::ColorUniform()), 1,
                     floatData);

        drawQuad(renderingProgram.get(), essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

        EXPECT_PIXEL_COLOR32F_NEAR(
            0, 0, GLColor32F(floatData[0], floatData[1], floatData[2], floatData[3]), 1.0f);
    }

    void TestExtFloatBlend(GLenum internalFormat, GLenum type, bool shouldBlend)
    {
        constexpr char kVS[] =
            R"(void main()
{
    gl_PointSize = 1.0;
    gl_Position = vec4(0, 0, 0, 1);
})";

        constexpr char kFS[] =
            R"(void main()
{
    gl_FragColor = vec4(0.5, 0, 0, 0);
})";

        ANGLE_GL_PROGRAM(program, kVS, kFS);
        glUseProgram(program);

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, GL_RGBA, type, nullptr);
        EXPECT_GL_NO_ERROR();

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        ASSERT_EGLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_COLOR32F_NEAR(0, 0, GLColor32F(1, 0, 1, 1), 0.001f);

        glDisable(GL_BLEND);
        glDrawArrays(GL_POINTS, 0, 1);
        EXPECT_GL_NO_ERROR();

        glEnable(GL_BLEND);
        glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
        glBlendColor(10, 1, 1, 1);
        glViewport(0, 0, 1, 1);
        glDrawArrays(GL_POINTS, 0, 1);
        if (!shouldBlend)
        {
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
            return;
        }
        EXPECT_GL_NO_ERROR();

        if (!IsOpenGLES())
        {
            // GLES test machines will need a workaround.
            EXPECT_PIXEL_COLOR32F_NEAR(0, 0, GLColor32F(5, 0, 0, 0), 0.001f);
        }

        // Check sure that non-float attachments clamp BLEND_COLOR.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glDrawArrays(GL_POINTS, 0, 1);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0x80, 0, 0, 0), 1);
    }

    void TestDifferentStencilMaskAndRef(GLenum errIfMismatch);

    // Called from RenderingFeedbackLoopWithDrawBuffersEXT.
    void drawBuffersEXTFeedbackLoop(GLuint program,
                                    const std::array<GLenum, 2> &drawBuffers,
                                    GLenum expectedError);

    // Called from RenderingFeedbackLoopWithDrawBuffers.
    void drawBuffersFeedbackLoop(GLuint program,
                                 const std::array<GLenum, 2> &drawBuffers,
                                 GLenum expectedError);

    // Called from Enable[Compressed]TextureFormatExtensions
    void validateTexImageExtensionFormat(GLenum format, const std::string &extName);
    void validateCompressedTexImageExtensionFormat(GLenum format,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei blockSize,
                                                   const std::string &extName,
                                                   bool subImageAllowed);
};

class WebGL2CompatibilityTest : public WebGLCompatibilityTest
{};

// Context creation would fail if EGL_ANGLE_create_context_webgl_compatibility was not available so
// the GL extension should always be present
TEST_P(WebGLCompatibilityTest, ExtensionStringExposed)
{
    EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_webgl_compatibility"));
}

// Verify that all extension entry points are available
TEST_P(WebGLCompatibilityTest, EntryPoints)
{
    if (IsGLExtensionEnabled("GL_ANGLE_request_extension"))
    {
        EXPECT_NE(nullptr, eglGetProcAddress("glRequestExtensionANGLE"));
    }
}

// WebGL 1 allows GL_DEPTH_STENCIL_ATTACHMENT as a valid binding point.  Make sure it is usable,
// even in ES2 contexts.
TEST_P(WebGLCompatibilityTest, DepthStencilBindingPoint)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer.get());

    EXPECT_GL_NO_ERROR();
}

// Test that attempting to enable an extension that doesn't exist generates GL_INVALID_OPERATION
TEST_P(WebGLCompatibilityTest, EnableExtensionValidation)
{
    glRequestExtensionANGLE("invalid_extension_string");
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test enabling the GL_OES_element_index_uint extension
TEST_P(WebGLCompatibilityTest, EnableExtensionUintIndices)
{
    if (getClientMajorVersion() != 2)
    {
        // This test only works on ES2 where uint indices are not available by default
        return;
    }

    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_element_index_uint"));

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());

    GLuint data[] = {0, 1, 2, 1, 3, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    ANGLE_GL_PROGRAM(program, "void main() { gl_Position = vec4(0, 0, 0, 1); }",
                     "void main() { gl_FragColor = vec4(0, 1, 0, 1); }");
    glUseProgram(program.get());

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_OES_element_index_uint"))
    {
        glRequestExtensionANGLE("GL_OES_element_index_uint");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_element_index_uint"));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_OES_standard_derivatives extension
TEST_P(WebGLCompatibilityTest, EnableExtensionStandardDerivitives)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_standard_derivatives"));

    constexpr char kFS[] =
        R"(#extension GL_OES_standard_derivatives : require
void main() { gl_FragColor = vec4(dFdx(vec2(1.0, 1.0)).x, 1, 0, 1); })";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFS));

    if (IsGLExtensionRequestable("GL_OES_standard_derivatives"))
    {
        glRequestExtensionANGLE("GL_OES_standard_derivatives");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_standard_derivatives"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFS);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_shader_texture_lod extension
TEST_P(WebGLCompatibilityTest, EnableExtensionTextureLOD)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_shader_texture_lod"));

    constexpr char kFS[] =
        R"(#extension GL_EXT_shader_texture_lod : require
uniform sampler2D u_texture;
void main() {
    gl_FragColor = texture2DGradEXT(u_texture, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0,
0.0));
})";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFS));

    if (IsGLExtensionRequestable("GL_EXT_shader_texture_lod"))
    {
        glRequestExtensionANGLE("GL_EXT_shader_texture_lod");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_shader_texture_lod"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFS);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_frag_depth extension
TEST_P(WebGLCompatibilityTest, EnableExtensionFragDepth)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_frag_depth"));

    constexpr char kFS[] =
        R"(#extension GL_EXT_frag_depth : require
void main() {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    gl_FragDepthEXT = 1.0;
})";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFS));

    if (IsGLExtensionRequestable("GL_EXT_frag_depth"))
    {
        glRequestExtensionANGLE("GL_EXT_frag_depth");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_frag_depth"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, kFS);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_texture_filter_anisotropic extension
TEST_P(WebGLCompatibilityTest, EnableExtensionTextureFilterAnisotropic)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_texture_filter_anisotropic"));

    GLfloat maxAnisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    ASSERT_GL_NO_ERROR();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLfloat currentAnisotropy = 0.0f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &currentAnisotropy);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_EXT_texture_filter_anisotropic"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_filter_anisotropic");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_texture_filter_anisotropic"));

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        ASSERT_GL_NO_ERROR();
        EXPECT_GE(maxAnisotropy, 2.0f);

        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &currentAnisotropy);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(1.0f, currentAnisotropy);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
        ASSERT_GL_NO_ERROR();
    }
}

// Test enabling the EGL image extensions
TEST_P(WebGLCompatibilityTest, EnableExtensionEGLImage)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_EGL_image"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_EGL_image_external"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_EGL_image_external_essl3"));
    EXPECT_FALSE(IsGLExtensionEnabled("NV_EGL_stream_consumer_external"));

    constexpr char kFSES2[] =
        R"(#extension GL_OES_EGL_image_external : require
precision highp float;
uniform samplerExternalOES sampler;
void main()
{
    gl_FragColor = texture2D(sampler, vec2(0, 0));
})";
    EXPECT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFSES2));

    constexpr char kFSES3[] =
        R"(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;
uniform samplerExternalOES sampler;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(sampler, vec2(0, 0));
})";
    if (getClientMajorVersion() >= 3)
    {
        EXPECT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFSES3));
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint result;
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_OES_EGL_image_external"))
    {
        glRequestExtensionANGLE("GL_OES_EGL_image_external");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_EGL_image_external"));

        EXPECT_NE(0u, CompileShader(GL_FRAGMENT_SHADER, kFSES2));

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        EXPECT_GL_NO_ERROR();

        glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &result);
        EXPECT_GL_NO_ERROR();

        if (getClientMajorVersion() >= 3 &&
            IsGLExtensionRequestable("GL_OES_EGL_image_external_essl3"))
        {
            glRequestExtensionANGLE("GL_OES_EGL_image_external_essl3");
            EXPECT_GL_NO_ERROR();
            EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_EGL_image_external_essl3"));

            EXPECT_NE(0u, CompileShader(GL_FRAGMENT_SHADER, kFSES3));
        }
        else
        {
            EXPECT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, kFSES3));
        }
    }
}

// Verify that shaders are of a compatible spec when the extension is enabled.
TEST_P(WebGLCompatibilityTest, ExtensionCompilerSpec)
{
    EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_webgl_compatibility"));

    // Use of reserved _webgl prefix should fail when the shader specification is for WebGL.
    constexpr char kVS[] =
        R"(struct Foo {
    int _webgl_bar;
};
void main()
{
    Foo foo = Foo(1);
})";

    // Default fragement shader.
    constexpr char kFS[] =
        R"(void main()
{
    gl_FragColor = vec4(1.0,0.0,0.0,1.0);
})";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
    glDeleteProgram(program);
}

// Test enabling the GL_NV_pixel_buffer_object extension
TEST_P(WebGLCompatibilityTest, EnablePixelBufferObjectExtensions)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_NV_pixel_buffer_object"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_mapbuffer"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_map_buffer_range"));

    // These extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLBuffer buffer;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_NV_pixel_buffer_object"))
    {
        glRequestExtensionANGLE("GL_NV_pixel_buffer_object");
        EXPECT_GL_NO_ERROR();

        // Create a framebuffer to read from
        GLRenderbuffer renderbuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderbuffer);
        EXPECT_GL_NO_ERROR();

        glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
        EXPECT_GL_NO_ERROR();

        glBufferData(GL_PIXEL_PACK_BUFFER, 4, nullptr, GL_STATIC_DRAW);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_EXT_texture_storage extension
TEST_P(WebGLCompatibilityTest, EnableTextureStorage)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_texture_storage"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    GLint result;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &result);
    if (getClientMajorVersion() >= 3)
    {
        EXPECT_GL_NO_ERROR();
    }
    else
    {
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }

    if (IsGLExtensionRequestable("GL_EXT_texture_storage"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_storage");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_texture_storage"));

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &result);
        EXPECT_GL_NO_ERROR();

        const GLenum alwaysAcceptableFormats[] = {
            GL_ALPHA8_EXT,
            GL_LUMINANCE8_EXT,
            GL_LUMINANCE8_ALPHA8_EXT,
        };
        for (const auto &acceptableFormat : alwaysAcceptableFormats)
        {
            GLTexture localTexture;
            glBindTexture(GL_TEXTURE_2D, localTexture);
            glTexStorage2DEXT(GL_TEXTURE_2D, 1, acceptableFormat, 1, 1);
            EXPECT_GL_NO_ERROR();
        }
    }
}

// Test enabling the GL_OES_mapbuffer and GL_EXT_map_buffer_range extensions
TEST_P(WebGLCompatibilityTest, EnableMapBufferExtensions)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_mapbuffer"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_map_buffer_range"));

    // These extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLBuffer buffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4, nullptr, GL_STATIC_DRAW);

    glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glMapBufferRangeEXT(GL_ELEMENT_ARRAY_BUFFER, 0, 4, GL_MAP_WRITE_BIT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    GLint access = 0;
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_ACCESS_OES, &access);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_OES_mapbuffer"))
    {
        glRequestExtensionANGLE("GL_OES_mapbuffer");
        EXPECT_GL_NO_ERROR();

        glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
        glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_ACCESS_OES, &access);
        EXPECT_GL_NO_ERROR();
    }

    if (IsGLExtensionRequestable("GL_EXT_map_buffer_range"))
    {
        glRequestExtensionANGLE("GL_EXT_map_buffer_range");
        EXPECT_GL_NO_ERROR();

        glMapBufferRangeEXT(GL_ELEMENT_ARRAY_BUFFER, 0, 4, GL_MAP_WRITE_BIT);
        glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
        glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_ACCESS_OES, &access);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_OES_fbo_render_mipmap extension
TEST_P(WebGLCompatibilityTest, EnableRenderMipmapExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_fbo_render_mipmap"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    if (IsGLExtensionRequestable("GL_OES_fbo_render_mipmap"))
    {
        glRequestExtensionANGLE("GL_OES_fbo_render_mipmap");
        EXPECT_GL_NO_ERROR();

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_EXT_blend_minmax extension
TEST_P(WebGLCompatibilityTest, EnableBlendMinMaxExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_blend_minmax"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    glBlendEquation(GL_MIN);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glBlendEquation(GL_MAX);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_EXT_blend_minmax"))
    {
        glRequestExtensionANGLE("GL_EXT_blend_minmax");
        EXPECT_GL_NO_ERROR();

        glBlendEquation(GL_MIN);
        glBlendEquation(GL_MAX);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the query extensions
TEST_P(WebGLCompatibilityTest, EnableQueryExtensions)
{
    // Seems to be causing a device lost. http://anglebug.com/2423
    ANGLE_SKIP_TEST_IF(IsAMD() && IsWindows() && IsOpenGL());

    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_occlusion_query_boolean"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_disjoint_timer_query"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_CHROMIUM_sync_query"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLQueryEXT badQuery;

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, badQuery);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, badQuery);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBeginQueryEXT(GL_TIME_ELAPSED_EXT, badQuery);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glQueryCounterEXT(GL_TIMESTAMP_EXT, badQuery);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, badQuery);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_EXT_occlusion_query_boolean"))
    {
        glRequestExtensionANGLE("GL_EXT_occlusion_query_boolean");
        EXPECT_GL_NO_ERROR();

        GLQueryEXT query;
        glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query);
        glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
        EXPECT_GL_NO_ERROR();
    }

    if (IsGLExtensionRequestable("GL_EXT_disjoint_timer_query"))
    {
        glRequestExtensionANGLE("GL_EXT_disjoint_timer_query");
        EXPECT_GL_NO_ERROR();

        GLQueryEXT query1;
        glBeginQueryEXT(GL_TIME_ELAPSED_EXT, query1);
        glEndQueryEXT(GL_TIME_ELAPSED_EXT);
        EXPECT_GL_NO_ERROR();

        GLQueryEXT query2;
        glQueryCounterEXT(query2, GL_TIMESTAMP_EXT);
        EXPECT_GL_NO_ERROR();
    }

    if (IsGLExtensionRequestable("GL_CHROMIUM_sync_query"))
    {
        glRequestExtensionANGLE("GL_CHROMIUM_sync_query");
        EXPECT_GL_NO_ERROR();

        GLQueryEXT query;
        glBeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query);
        glEndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_ANGLE_framebuffer_multisample extension
TEST_P(WebGLCompatibilityTest, EnableFramebufferMultisampleExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_framebuffer_multisample"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 1, GL_RGBA4, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_ANGLE_framebuffer_multisample"))
    {
        glRequestExtensionANGLE("GL_ANGLE_framebuffer_multisample");
        EXPECT_GL_NO_ERROR();

        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        EXPECT_GL_NO_ERROR();

        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, maxSamples, GL_RGBA4, 1, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_ANGLE_instanced_arrays extension
TEST_P(WebGLCompatibilityTest, EnableInstancedArraysExtensionANGLE)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_instanced_arrays"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLint divisor = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glVertexAttribDivisorANGLE(0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_ANGLE_instanced_arrays"))
    {
        glRequestExtensionANGLE("GL_ANGLE_instanced_arrays");
        EXPECT_GL_NO_ERROR();

        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
        glVertexAttribDivisorANGLE(0, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_EXT_instanced_arrays extension
TEST_P(WebGLCompatibilityTest, EnableInstancedArraysExtensionEXT)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_instanced_arrays"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLint divisor = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glVertexAttribDivisorEXT(0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_EXT_instanced_arrays"))
    {
        glRequestExtensionANGLE("GL_EXT_instanced_arrays");
        EXPECT_GL_NO_ERROR();

        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
        glVertexAttribDivisorEXT(0, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_ANGLE_pack_reverse_row_order extension
TEST_P(WebGLCompatibilityTest, EnablePackReverseRowOrderExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_pack_reverse_row_order"));

    GLint result = 0;
    glGetIntegerv(GL_PACK_REVERSE_ROW_ORDER_ANGLE, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glPixelStorei(GL_PACK_REVERSE_ROW_ORDER_ANGLE, GL_TRUE);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_ANGLE_pack_reverse_row_order"))
    {
        glRequestExtensionANGLE("GL_ANGLE_pack_reverse_row_order");
        EXPECT_GL_NO_ERROR();

        glGetIntegerv(GL_PACK_REVERSE_ROW_ORDER_ANGLE, &result);
        glPixelStorei(GL_PACK_REVERSE_ROW_ORDER_ANGLE, GL_TRUE);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_EXT_unpack_subimage extension
TEST_P(WebGLCompatibilityTest, EnablePackUnpackSubImageExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_EXT_unpack_subimage"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    constexpr GLenum parameters[] = {
        GL_UNPACK_ROW_LENGTH_EXT,
        GL_UNPACK_SKIP_ROWS_EXT,
        GL_UNPACK_SKIP_PIXELS_EXT,
    };

    for (GLenum param : parameters)
    {
        GLint resultI = 0;
        glGetIntegerv(param, &resultI);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        GLfloat resultF = 0.0f;
        glGetFloatv(param, &resultF);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glPixelStorei(param, 0);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }

    if (IsGLExtensionRequestable("GL_EXT_unpack_subimage"))
    {
        glRequestExtensionANGLE("GL_EXT_unpack_subimage");
        EXPECT_GL_NO_ERROR();

        for (GLenum param : parameters)
        {
            GLint resultI = 0;
            glGetIntegerv(param, &resultI);

            GLfloat resultF = 0.0f;
            glGetFloatv(param, &resultF);

            glPixelStorei(param, 0);

            EXPECT_GL_NO_ERROR();
        }
    }
}

TEST_P(WebGLCompatibilityTest, EnableTextureRectangle)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_RECTANGLE_ANGLE, texture);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint minFilter = 0;
    glGetTexParameteriv(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MIN_FILTER, &minFilter);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_ANGLE_texture_rectangle"))
    {
        glRequestExtensionANGLE("GL_ANGLE_texture_rectangle");
        EXPECT_GL_NO_ERROR();

        EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"));

        glBindTexture(GL_TEXTURE_RECTANGLE_ANGLE, texture);
        EXPECT_GL_NO_ERROR();

        glTexImage2D(GL_TEXTURE_RECTANGLE_ANGLE, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        EXPECT_GL_NO_ERROR();

        glDisableExtensionANGLE("GL_ANGLE_texture_rectangle");
        EXPECT_GL_NO_ERROR();

        EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_texture_rectangle"));

        glBindTexture(GL_TEXTURE_RECTANGLE_ANGLE, texture);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
}

// Test enabling the GL_NV_pack_subimage extension
TEST_P(WebGLCompatibilityTest, EnablePackPackSubImageExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_NV_pack_subimage"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    constexpr GLenum parameters[] = {
        GL_PACK_ROW_LENGTH,
        GL_PACK_SKIP_ROWS,
        GL_PACK_SKIP_PIXELS,
    };

    for (GLenum param : parameters)
    {
        GLint resultI = 0;
        glGetIntegerv(param, &resultI);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        GLfloat resultF = 0.0f;
        glGetFloatv(param, &resultF);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glPixelStorei(param, 0);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }

    if (IsGLExtensionRequestable("GL_NV_pack_subimage"))
    {
        glRequestExtensionANGLE("GL_NV_pack_subimage");
        EXPECT_GL_NO_ERROR();

        for (GLenum param : parameters)
        {
            GLint resultI = 0;
            glGetIntegerv(param, &resultI);

            GLfloat resultF = 0.0f;
            glGetFloatv(param, &resultF);

            glPixelStorei(param, 0);

            EXPECT_GL_NO_ERROR();
        }
    }
}

TEST_P(WebGLCompatibilityTest, EnableRGB8RGBA8Extension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_rgb8_rgba8"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    EXPECT_GL_NO_ERROR();

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8_OES, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_OES_rgb8_rgba8"))
    {
        glRequestExtensionANGLE("GL_OES_rgb8_rgba8");
        EXPECT_GL_NO_ERROR();

        EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_rgb8_rgba8"));

        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8_OES, 1, 1);
        EXPECT_GL_NO_ERROR();

        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, 1, 1);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_ANGLE_framebuffer_blit extension
TEST_P(WebGLCompatibilityTest, EnableFramebufferBlitExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLFramebuffer fbo;

    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, fbo);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLint result;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_ANGLE, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glBlitFramebufferANGLE(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_ANGLE_framebuffer_blit"))
    {
        glRequestExtensionANGLE("GL_ANGLE_framebuffer_blit");
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, fbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_ANGLE, &result);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_OES_get_program_binary extension
TEST_P(WebGLCompatibilityTest, EnableProgramBinaryExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_get_program_binary"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLint result = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    constexpr char kVS[] =
        R"(void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
})";
    constexpr char kFS[] =
        R"(precision highp float;
void main()
{
    gl_FragColor = vec4(1.0);
})";
    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    uint8_t tempArray[512];
    GLenum tempFormat  = 0;
    GLsizei tempLength = 0;
    glGetProgramBinaryOES(program, static_cast<GLsizei>(ArraySize(tempArray)), &tempLength,
                          &tempFormat, tempArray);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_OES_get_program_binary"))
    {
        glRequestExtensionANGLE("GL_OES_get_program_binary");
        EXPECT_GL_NO_ERROR();

        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &result);
        glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, &result);
        EXPECT_GL_NO_ERROR();

        GLint binaryLength = 0;
        glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
        EXPECT_GL_NO_ERROR();

        GLenum binaryFormat;
        GLsizei writeLength = 0;
        std::vector<uint8_t> binary(binaryLength);
        glGetProgramBinaryOES(program, binaryLength, &writeLength, &binaryFormat, binary.data());
        EXPECT_GL_NO_ERROR();

        glProgramBinaryOES(program, binaryFormat, binary.data(), binaryLength);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_OES_vertex_array_object extension
TEST_P(WebGLCompatibilityTest, EnableVertexArrayExtension)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_vertex_array_object"));

    // This extensions become core in in ES3/WebGL2.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    GLint result = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Expect that GL_OES_vertex_array_object is always available.  It is implemented in the GL
    // frontend.
    EXPECT_TRUE(IsGLExtensionRequestable("GL_OES_vertex_array_object"));

    glRequestExtensionANGLE("GL_OES_vertex_array_object");
    EXPECT_GL_NO_ERROR();

    EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_vertex_array_object"));

    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &result);
    EXPECT_GL_NO_ERROR();

    GLuint vao = 0;
    glGenVertexArraysOES(0, &vao);
    EXPECT_GL_NO_ERROR();

    glBindVertexArrayOES(vao);
    EXPECT_GL_NO_ERROR();

    glDeleteVertexArraysOES(1, &vao);
    EXPECT_GL_NO_ERROR();
}

// Verify that the context generates the correct error when the framebuffer attachments are
// different sizes
TEST_P(WebGLCompatibilityTest, FramebufferAttachmentSizeMismatch)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 3, 3);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    if (IsGLExtensionRequestable("GL_EXT_draw_buffers"))
    {
        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_draw_buffers"));

        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[1], 0);
        ASSERT_GL_NO_ERROR();

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

// Test that client-side array buffers are forbidden in WebGL mode
TEST_P(WebGLCompatibilityTest, ForbidsClientSideArrayBuffer)
{
    constexpr char kVS[] =
        R"(attribute vec3 a_pos;
void main()
{
    gl_Position = vec4(a_pos, 1.0);
})";

    constexpr char kFS[] =
        R"(precision highp float;
void main()
{
    gl_FragColor = vec4(1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 4, vertices.data());
    glEnableVertexAttribArray(posLocation);

    ASSERT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that client-side element array buffers are forbidden in WebGL mode
TEST_P(WebGLCompatibilityTest, ForbidsClientSideElementBuffer)
{
    constexpr char kVS[] =
        R"(attribute vec3 a_pos;
void main()
{
    gl_Position = vec4(a_pos, 1.0);
})";

    constexpr char kFS[] =
        R"(precision highp float;
void main()
{
    gl_FragColor = vec4(1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    ASSERT_GL_NO_ERROR();

    // Use the pointer with value of 1 for indices instead of an actual pointer because WebGL also
    // enforces that the top bit of indices must be 0 (i.e. offset >= 0) and would generate
    // GL_INVALID_VALUE in that case. Using a null pointer gets caught by another check.
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void *>(intptr_t(1)));
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that client-side array buffers are forbidden even if the program doesn't use the attribute
TEST_P(WebGLCompatibilityTest, ForbidsClientSideArrayBufferEvenNotUsedOnes)
{
    constexpr char kVS[] =
        R"(void main()
{
    gl_Position = vec4(1.0);
})";

    constexpr char kFS[] =
        R"(precision highp float;
void main()
{
    gl_FragColor = vec4(1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4, vertices.data());
    glEnableVertexAttribArray(0);

    ASSERT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that passing a null pixel data pointer to TexSubImage calls generates an INVALID_VALUE error
TEST_P(WebGLCompatibilityTest, NullPixelDataForSubImage)
{
    // glTexSubImage2D
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);

        // TexImage with null data - OK
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();

        // TexSubImage with zero size and null data - OK
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();

        // TexSubImage with non-zero size and null data - Invalid value
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // glTexSubImage3D
    if (getClientMajorVersion() >= 3)
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_3D, texture);

        // TexImage with null data - OK
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();

        // TexSubImage with zero size and null data - OK
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_NO_ERROR();

        // TexSubImage with non-zero size and null data - Invalid value
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }
}

// Tests the WebGL requirement of having the same stencil mask, writemask and ref for front and back
// (when stencil testing is enabled)
void WebGLCompatibilityTest::TestDifferentStencilMaskAndRef(GLenum errIfMismatch)
{
    // Run the test in an FBO to make sure we have some stencil bits.
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer.get());

    ANGLE_GL_PROGRAM(program, "void main() { gl_Position = vec4(0, 0, 0, 1); }",
                     "void main() { gl_FragColor = vec4(0, 1, 0, 1); }");
    glUseProgram(program.get());
    ASSERT_GL_NO_ERROR();

    // Having ref and mask the same for front and back is valid.
    glStencilMask(255);
    glStencilFunc(GL_ALWAYS, 0, 255);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different front - back write mask generates an error.
    glStencilMaskSeparate(GL_FRONT, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(errIfMismatch);

    // Setting both write masks separately to the same value is valid.
    glStencilMaskSeparate(GL_BACK, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different stencil front - back mask generates an error
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(errIfMismatch);

    // Setting both masks separately to the same value is valid.
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different stencil front - back reference generates an error
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(errIfMismatch);

    // Setting both references separately to the same value is valid.
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Using different stencil funcs, everything being equal is valid.
    glStencilFuncSeparate(GL_BACK, GL_NEVER, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
}

TEST_P(WebGLCompatibilityTest, StencilTestEnabledDisallowsDifferentStencilMaskAndRef)
{
    glEnable(GL_STENCIL_TEST);
    TestDifferentStencilMaskAndRef(GL_INVALID_OPERATION);
}

TEST_P(WebGLCompatibilityTest, StencilTestDisabledAllowsDifferentStencilMaskAndRef)
{
    glDisable(GL_STENCIL_TEST);
    TestDifferentStencilMaskAndRef(GL_NO_ERROR);
}

// Test that GL_FIXED is forbidden
TEST_P(WebGLCompatibilityTest, ForbidsGLFixed)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    ASSERT_GL_NO_ERROR();

    glVertexAttribPointer(0, 1, GL_FIXED, GL_FALSE, 0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test the WebGL limit of 255 for the attribute stride
TEST_P(WebGLCompatibilityTest, MaxStride)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 255, nullptr);
    ASSERT_GL_NO_ERROR();

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 256, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test the checks for OOB reads in the vertex buffers, non-instanced version
TEST_P(WebGLCompatibilityTest, DrawArraysBufferOutOfBoundsNonInstanced)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);

    const uint8_t *zeroOffset = nullptr;

    // Test touching the last element is valid.
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 12);
    glDrawArrays(GL_POINTS, 0, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid.
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 13);
    glDrawArrays(GL_POINTS, 0, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test touching the last element is valid, using a stride.
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 9);
    glDrawArrays(GL_POINTS, 0, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid, using a stride.
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 10);
    glDrawArrays(GL_POINTS, 0, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset is valid if no vertices are drawn.
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArrays(GL_POINTS, 0, 0);
    ASSERT_GL_NO_ERROR();

    // Test a case of overflow that could give a max vertex that's negative
    constexpr GLint kIntMax = std::numeric_limits<GLint>::max();
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 0);
    glDrawArrays(GL_POINTS, kIntMax, kIntMax);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that index values outside of the 32-bit integer range do not read out of bounds
TEST_P(WebGLCompatibilityTest, LargeIndexRange)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_element_index_uint"));

    constexpr char kVS[] =
        R"(attribute vec4 a_Position;
void main()
{
    gl_Position = a_Position;
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    glUseProgram(program.get());

    glEnableVertexAttribArray(glGetAttribLocation(program, "a_Position"));

    constexpr float kVertexData[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertexData), kVertexData, GL_STREAM_DRAW);

    constexpr GLuint kMaxIntAsGLuint = static_cast<GLuint>(std::numeric_limits<GLint>::max());
    constexpr GLuint kIndexData[]    = {
        kMaxIntAsGLuint,
        kMaxIntAsGLuint + 1,
        kMaxIntAsGLuint + 2,
        kMaxIntAsGLuint + 3,
    };

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndexData), kIndexData, GL_DYNAMIC_DRAW);

    EXPECT_GL_NO_ERROR();

    // First index is representable as 32-bit int but second is not
    glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Neither index is representable as 32-bit int
    glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, reinterpret_cast<void *>(sizeof(GLuint) * 2));
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test for drawing with a null index buffer
TEST_P(WebGLCompatibilityTest, NullIndexBuffer)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    glUseProgram(program.get());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);

    glDrawElements(GL_TRIANGLES, 0, GL_UNSIGNED_BYTE, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test the checks for OOB reads in the vertex buffers, instanced version
TEST_P(WebGL2CompatibilityTest, DrawArraysBufferOutOfBoundsInstanced)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
attribute float a_w;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, a_w);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    GLint wLocation   = glGetAttribLocation(program.get(), "a_w");
    ASSERT_NE(-1, posLocation);
    ASSERT_NE(-1, wLocation);
    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
    glVertexAttribDivisor(posLocation, 0);

    glEnableVertexAttribArray(wLocation);
    glVertexAttribDivisor(wLocation, 1);

    const uint8_t *zeroOffset = nullptr;

    // Test touching the last element is valid.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 12);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 13);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test touching the last element is valid, using a stride.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 9);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid, using a stride.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 10);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset is valid if no vertices are drawn.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArraysInstanced(GL_POINTS, 0, 0, 1);
    ASSERT_GL_NO_ERROR();

    // Test any offset is valid if no primitives are drawn.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 0);
    ASSERT_GL_NO_ERROR();
}

// Test the checks for OOB reads in the vertex buffers, ANGLE_instanced_arrays version
TEST_P(WebGLCompatibilityTest, DrawArraysBufferOutOfBoundsInstancedANGLE)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionRequestable("GL_ANGLE_instanced_arrays"));
    glRequestExtensionANGLE("GL_ANGLE_instanced_arrays");
    EXPECT_GL_NO_ERROR();

    constexpr char kVS[] =
        R"(attribute float a_pos;
attribute float a_w;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, a_w);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    GLint wLocation   = glGetAttribLocation(program.get(), "a_w");
    ASSERT_NE(-1, posLocation);
    ASSERT_NE(-1, wLocation);
    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
    glVertexAttribDivisorANGLE(posLocation, 0);

    glEnableVertexAttribArray(wLocation);
    glVertexAttribDivisorANGLE(wLocation, 1);

    const uint8_t *zeroOffset = nullptr;

    // Test touching the last element is valid.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 12);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR() << "touching the last element.";

    // Test touching the last element + 1 is invalid.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 13);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "touching the last element + 1.";

    // Test touching the last element is valid, using a stride.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 9);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR() << "touching the last element using a stride.";

    // Test touching the last element + 1 is invalid, using a stride.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 10);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "touching the last element + 1 using a stride.";

    // Test any offset is valid if no vertices are drawn.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 0, 1);
    ASSERT_GL_NO_ERROR() << "any offset with no vertices.";

    // Test any offset is valid if no primitives are drawn.
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArraysInstancedANGLE(GL_POINTS, 0, 1, 0);
    ASSERT_GL_NO_ERROR() << "any offset with primitives.";
}

// Test the checks for OOB reads in the index buffer
TEST_P(WebGLCompatibilityTest, DrawElementsBufferOutOfBoundsInIndexBuffer)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, nullptr);

    const uint8_t *zeroOffset   = nullptr;
    const uint8_t zeroIndices[] = {0, 0, 0, 0, 0, 0, 0, 0};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(zeroIndices), zeroIndices, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Test touching the last index is valid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last + 1 element is invalid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 5);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset if valid if count is zero
    glDrawElements(GL_POINTS, 0, GL_UNSIGNED_BYTE, zeroOffset + 42);
    ASSERT_GL_NO_ERROR();

    // Test touching the first index is valid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the first - 1 index is invalid
    // The error ha been specified to be INVALID_VALUE instead of INVALID_OPERATION because it was
    // the historic behavior of WebGL implementations
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset - 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test the checks for OOB in vertex buffers caused by indices, non-instanced version
TEST_P(WebGLCompatibilityTest, DrawElementsBufferOutOfBoundsInVertexBuffer)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());
    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, 8, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribPointer(posLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, nullptr);

    const uint8_t *zeroOffset   = nullptr;
    const uint8_t testIndices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 255};

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(testIndices), testIndices, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Test touching the end of the vertex buffer is valid
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, zeroOffset + 7);
    ASSERT_GL_NO_ERROR();

    // Test touching just after the end of the vertex buffer is invalid
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, zeroOffset + 8);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test touching the whole vertex buffer is valid
    glDrawElements(GL_POINTS, 8, GL_UNSIGNED_BYTE, zeroOffset + 0);
    ASSERT_GL_NO_ERROR();

    // Test an index that would be negative
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, zeroOffset + 9);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test depth range with 'near' more or less than 'far.'
TEST_P(WebGLCompatibilityTest, DepthRange)
{
    glDepthRangef(0, 1);
    ASSERT_GL_NO_ERROR();

    glDepthRangef(.5, .5);
    ASSERT_GL_NO_ERROR();

    glDepthRangef(1, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test all blend function combinations.
// In WebGL it is invalid to combine constant color with constant alpha.
TEST_P(WebGLCompatibilityTest, BlendWithConstantColor)
{
    constexpr GLenum srcFunc[] = {
        GL_ZERO,
        GL_ONE,
        GL_SRC_COLOR,
        GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR,
        GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_DST_ALPHA,
        GL_ONE_MINUS_DST_ALPHA,
        GL_CONSTANT_COLOR,
        GL_ONE_MINUS_CONSTANT_COLOR,
        GL_CONSTANT_ALPHA,
        GL_ONE_MINUS_CONSTANT_ALPHA,
        GL_SRC_ALPHA_SATURATE,
    };

    constexpr GLenum dstFunc[] = {
        GL_ZERO,           GL_ONE,
        GL_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,
        GL_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,
        GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR,
        GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA,
    };

    for (GLenum src : srcFunc)
    {
        for (GLenum dst : dstFunc)
        {
            glBlendFunc(src, dst);
            CheckBlendFunctions(src, dst);
            glBlendFuncSeparate(src, dst, GL_ONE, GL_ONE);
            CheckBlendFunctions(src, dst);
        }
    }

    // Ensure the same semantics for indexed blendFunc
    if (IsGLExtensionRequestable("GL_OES_draw_buffers_indexed"))
    {
        glRequestExtensionANGLE("GL_OES_draw_buffers_indexed");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"));

        for (GLenum src : srcFunc)
        {
            for (GLenum dst : dstFunc)
            {
                glBlendFunciOES(0, src, dst);
                CheckBlendFunctions(src, dst);
                glBlendFuncSeparateiOES(0, src, dst, GL_ONE, GL_ONE);
                CheckBlendFunctions(src, dst);
            }
        }
    }
}

// Test draw state validation and invalidation wrt indexed blendFunc.
TEST_P(WebGLCompatibilityTest, IndexedBlendWithConstantColorInvalidation)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionRequestable("GL_OES_draw_buffers_indexed"));

    glRequestExtensionANGLE("GL_OES_draw_buffers_indexed");
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"));

    constexpr char kVS[] =
        R"(#version 300 es
void main()
{
    gl_PointSize = 1.0;
    gl_Position = vec4(0, 0, 0, 1);
})";

    constexpr char kFS[] =
        R"(#version 300 es
precision lowp float;
layout(location = 0) out vec4 o_color0;
layout(location = 1) out vec4 o_color1;
void main()
{
    o_color0 = vec4(1, 0, 0, 1);
    o_color1 = vec4(0, 1, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glDisable(GL_BLEND);
    glEnableiOES(GL_BLEND, 0);
    glEnableiOES(GL_BLEND, 1);

    GLTexture texture1;
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture2, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLenum drawbuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawbuffers);

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    // Force-invalidate draw call
    glBlendFuncSeparateiOES(0, GL_CONSTANT_COLOR, GL_CONSTANT_COLOR, GL_CONSTANT_ALPHA,
                            GL_CONSTANT_ALPHA);
    EXPECT_GL_NO_ERROR();

    glBlendFuncSeparateiOES(1, GL_CONSTANT_ALPHA, GL_CONSTANT_ALPHA, GL_CONSTANT_COLOR,
                            GL_CONSTANT_COLOR);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test getIndexedParameter wrt GL_OES_draw_buffers_indexed.
TEST_P(WebGLCompatibilityTest, DrawBuffersIndexedGetIndexedParameter)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionRequestable("GL_OES_draw_buffers_indexed"));

    GLint value;
    GLboolean data[4];

    glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetIntegeri_v(GL_BLEND_SRC_ALPHA, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetIntegeri_v(GL_BLEND_DST_RGB, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetIntegeri_v(GL_BLEND_DST_ALPHA, 0, &value);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 0, data);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glRequestExtensionANGLE("GL_OES_draw_buffers_indexed");
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"));

    glDisable(GL_BLEND);
    glEnableiOES(GL_BLEND, 0);
    glBlendEquationSeparateiOES(0, GL_FUNC_ADD, GL_FUNC_SUBTRACT);
    glBlendFuncSeparateiOES(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ZERO);
    glColorMaskiOES(0, true, false, true, false);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(true, glIsEnablediOES(GL_BLEND, 0));
    EXPECT_GL_NO_ERROR();
    glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_FUNC_ADD, value);
    glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_FUNC_SUBTRACT, value);
    glGetIntegeri_v(GL_BLEND_SRC_RGB, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_SRC_ALPHA, value);
    glGetIntegeri_v(GL_BLEND_SRC_ALPHA, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_ZERO, value);
    glGetIntegeri_v(GL_BLEND_DST_RGB, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_ONE_MINUS_SRC_ALPHA, value);
    glGetIntegeri_v(GL_BLEND_DST_ALPHA, 0, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_ZERO, value);
    glGetBooleani_v(GL_COLOR_WRITEMASK, 0, data);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(true, data[0]);
    EXPECT_EQ(false, data[1]);
    EXPECT_EQ(true, data[2]);
    EXPECT_EQ(false, data[3]);
}

// Test that binding/querying uniforms and attributes with invalid names generates errors
TEST_P(WebGLCompatibilityTest, InvalidAttributeAndUniformNames)
{
    const std::string validAttribName =
        "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    const std::string validUniformName =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890";
    std::vector<char> invalidSet = {'"', '$', '`', '@', '\''};
    if (getClientMajorVersion() < 3)
    {
        invalidSet.push_back('\\');
    }

    std::string vert = "attribute float ";
    vert += validAttribName;
    vert +=
        R"(;
void main()
{
    gl_Position = vec4(1.0);
})";

    std::string frag =
        R"(precision highp float;
uniform vec4 )";
    frag += validUniformName;
    // Insert illegal characters into comments
    frag +=
        R"(;
    // $ \" @ /*
void main()
{/*
    ` @ $
    */gl_FragColor = vec4(1.0);
})";

    ANGLE_GL_PROGRAM(program, vert.c_str(), frag.c_str());
    EXPECT_GL_NO_ERROR();

    for (char invalidChar : invalidSet)
    {
        std::string invalidName = validAttribName + invalidChar;
        glGetAttribLocation(program, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glGetAttribLocation unexpectedly succeeded for name \"" << invalidName << "\".";

        glBindAttribLocation(program, 0, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glBindAttribLocation unexpectedly succeeded for name \"" << invalidName << "\".";
    }

    for (char invalidChar : invalidSet)
    {
        std::string invalidName = validUniformName + invalidChar;
        glGetUniformLocation(program, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glGetUniformLocation unexpectedly succeeded for name \"" << invalidName << "\".";
    }

    for (char invalidChar : invalidSet)
    {
        std::string invalidAttribName = validAttribName + invalidChar;
        const char *invalidVert[]     = {
            "attribute float ",
            invalidAttribName.c_str(),
            R"(;,
void main(),
{,
    gl_Position = vec4(1.0);,
})",
        };

        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shader, static_cast<GLsizei>(ArraySize(invalidVert)), invalidVert, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        glDeleteShader(shader);
    }
}

// Test that line continuation is handled correctly when valdiating shader source
TEST_P(WebGLCompatibilityTest, ShaderSourceLineContinuation)
{
    // Verify that a line continuation character (i.e. backslash) cannot be used
    // within a preprocessor directive in a ES2 context.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);

    const char *validVert =
        R"(#define foo this is a test
precision mediump float;
void main()
{
    gl_Position = vec4(1.0);
})";

    const char *invalidVert =
        R"(#define foo this \
    is a test
precision mediump float;
void main()
{
    gl_Position = vec4(1.0);
})";

    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &validVert, nullptr);
    EXPECT_GL_NO_ERROR();

    glShaderSource(shader, 1, &invalidVert, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDeleteShader(shader);
}

// Test that line continuation is handled correctly when valdiating shader source
TEST_P(WebGL2CompatibilityTest, ShaderSourceLineContinuation)
{
    const char *validVert =
        R"(#version 300 es
precision mediump float;

void main ()
{
    float f\
oo = 1.0;
    gl_Position = vec4(foo);
})";

    const char *invalidVert =
        R"(#version 300 es
precision mediump float;

void main ()
{
    float f\$
oo = 1.0;
    gl_Position = vec4(foo);
})";

    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &validVert, nullptr);
    EXPECT_GL_NO_ERROR();
    glShaderSource(shader, 1, &invalidVert, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDeleteShader(shader);
}

// Tests bindAttribLocations for reserved prefixes and length limits
TEST_P(WebGLCompatibilityTest, BindAttribLocationLimitation)
{
    constexpr int maxLocStringLength = 256;
    const std::string tooLongString(maxLocStringLength + 1, '_');

    glBindAttribLocation(0, 0, "_webgl_var");

    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBindAttribLocation(0, 0, static_cast<const GLchar *>(tooLongString.c_str()));

    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that having no attributes with a zero divisor is valid in WebGL2
TEST_P(WebGL2CompatibilityTest, InstancedDrawZeroDivisor)
{
    constexpr char kVS[] =
        R"(attribute float a_pos;
void main()
{
    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);

    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribDivisor(posLocation, 1);

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, nullptr);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();
}

// Tests that NPOT is not enabled by default in WebGL 1 and that it can be enabled
TEST_P(WebGLCompatibilityTest, NPOT)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_OES_texture_npot"));

    // Create a texture and set an NPOT mip 0, should always be acceptable.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // Try setting an NPOT mip 1 and verify the error if WebGL 1
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 5, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (getClientMajorVersion() < 3)
    {
        ASSERT_GL_ERROR(GL_INVALID_VALUE);
    }
    else
    {
        ASSERT_GL_NO_ERROR();
    }

    if (IsGLExtensionRequestable("GL_OES_texture_npot"))
    {
        glRequestExtensionANGLE("GL_OES_texture_npot");
        ASSERT_GL_NO_ERROR();

        // Try again to set NPOT mip 1
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 5, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();
    }
}

template <typename T>
void FillTexture2D(GLuint texture,
                   GLsizei width,
                   GLsizei height,
                   const T &onePixelData,
                   GLint level,
                   GLint internalFormat,
                   GLenum format,
                   GLenum type)
{
    std::vector<T> allPixelsData(width * height, onePixelData);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0, format, type,
                 allPixelsData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// Test that unset gl_Position defaults to (0,0,0,0).
TEST_P(WebGLCompatibilityTest, DefaultPosition)
{
    // Draw a quad where each vertex is red if gl_Position is (0,0,0,0) before it is set,
    // and green otherwise.  The center of each quadrant will be red if and only if all
    // four corners are red.
    constexpr char kVS[] =
        R"(attribute vec3 pos;
varying vec4 color;
void main() {
    if (gl_Position == vec4(0,0,0,0)) {
        color = vec4(1,0,0,1);
    } else {
        color = vec4(0,1,0,1);
    }
    gl_Position = vec4(pos,1);
})";

    constexpr char kFS[] =
        R"(precision mediump float;
varying vec4 color;
void main() {
    gl_FragColor = color;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program.get(), "pos", 0.0f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 1 / 4, getWindowHeight() * 1 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 1 / 4, getWindowHeight() * 3 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() * 1 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() * 3 / 4, GLColor::red);
}

// Tests that a rendering feedback loop triggers a GL error under WebGL.
// Based on WebGL test conformance/renderbuffers/feedback-loop.html.
TEST_P(WebGLCompatibilityTest, RenderingFeedbackLoop)
{
    constexpr char kVS[] =
        R"(attribute vec4 a_position;
varying vec2 v_texCoord;
void main() {
    gl_Position = a_position;
    v_texCoord = (a_position.xy * 0.5) + 0.5;
})";

    constexpr char kFS[] =
        R"(precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;
void main() {
    // Shader swizzles color channels so we can tell if the draw succeeded.
    gl_FragColor = texture2D(u_texture, v_texCoord).gbra;
})";

    GLTexture texture;
    FillTexture2D(texture.get(), 1, 1, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    GLint uniformLoc = glGetUniformLocation(program.get(), "u_texture");
    ASSERT_NE(-1, uniformLoc);

    glUseProgram(program.get());
    glUniform1i(uniformLoc, 0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    // Drawing with a texture that is also bound to the current framebuffer should fail
    glBindTexture(GL_TEXTURE_2D, texture.get());
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Ensure that the texture contents did not change after the previous render
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Drawing when texture is bound to an inactive uniform should succeed
    GLTexture texture2;
    FillTexture2D(texture2.get(), 1, 1, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture.get());
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Multi-context uses of textures should not cause rendering feedback loops.
TEST_P(WebGLCompatibilityTest, MultiContextNoRenderingFeedbackLoops)
{
    constexpr char kUnusedTextureVS[] =
        R"(attribute vec4 a_position;
varying vec2 v_texCoord;
void main() {
    gl_Position = a_position;
    v_texCoord = (a_position.xy * 0.5) + 0.5;
})";

    constexpr char kUnusedTextureFS[] =
        R"(precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;
void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord).rgba;
})";

    ANGLE_GL_PROGRAM(unusedProgram, kUnusedTextureVS, kUnusedTextureFS);

    glUseProgram(unusedProgram.get());
    GLint uniformLoc = glGetUniformLocation(unusedProgram.get(), "u_texture");
    ASSERT_NE(-1, uniformLoc);
    glUniform1i(uniformLoc, 0);

    GLTexture texture;
    FillTexture2D(texture.get(), 1, 1, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    glBindTexture(GL_TEXTURE_2D, texture.get());
    // Note that _texture_ is still bound to GL_TEXTURE_2D in this context at this point.

    EGLWindow *window          = getEGLWindow();
    EGLDisplay display         = window->getDisplay();
    EGLConfig config           = window->getConfig();
    EGLSurface surface         = window->getSurface();
    EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        GetParam().majorVersion,
        EGL_CONTEXT_MINOR_VERSION_KHR,
        GetParam().minorVersion,
        EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE,
        EGL_TRUE,
        EGL_NONE,
    };
    auto context1 = eglGetCurrentContext();
    // Create context2, sharing resources with context1.
    auto context2 = eglCreateContext(display, config, context1, contextAttributes);
    ASSERT_NE(context2, EGL_NO_CONTEXT);
    eglMakeCurrent(display, surface, surface, context2);

    constexpr char kVS[] =
        R"(attribute vec4 a_position;
void main() {
    gl_Position = a_position;
})";

    constexpr char kFS[] =
        R"(precision mediump float;
void main() {
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());

    ASSERT_GL_NO_ERROR();

    // Render to the texture in context2.
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    // Texture is still a valid name in context2.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    // There is no rendering feedback loop at this point.

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    eglMakeCurrent(display, surface, surface, context1);
    eglDestroyContext(display, context2);
}

// Test for the max draw buffers and color attachments.
TEST_P(WebGLCompatibilityTest, MaxDrawBuffersAttachmentPoints)
{
    // This test only applies to ES2.
    if (getClientMajorVersion() != 2)
    {
        return;
    }

    GLFramebuffer fbo[2];
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0].get());

    // Test that is valid when we bind with a single attachment point.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);
    ASSERT_GL_NO_ERROR();

    // Test that enabling the draw buffers extension will allow us to bind with a non-zero
    // attachment point.
    if (IsGLExtensionRequestable("GL_EXT_draw_buffers"))
    {
        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(IsGLExtensionEnabled("GL_EXT_draw_buffers"));

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[1].get());

        GLTexture texture2;
        glBindTexture(GL_TEXTURE_2D, texture2.get());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture2.get(),
                               0);
        ASSERT_GL_NO_ERROR();
    }
}

// Test that the offset in the index buffer is forced to be a multiple of the element size
TEST_P(WebGLCompatibilityTest, DrawElementsOffsetRestriction)
{
    constexpr char kVS[] =
        R"(attribute vec3 a_pos;
void main()
{
    gl_Position = vec4(a_pos, 1.0);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl1_shaders::fs::Red());

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    GLBuffer indexBuffer;
    const GLubyte indices[] = {0, 0, 0, 0, 0, 0, 0, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    ASSERT_GL_NO_ERROR();

    const char *zeroIndices = nullptr;

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, zeroIndices);
    ASSERT_GL_NO_ERROR();

    glDrawElements(GL_TRIANGLES, 4, GL_UNSIGNED_SHORT, zeroIndices);
    ASSERT_GL_NO_ERROR();

    glDrawElements(GL_TRIANGLES, 4, GL_UNSIGNED_SHORT, zeroIndices + 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that the offset and stride in the vertex buffer is forced to be a multiple of the element
// size
TEST_P(WebGLCompatibilityTest, VertexAttribPointerOffsetRestriction)
{
    const char *zeroOffset = nullptr;

    // Base case, vector of two floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset);
    ASSERT_GL_NO_ERROR();

    // Test setting a non-multiple offset
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 3);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test setting a non-multiple stride
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 1, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

void WebGLCompatibilityTest::drawBuffersEXTFeedbackLoop(GLuint program,
                                                        const std::array<GLenum, 2> &drawBuffers,
                                                        GLenum expectedError)
{
    glDrawBuffersEXT(2, drawBuffers.data());

    // Make sure framebuffer is complete before feedback loop detection
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    drawQuad(program, "aPosition", 0.5f, 1.0f, true);

    // "Rendering to a texture where it samples from should geneates INVALID_OPERATION. Otherwise,
    // it should be NO_ERROR"
    EXPECT_GL_ERROR(expectedError);
}

// This tests that rendering feedback loops works as expected with GL_EXT_draw_buffers.
// Based on WebGL test conformance/extensions/webgl-draw-buffers-feedback-loop.html
TEST_P(WebGLCompatibilityTest, RenderingFeedbackLoopWithDrawBuffersEXT)
{
    constexpr char kVS[] =
        R"(attribute vec4 aPosition;
varying vec2 texCoord;
void main() {
    gl_Position = aPosition;
    texCoord = (aPosition.xy * 0.5) + 0.5;
})";

    constexpr char kFS[] =
        R"(#extension GL_EXT_draw_buffers : require
precision mediump float;
uniform sampler2D tex;
varying vec2 texCoord;
void main() {
    gl_FragData[0] = texture2D(tex, texCoord);
    gl_FragData[1] = texture2D(tex, texCoord);
})";

    GLsizei width  = 8;
    GLsizei height = 8;

    // This shader cannot be run in ES3, because WebGL 2 does not expose the draw buffers
    // extension and gl_FragData semantics are changed to enforce indexing by zero always.
    // TODO(jmadill): This extension should be disabled in WebGL 2 contexts.
    if (/*!IsGLExtensionEnabled("GL_EXT_draw_buffers")*/ getClientMajorVersion() != 2)
    {
        // No WEBGL_draw_buffers support -- this is legal.
        return;
    }

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    // Test skipped because MAX_DRAW_BUFFERS is too small.
    ANGLE_SKIP_TEST_IF(maxDrawBuffers < 2);

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());
    glViewport(0, 0, width, height);

    GLTexture tex0;
    GLTexture tex1;
    GLFramebuffer fbo;
    FillTexture2D(tex0.get(), width, height, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, tex1.get());
    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    ASSERT_NE(-1, texLoc);
    glUniform1i(texLoc, 0);
    ASSERT_GL_NO_ERROR();

    // The sampling texture is bound to COLOR_ATTACHMENT1 during resource allocation
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex1.get(), 0);

    drawBuffersEXTFeedbackLoop(program.get(), {{GL_NONE, GL_COLOR_ATTACHMENT1}},
                               GL_INVALID_OPERATION);
    drawBuffersEXTFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}},
                               GL_INVALID_OPERATION);
    // A feedback loop is formed regardless of drawBuffers settings.
    drawBuffersEXTFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_NONE}},
                               GL_INVALID_OPERATION);
}

// Test tests that texture copying feedback loops are properly rejected in WebGL.
// Based on the WebGL test conformance/textures/misc/texture-copying-feedback-loops.html
TEST_P(WebGLCompatibilityTest, TextureCopyingFeedbackLoops)
{
    // Vulkan does not support copying from a texture to itself. http://anglebug.com/2914
    ANGLE_SKIP_TEST_IF(IsVulkan());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);

    // framebuffer should be FRAMEBUFFER_COMPLETE.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    // testing copyTexImage2D

    // copyTexImage2D to same texture but different level
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glCopyTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_NO_ERROR();

    // copyTexImage2D to same texture same level, invalid feedback loop
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // copyTexImage2D to different texture
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_NO_ERROR();

    // testing copyTexSubImage2D

    // copyTexSubImage2D to same texture but different level
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glCopyTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();

    // copyTexSubImage2D to same texture same level, invalid feedback loop
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // copyTexSubImage2D to different texture
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();
}

void WebGLCompatibilityTest::drawBuffersFeedbackLoop(GLuint program,
                                                     const std::array<GLenum, 2> &drawBuffers,
                                                     GLenum expectedError)
{
    glDrawBuffers(2, drawBuffers.data());

    // Make sure framebuffer is complete before feedback loop detection
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    drawQuad(program, "aPosition", 0.5f, 1.0f, true);

    // "Rendering to a texture where it samples from should geneates INVALID_OPERATION. Otherwise,
    // it should be NO_ERROR"
    EXPECT_GL_ERROR(expectedError);
}

// Tests invariance matching rules between built in varyings.
// Based on WebGL test conformance/glsl/misc/shaders-with-invariance.html.
TEST_P(WebGLCompatibilityTest, BuiltInInvariant)
{
    constexpr char kVS[] =
        R"(varying vec4 v_varying;
void main()
{
    gl_PointSize = 1.0;
    gl_Position = v_varying;
})";
    constexpr char kFSInvariantGlFragCoord[] =
        R"(invariant gl_FragCoord;
void main()
{
    gl_FragColor = gl_FragCoord;
})";
    constexpr char kFSInvariantGlPointCoord[] =
        R"(invariant gl_PointCoord;
void main()
{
    gl_FragColor = vec4(gl_PointCoord, 0.0, 0.0);
})";

    GLuint program = CompileProgram(kVS, kFSInvariantGlFragCoord);
    EXPECT_EQ(0u, program);

    program = CompileProgram(kVS, kFSInvariantGlPointCoord);
    EXPECT_EQ(0u, program);
}

// Tests global namespace conflicts between uniforms and attributes.
// Based on WebGL test conformance/glsl/misc/shaders-with-name-conflicts.html.
TEST_P(WebGLCompatibilityTest, GlobalNamesConflict)
{
    constexpr char kVS[] =
        R"(attribute vec4 foo;
void main()
{
    gl_Position = foo;
})";
    constexpr char kFS[] =
        R"(precision mediump float;
uniform vec4 foo;
void main()
{
    gl_FragColor = foo;
})";

    GLuint program = CompileProgram(kVS, kFS);
    EXPECT_EQ(0u, program);
}

// Test dimension and image size validation of compressed textures
TEST_P(WebGLCompatibilityTest, CompressedTextureS3TC)
{
    if (IsGLExtensionRequestable("GL_EXT_texture_compression_dxt1"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_compression_dxt1");
    }

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    constexpr uint8_t CompressedImageDXT1[] = {0x00, 0xf8, 0x00, 0xf8, 0xaa, 0xaa, 0xaa, 0xaa};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Regular case, verify that it works
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test various dimensions that are not valid
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 3, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 3, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 2, 2, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 1, 1, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test various image sizes that are not valid
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1) - 1, CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1) + 1, CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 0,
                           CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    // Fill a full mip chain and verify that it works
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 2, 2, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexImage2D(GL_TEXTURE_2D, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 1, 1, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test that non-block size sub-uploads are not valid for the 0 mip
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test that non-block size sub-uploads are valid for if they fill the whole mip
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 1, 1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test that if the format miss-matches the texture, an error is generated
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

TEST_P(WebGLCompatibilityTest, L32FTextures)
{
    constexpr float textureData[]   = {15.1f, 0.0f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {textureData[0], textureData[0], textureData[0], 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized L 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized L 32F
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_storage");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_LUMINANCE32F_EXT, GL_LUMINANCE, GL_FLOAT, texture, filter,
                                   render, textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, A32FTextures)
{
    constexpr float textureData[]   = {33.33f, 0.0f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {0.0f, 0.0f, 0.0f, textureData[0]};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized A 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_ALPHA, GL_ALPHA, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized A 32F
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_storage");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_ALPHA32F_EXT, GL_ALPHA, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, LA32FTextures)
{
    constexpr float textureData[]   = {-0.21f, 15.1f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {textureData[0], textureData[0], textureData[0],
                                       textureData[1]};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized LA 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT, texture,
                                   filter, render, textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized LA 32F
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_storage");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_LUMINANCE_ALPHA32F_EXT, GL_LUMINANCE_ALPHA, GL_FLOAT, texture,
                                   filter, render, textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, R32FTextures)
{
    constexpr float data[] = {1000.0f, 0.0f, 0.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized R 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_rg");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RED, GL_RED, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized R 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (IsGLExtensionEnabled("GL_OES_texture_float") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_rg") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_storage"));
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_R32F, GL_RED, GL_FLOAT, texture, filter, render, data, data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RG32FTextures)
{
    constexpr float data[] = {1000.0f, -0.001f, 0.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RG 32F
        {
            bool texture = (IsGLExtensionEnabled("GL_OES_texture_float") &&
                            IsGLExtensionEnabled("GL_EXT_texture_rg"));
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG, GL_RG, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RG 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (IsGLExtensionEnabled("GL_OES_texture_float") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_rg") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_storage"));
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG32F, GL_RG, GL_FLOAT, texture, filter, render, data, data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGB32FTextures)
{
    // TODO(syoussefi): Missing format support.  http://anglebug.com/2898
    ANGLE_SKIP_TEST_IF(IsVulkan());

    constexpr float data[] = {1000.0f, -500.0f, 10.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGB 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGB 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (IsGLExtensionEnabled("GL_OES_texture_float") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_storage"));
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = IsGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgb");
            TestFloatTextureFormat(GL_RGB32F, GL_RGB, GL_FLOAT, texture, filter, render, data,
                                   data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGBA32FTextures)
{
    // TODO(syoussefi): Missing format support.  http://anglebug.com/2898
    ANGLE_SKIP_TEST_IF(IsVulkan());

    constexpr float data[] = {7000.0f, 100.0f, 33.0f, -1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGBA 32F
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGBA 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (IsGLExtensionEnabled("GL_OES_texture_float") &&
                                                   IsGLExtensionEnabled("GL_EXT_texture_storage"));
            bool filter = IsGLExtensionEnabled("GL_OES_texture_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_float") ||
                          IsGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgba");
            TestFloatTextureFormat(GL_RGBA32F, GL_RGBA, GL_FLOAT, texture, filter, render, data,
                                   data);
        }
    }
}

// Test that has float color attachment caching works when color attachments change, by calling draw
// command when blending is enabled
TEST_P(WebGLCompatibilityTest, FramebufferFloatColorAttachment)
{
    if (getClientMajorVersion() >= 3)
    {
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"));
    }
    else
    {
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_texture_float"));
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgba"));
    }

    constexpr char kVS[] =
        R"(void main()
{
    gl_Position = vec4(0, 0, 0, 1);
})";

    constexpr char kFS[] =
        R"(void main()
{
    gl_FragColor = vec4(0, 1, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glEnable(GL_BLEND);

    GLTexture texture1;
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    EXPECT_GL_NO_ERROR();

    GLFramebuffer fbo1;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLFramebuffer fbo2;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDisable(GL_BLEND);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();
    glEnable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    glDrawArrays(GL_POINTS, 0, 1);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0,
                           0);  // test unbind
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDisable(GL_BLEND);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();
}

// Test that has float color attachment caching works with multiple color attachments bound to a
// Framebuffer
TEST_P(WebGLCompatibilityTest, FramebufferFloatColorAttachmentMRT)
{
    bool isWebGL2 = getClientMajorVersion() >= 3;
    if (isWebGL2)
    {
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"));

        constexpr char kVS[] =
            R"(#version 300 es
void main()
{
    gl_Position = vec4(0, 0, 0, 1);
})";

        constexpr char kFS[] =
            R"(#version 300 es
precision lowp float;
layout(location = 0) out vec4 o_color0;
layout(location = 1) out vec4 o_color1;
void main()
{
    o_color0 = vec4(1, 0, 0, 1);
    o_color1 = vec4(0, 1, 0, 1);
})";

        ANGLE_GL_PROGRAM(program, kVS, kFS);
        glUseProgram(program);
    }
    else
    {
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_texture_float"));
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgba"));
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_draw_buffers"));

        constexpr char kVS[] =
            R"(void main()
{
    gl_Position = vec4(0, 0, 0, 1);
})";

        constexpr char kFS[] =
            R"(#extension GL_EXT_draw_buffers : require
precision lowp float;
void main()
{
    gl_FragData[0] = vec4(1, 0, 0, 1);
    gl_FragData[1] = vec4(0, 1, 0, 1);
})";

        ANGLE_GL_PROGRAM(program, kVS, kFS);
        glUseProgram(program);
    }

    glEnable(GL_BLEND);

    GLTexture texture1;
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_NO_ERROR();

    GLTexture textureF1;
    glBindTexture(GL_TEXTURE_2D, textureF1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    EXPECT_GL_NO_ERROR();

    GLTexture textureF2;
    glBindTexture(GL_TEXTURE_2D, textureF2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    EXPECT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture2, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLenum drawbuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    if (isWebGL2)
    {
        glDrawBuffers(2, drawbuffers);
    }
    else
    {
        glDrawBuffersEXT(2, drawbuffers);
    }

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureF1, 0);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textureF2, 0);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (isWebGL2)
    {
        // WebGL 1 will report a FRAMEBUFFER_UNSUPPORTED for one unsigned_byte and one float
        // attachment bound to one FBO at the same time
        glDrawBuffers(1, drawbuffers);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glDrawArrays(GL_POINTS, 0, 1);
        EXPECT_GL_NO_ERROR();
        glDrawBuffers(2, drawbuffers);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture2, 0);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();
}

static void TestBlendColor(const bool shouldClamp)
{
    auto expected = GLColor32F(5, 0, 0, 0);
    glBlendColor(expected.R, expected.G, expected.B, expected.A);
    if (shouldClamp)
    {
        expected.R = 1;
    }

    float arr[4] = {};
    glGetFloatv(GL_BLEND_COLOR, arr);
    const auto actual = GLColor32F(arr[0], arr[1], arr[2], arr[3]);
    EXPECT_COLOR_NEAR(expected, actual, 0.001);
}

// Test if blending of float32 color attachment generates GL_INVALID_OPERATION when
// GL_EXT_float_blend is not enabled
TEST_P(WebGLCompatibilityTest, FloatBlend)
{
    if (getClientMajorVersion() >= 3)
    {
        TestBlendColor(false);
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"));
    }
    else
    {
        TestBlendColor(true);
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_texture_float"));
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_CHROMIUM_color_buffer_float_rgba"));
    }

    TestBlendColor(false);

    // -

    TestExtFloatBlend(GL_RGBA32F, GL_FLOAT, false);

    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_float_blend"));
    ASSERT_GL_NO_ERROR();

    // D3D9 supports float rendering explicitly, supports blending operations in practice,
    // but cannot support float blend colors.
    ANGLE_SKIP_TEST_IF(IsD3D9());

    TestExtFloatBlend(GL_RGBA32F, GL_FLOAT, true);
}

// Test the blending of float16 color attachments
TEST_P(WebGLCompatibilityTest, HalfFloatBlend)
{
    GLenum internalFormat = GL_RGBA16F;
    GLenum type           = GL_FLOAT;
    if (getClientMajorVersion() >= 3)
    {
        TestBlendColor(false);
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"));
    }
    else
    {
        TestBlendColor(true);
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_OES_texture_half_float"));
        ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_color_buffer_half_float"));
        internalFormat = GL_RGBA;
        type           = GL_HALF_FLOAT_OES;
    }

    TestBlendColor(false);

    // -

    // D3D9 supports float rendering explicitly, supports blending operations in practice,
    // but cannot support float blend colors.
    ANGLE_SKIP_TEST_IF(IsD3D9());

    TestExtFloatBlend(internalFormat, type, true);
}

TEST_P(WebGLCompatibilityTest, R16FTextures)
{
    constexpr float readPixelsData[] = {-5000.0f, 0.0f, 0.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized R 16F (OES)
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_rg");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_RED, GL_RED, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized R 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RED, GL_RED, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3)
        {
            // Sized R 16F
            bool texture = true;
            bool filter  = true;
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_R16F, GL_RED, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
        else if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized R 16F (OES)
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_rg");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_R16F, GL_RED, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RG16FTextures)
{
    constexpr float readPixelsData[] = {7108.0f, -10.0f, 0.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RG 16F (OES)
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_rg");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_RG, GL_RG, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RG 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RG, GL_RG, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3)
        {
            // Sized RG 16F
            bool texture = true;
            bool filter  = true;
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG16F, GL_RG, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
        else if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RG 16F (OES)
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float") &&
                           IsGLExtensionEnabled("GL_EXT_texture_rg");
            bool filter = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RG16F, GL_RG, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGB16FTextures)
{
    // TODO(syoussefi): Missing format support.  http://anglebug.com/2898
    ANGLE_SKIP_TEST_IF(IsVulkan());

    ANGLE_SKIP_TEST_IF(IsOzone() && IsIntel());

    constexpr float readPixelsData[] = {7000.0f, 100.0f, 33.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGB 16F (OES)
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            // WebGL says that Unsized RGB 16F (OES) can be renderable with
            // GL_EXT_color_buffer_half_float.
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RGB 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3)
        {
            // Sized RGB 16F
            bool texture = true;
            bool filter  = true;
            // It is unclear how EXT_color_buffer_half_float applies to ES3.0 and above, however,
            // dEQP GLES3 es3fFboColorbufferTests.cpp verifies that texture attachment of GL_RGB16F
            // is possible, so assume that all GLES implementations support it.
            bool render = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGB16F, GL_RGB, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
        else if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGB 16F (OES)
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGB16F, GL_RGB, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGBA16FTextures)
{
    ANGLE_SKIP_TEST_IF(IsOzone() && IsIntel());

    constexpr float readPixelsData[] = {7000.0f, 100.0f, 33.0f, -1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && IsGLExtensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGBA 16F (OES)
        {
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RGBA 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3)
        {
            // Sized RGBA 16F
            bool texture = true;
            bool filter  = true;
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
        else if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGBA 16F (OES)
            bool texture = IsGLExtensionEnabled("GL_OES_texture_half_float");
            bool filter  = IsGLExtensionEnabled("GL_OES_texture_half_float_linear");
            bool render  = IsGLExtensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

// Test that when GL_CHROMIUM_color_buffer_float_rgb[a] is enabled, sized GL_RGB[A]_32F formats are
// accepted by glTexImage2D
TEST_P(WebGLCompatibilityTest, SizedRGBA32FFormats)
{
    // Test skipped because it is only valid for WebGL1 contexts.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() != 2);

    ANGLE_SKIP_TEST_IF(!IsGLExtensionRequestable("GL_OES_texture_float"));

    glRequestExtensionANGLE("GL_OES_texture_float");
    ASSERT_GL_NO_ERROR();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    // dEQP implicitly defines error code ordering
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, nullptr);
    // dEQP implicitly defines error code ordering
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable("GL_CHROMIUM_color_buffer_float_rgba"))
    {
        glRequestExtensionANGLE("GL_CHROMIUM_color_buffer_float_rgba");
        ASSERT_GL_NO_ERROR();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        EXPECT_GL_NO_ERROR();
    }

    if (IsGLExtensionRequestable("GL_CHROMIUM_color_buffer_float_rgb"))
    {
        glRequestExtensionANGLE("GL_CHROMIUM_color_buffer_float_rgb");
        ASSERT_GL_NO_ERROR();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// Verify GL_DEPTH_STENCIL_ATTACHMENT is a valid attachment point.
TEST_P(WebGLCompatibilityTest, DepthStencilAttachment)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() > 2);

    // Test that attaching a bound texture succeeds.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture, 0);

    GLint attachmentType = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_TEXTURE, attachmentType);

    // Test when if no attach object at the named attachment point and pname is not OBJECT_TYPE.
    GLFramebuffer fbo2;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attachmentType);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Verify framebuffer attachments return expected types when in an inconsistant state.
TEST_P(WebGLCompatibilityTest, FramebufferAttachmentConsistancy)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() > 2);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer rb1;
    glBindRenderbuffer(GL_RENDERBUFFER, rb1);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb1);

    GLint attachmentType = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);

    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_RENDERBUFFER, attachmentType);

    GLRenderbuffer rb2;
    glBindRenderbuffer(GL_RENDERBUFFER, rb2);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb2);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);

    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_RENDERBUFFER, attachmentType);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb2);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);

    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_RENDERBUFFER, attachmentType);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb2);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);

    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_RENDERBUFFER, attachmentType);
}

// This tests that rendering feedback loops works as expected with WebGL 2.
// Based on WebGL test conformance2/rendering/rendering-sampling-feedback-loop.html
TEST_P(WebGL2CompatibilityTest, RenderingFeedbackLoopWithDrawBuffers)
{
    constexpr char kVS[] =
        R"(#version 300 es
in vec4 aPosition;
out vec2 texCoord;
void main() {
    gl_Position = aPosition;
    texCoord = (aPosition.xy * 0.5) + 0.5;
})";

    constexpr char kFS[] =
        R"(#version 300 es
precision mediump float;
uniform sampler2D tex;
in vec2 texCoord;
out vec4 oColor;
void main() {
    oColor = texture(tex, texCoord);
})";

    GLsizei width  = 8;
    GLsizei height = 8;

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    // ES3 requires a minimum value of 4 for MAX_DRAW_BUFFERS.
    ASSERT_GE(maxDrawBuffers, 2);

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());
    glViewport(0, 0, width, height);

    GLTexture tex0;
    GLTexture tex1;
    GLFramebuffer fbo;
    FillTexture2D(tex0.get(), width, height, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, tex1.get());
    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    ASSERT_NE(-1, texLoc);
    glUniform1i(texLoc, 0);

    // The sampling texture is bound to COLOR_ATTACHMENT1 during resource allocation
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex1.get(), 0);
    ASSERT_GL_NO_ERROR();

    drawBuffersFeedbackLoop(program.get(), {{GL_NONE, GL_COLOR_ATTACHMENT1}}, GL_INVALID_OPERATION);
    drawBuffersFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}},
                            GL_INVALID_OPERATION);
    // A feedback loop is formed regardless of drawBuffers settings.
    drawBuffersFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_NONE}}, GL_INVALID_OPERATION);
}

// This test covers detection of rendering feedback loops between the FBO and a depth Texture.
// Based on WebGL test conformance2/rendering/depth-stencil-feedback-loop.html
TEST_P(WebGL2CompatibilityTest, RenderingFeedbackLoopWithDepthStencil)
{
    constexpr char kVS[] =
        R"(#version 300 es
in vec4 aPosition;
out vec2 texCoord;
void main() {
    gl_Position = aPosition;
    texCoord = (aPosition.xy * 0.5) + 0.5;
})";

    constexpr char kFS[] =
        R"(#version 300 es
precision mediump float;
uniform sampler2D tex;
in vec2 texCoord;
out vec4 oColor;
void main() {
    oColor = texture(tex, texCoord);
})";

    GLsizei width  = 8;
    GLsizei height = 8;

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());

    glViewport(0, 0, width, height);

    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    glUniform1i(texLoc, 0);

    // Create textures and allocate storage
    GLTexture tex0;
    GLTexture tex1;
    GLTexture tex2;
    FillTexture2D(tex0.get(), width, height, GLColor::black, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, 0x80, 0, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
                  GL_UNSIGNED_INT);
    FillTexture2D(tex2.get(), width, height, 0x40, 0, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
                  GL_UNSIGNED_INT_24_8);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);

    // Test rendering and sampling feedback loop for depth buffer
    glBindTexture(GL_TEXTURE_2D, tex1.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex1.get(), 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // The same image is used as depth buffer during rendering.
    glEnable(GL_DEPTH_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Same image as depth buffer should fail";

    // The same image is used as depth buffer. But depth mask is false.
    // This is now considered a feedback loop and should generate an error. http://crbug.com/763695
    glDepthMask(GL_FALSE);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Depth writes disabled should still fail";

    // The same image is used as depth buffer. But depth test is not enabled during rendering.
    // This is now considered a feedback loop and should generate an error. http://crbug.com/763695
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Depth read disabled should still fail";

    // Test rendering and sampling feedback loop for stencil buffer
    glBindTexture(GL_TEXTURE_2D, tex2.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex2.get(), 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    constexpr GLint stencilClearValue = 0x40;
    glClearBufferiv(GL_STENCIL, 0, &stencilClearValue);

    // The same image is used as stencil buffer during rendering.
    glEnable(GL_STENCIL_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Same image as stencil buffer should fail";

    // The same image is used as stencil buffer. But stencil mask is zero.
    // This is now considered a feedback loop and should generate an error. http://crbug.com/763695
    glStencilMask(0x0);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Stencil mask zero should still fail";

    // The same image is used as stencil buffer. But stencil test is not enabled during rendering.
    // This is now considered a feedback loop and should generate an error. http://crbug.com/763695
    glStencilMask(0xffff);
    glDisable(GL_STENCIL_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION) << "Stencil test disabled should still fail";
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// But the level of the 3D texture != the level of the read attachment.
TEST_P(WebGL2CompatibilityTest, NoTextureCopyingFeedbackLoopBetween3DLevels)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 0);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_NO_ERROR();
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// But the zoffset of the 3D texture != the layer of the read attachment.
TEST_P(WebGL2CompatibilityTest, NoTextureCopyingFeedbackLoopBetween3DLayers)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 1);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_NO_ERROR();
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// And the level / zoffset of the 3D texture is equal to the level / layer of the read attachment.
TEST_P(WebGL2CompatibilityTest, TextureCopyingFeedbackLoop3D)
{
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 2, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 1, 0);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when there isn't a defined conversion between the clear type and
// the buffer type.
TEST_P(WebGL2CompatibilityTest, ClearBufferTypeCompatibity)
{
    // Test skipped for D3D11 because it generates D3D11 runtime warnings.
    ANGLE_SKIP_TEST_IF(IsD3D11());

    constexpr float clearFloat[]       = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr int clearInt[]           = {0, 0, 0, 0};
    constexpr unsigned int clearUint[] = {0, 0, 0, 0};

    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_2D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);
    ASSERT_GL_NO_ERROR();

    // Unsigned integer buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_NO_ERROR();

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Integer buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_NO_ERROR();

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Float buffer
    if (IsGLExtensionRequestable("GL_EXT_color_buffer_float"))
    {
        glRequestExtensionANGLE("GL_EXT_color_buffer_float");
    }

    if (IsGLExtensionEnabled("GL_EXT_color_buffer_float"))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        ASSERT_GL_NO_ERROR();

        glClearBufferfv(GL_COLOR, 0, clearFloat);
        EXPECT_GL_NO_ERROR();

        glClearBufferiv(GL_COLOR, 0, clearInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glClearBufferuiv(GL_COLOR, 0, clearUint);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_GL_NO_ERROR();
    }

    // Normalized uint buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_NO_ERROR();

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();
}

// Test the interaction of WebGL compatibility clears with default framebuffers
TEST_P(WebGL2CompatibilityTest, ClearBufferDefaultFramebuffer)
{
    constexpr float clearFloat[]       = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr int clearInt[]           = {0, 0, 0, 0};
    constexpr unsigned int clearUint[] = {0, 0, 0, 0};

    // glClear works as usual, this is also a regression test for a bug where we
    // iterated on maxDrawBuffers for default framebuffers, triggering an assert
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Default framebuffers are normalized uints, so only glClearBufferfv works.
    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_NO_ERROR();

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generate when trying to blit from an image to itself
TEST_P(WebGL2CompatibilityTest, BlitFramebufferSameImage)
{
    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);

    GLRenderbuffer renderbuffers[2];
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 4, 4);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 4, 4);

    GLFramebuffer framebuffers[2];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);

    ASSERT_GL_NO_ERROR();

    // Same texture
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Same textures but different renderbuffers
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[1]);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Same renderbuffers but different textures
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1],
                           0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when the fragment shader output doesn't match the bound color
// buffer types
TEST_P(WebGL2CompatibilityTest, FragmentShaderColorBufferTypeMissmatch)
{
    constexpr char kVS[] =
        R"(#version 300 es
void main() {
    gl_Position = vec4(0, 0, 0, 1);
})";

    constexpr char kFS[] =
        R"(#version 300 es
precision mediump float;
layout(location = 0) out vec4 floatOutput;
layout(location = 1) out uvec4 uintOutput;
layout(location = 2) out ivec4 intOutput;
void main() {
    floatOutput = vec4(0, 0, 0, 1);
    uintOutput = uvec4(0, 0, 0, 1);
    intOutput = ivec4(0, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());

    GLuint floatLocation = glGetFragDataLocation(program, "floatOutput");
    GLuint uintLocation  = glGetFragDataLocation(program, "uintOutput");
    GLuint intLocation   = glGetFragDataLocation(program, "intOutput");

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer floatRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, floatRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + floatLocation, GL_RENDERBUFFER,
                              floatRenderbuffer);

    GLRenderbuffer uintRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, uintRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);

    GLRenderbuffer intRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, intRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8I, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              intRenderbuffer);

    ASSERT_GL_NO_ERROR();

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    std::vector<GLenum> drawBuffers(static_cast<size_t>(maxDrawBuffers), GL_NONE);
    drawBuffers[floatLocation] = GL_COLOR_ATTACHMENT0 + floatLocation;
    drawBuffers[uintLocation]  = GL_COLOR_ATTACHMENT0 + uintLocation;
    drawBuffers[intLocation]   = GL_COLOR_ATTACHMENT0 + intLocation;

    glDrawBuffers(maxDrawBuffers, drawBuffers.data());

    // Check that the correct case generates no errors
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Unbind some buffers and verify that there are still no errors
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Swap the int and uint buffers to and verify that an error is generated
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              intRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Swap the float and uint buffers to and verify that an error is generated
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              floatRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + floatLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              intRenderbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when the vertex shader intput doesn't match the bound attribute
// types
TEST_P(WebGL2CompatibilityTest, VertexShaderAttributeTypeMismatch)
{
    constexpr char kVS[] =
        R"(#version 300 es
in vec4 floatInput;
in uvec4 uintInput;
in ivec4 intInput;
void main() {
    gl_Position = vec4(floatInput.x, uintInput.x, intInput.x, 1);
})";

    constexpr char kFS[] =
        R"(#version 300 es
precision mediump float;
out vec4 outputColor;
void main() {
    outputColor = vec4(0, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program.get());

    GLint floatLocation = glGetAttribLocation(program, "floatInput");
    GLint uintLocation  = glGetAttribLocation(program, "uintInput");
    GLint intLocation   = glGetAttribLocation(program, "intInput");

    // Default attributes are of float types
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Set the default attributes to the correct types, should succeed
    glVertexAttribI4ui(uintLocation, 0, 0, 0, 1);
    glVertexAttribI4i(intLocation, 0, 0, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Change the default float attribute to an integer, should fail
    glVertexAttribI4ui(floatLocation, 0, 0, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Use a buffer for some attributes
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
    glEnableVertexAttribArray(floatLocation);
    glVertexAttribPointer(floatLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Use a float pointer attrib for a uint input
    glEnableVertexAttribArray(uintLocation);
    glVertexAttribPointer(uintLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Use a uint pointer for the uint input
    glVertexAttribIPointer(uintLocation, 4, GL_UNSIGNED_INT, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();
}

// Test that it's not possible to query the non-zero color attachments without the drawbuffers
// extension in WebGL1
TEST_P(WebGLCompatibilityTest, FramebufferAttachmentQuery)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() > 2);
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_EXT_draw_buffers"));

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_NO_ERROR();

    GLint result;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &result);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, renderbuffer);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Tests WebGL reports INVALID_OPERATION for mismatch of drawbuffers and fragment output
TEST_P(WebGLCompatibilityTest, DrawBuffers)
{
    // Fails on Intel Ubuntu 19.04 Mesa 19.0.2 Vulkan. http://anglebug.com/3616
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsVulkan());

    // Make sure we can use at least 4 attachments for the tests.
    bool useEXT = false;
    if (getClientMajorVersion() < 3)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionRequestable("GL_EXT_draw_buffers"));

        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        useEXT = true;
        EXPECT_GL_NO_ERROR();
    }

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    // Test skipped because MAX_DRAW_BUFFERS is too small.
    ANGLE_SKIP_TEST_IF(maxDrawBuffers < 4);

    // Clears all the renderbuffers to red.
    auto ClearEverythingToRed = [](GLRenderbuffer *renderbuffers) {
        GLFramebuffer clearFBO;
        glBindFramebuffer(GL_FRAMEBUFFER, clearFBO);

        glClearColor(1, 0, 0, 1);
        for (int i = 0; i < 4; ++i)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                      renderbuffers[i]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        ASSERT_GL_NO_ERROR();
    };

    // Checks that the renderbuffers specified by mask have the correct color
    auto CheckColors = [](GLRenderbuffer *renderbuffers, int mask, GLColor color) {
        GLFramebuffer readFBO;
        glBindFramebuffer(GL_FRAMEBUFFER, readFBO);

        for (int attachmentIndex = 0; attachmentIndex < 4; ++attachmentIndex)
        {
            if (mask & (1 << attachmentIndex))
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                          renderbuffers[attachmentIndex]);
                EXPECT_PIXEL_COLOR_EQ(0, 0, color) << "attachment " << attachmentIndex;
            }
        }
        ASSERT_GL_NO_ERROR();
    };

    // Depending on whether we are using the extension or ES3, a different entrypoint must be called
    auto DrawBuffers = [](bool useEXT, int numBuffers, GLenum *buffers) {
        if (useEXT)
        {
            glDrawBuffersEXT(numBuffers, buffers);
        }
        else
        {
            glDrawBuffers(numBuffers, buffers);
        }
    };

    // Initialized the test framebuffer
    GLFramebuffer drawFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, drawFBO);

    GLRenderbuffer renderbuffers[4];
    for (int i = 0; i < 4; ++i)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER,
                                  renderbuffers[i]);
    }

    ASSERT_GL_NO_ERROR();

    GLenum allDrawBuffers[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    GLenum halfDrawBuffers[] = {
        GL_NONE,
        GL_COLOR_ATTACHMENT1,
        GL_NONE,
        GL_COLOR_ATTACHMENT3,
    };

    // Test that when using gl_FragColor with no-array
    const char *fragESSL1 =
        R"(precision highp float;
void main()
{
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
})";
    ANGLE_GL_PROGRAM(programESSL1, essl1_shaders::vs::Simple(), fragESSL1);

    {
        glBindFramebuffer(GL_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, allDrawBuffers);
        drawQuad(programESSL1, essl1_shaders::PositionAttrib(), 0.5, 1.0, true);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Test what happens when rendering to a subset of the outputs. There is a behavior difference
    // between the extension and ES3. In the extension gl_FragData is implicitly declared as an
    // array of size MAX_DRAW_BUFFERS, so the WebGL spec stipulates that elements not written to
    // should default to 0. On the contrary, in ES3 outputs are specified one by one, so
    // attachments not declared in the shader should not be written to.
    const char *positionAttrib;
    const char *writeOddOutputsVert;
    const char *writeOddOutputsFrag;
    if (useEXT)
    {
        positionAttrib      = essl1_shaders::PositionAttrib();
        writeOddOutputsVert = essl1_shaders::vs::Simple();
        writeOddOutputsFrag =
            R"(#extension GL_EXT_draw_buffers : require
precision highp float;
void main()
{
    gl_FragData[1] = vec4(0.0, 1.0, 0.0, 1.0);
    gl_FragData[3] = vec4(0.0, 1.0, 0.0, 1.0);
})";
    }
    else
    {
        positionAttrib      = essl3_shaders::PositionAttrib();
        writeOddOutputsVert = essl3_shaders::vs::Simple();
        writeOddOutputsFrag =
            R"(#version 300 es
precision highp float;
layout(location = 1) out vec4 output1;
layout(location = 3) out vec4 output2;
void main()
{
    output1 = vec4(0.0, 1.0, 0.0, 1.0);
    output2 = vec4(0.0, 1.0, 0.0, 1.0);
})";
    }
    ANGLE_GL_PROGRAM(writeOddOutputsProgram, writeOddOutputsVert, writeOddOutputsFrag);

    // Test that attachments not written to get the "unwritten" color (useEXT)
    // Or INVALID_OPERATION is generated if there's active draw buffer receive no output
    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, allDrawBuffers);
        drawQuad(writeOddOutputsProgram, positionAttrib, 0.5, 1.0, true);

        if (useEXT)
        {
            ASSERT_GL_NO_ERROR();
            CheckColors(renderbuffers, 0b1010, GLColor::green);
            // In the extension, when an attachment isn't written to, it should get 0's
            CheckColors(renderbuffers, 0b0101, GLColor(0, 0, 0, 0));
        }
        else
        {
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        }
    }

    // TODO(syoussefi): Qualcomm driver crashes in the presence of VK_ATTACHMENT_UNUSED.
    // http://anglebug.com/3423
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    // Test that attachments written to get the correct color from shader output but that even when
    // the extension is used, disabled attachments are not written at all and stay red.
    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, halfDrawBuffers);
        drawQuad(writeOddOutputsProgram, positionAttrib, 0.5, 1.0, true);
        ASSERT_GL_NO_ERROR();

        CheckColors(renderbuffers, 0b1010, GLColor::green);
        CheckColors(renderbuffers, 0b0101, GLColor::red);
    }
}

// Test that it's possible to generate mipmaps on unsized floating point textures once the
// extensions have been enabled
TEST_P(WebGLCompatibilityTest, GenerateMipmapUnsizedFloatingPointTexture)
{
    if (IsGLExtensionRequestable("GL_OES_texture_float"))
    {
        glRequestExtensionANGLE("GL_OES_texture_float");
    }
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    constexpr GLColor32F data[4] = {
        kFloatRed,
        kFloatRed,
        kFloatGreen,
        kFloatBlue,
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_FLOAT, data);
    ASSERT_GL_NO_ERROR();

    glGenerateMipmap(GL_TEXTURE_2D);
    EXPECT_GL_NO_ERROR();
}
// Test that it's possible to generate mipmaps on unsized floating point textures once the
// extensions have been enabled
TEST_P(WebGLCompatibilityTest, GenerateMipmapSizedFloatingPointTexture)
{
    if (IsGLExtensionRequestable("GL_OES_texture_float"))
    {
        glRequestExtensionANGLE("GL_OES_texture_float");
    }
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    if (IsGLExtensionRequestable("GL_EXT_texture_storage"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_storage");
    }
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_storage"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    constexpr GLColor32F data[4] = {
        kFloatRed,
        kFloatRed,
        kFloatGreen,
        kFloatBlue,
    };
    glTexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGBA32F, 2, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_FLOAT, data);
    ASSERT_GL_NO_ERROR();

    glGenerateMipmap(GL_TEXTURE_2D);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (IsGLExtensionRequestable("GL_EXT_color_buffer_float"))
    {
        // Format is renderable but not filterable
        glRequestExtensionANGLE("GL_EXT_color_buffer_float");
        glGenerateMipmap(GL_TEXTURE_2D);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    if (IsGLExtensionRequestable("GL_EXT_color_buffer_float_linear"))
    {
        // Format is renderable but not filterable
        glRequestExtensionANGLE("GL_EXT_color_buffer_float_linear");

        if (IsGLExtensionEnabled("GL_EXT_color_buffer_float"))
        {
            // Format is filterable and renderable
            glGenerateMipmap(GL_TEXTURE_2D);
            EXPECT_GL_NO_ERROR();
        }
        else
        {
            // Format is filterable but not renderable
            glGenerateMipmap(GL_TEXTURE_2D);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        }
    }
}

// Verify that a texture format is only allowed with extension enabled.
void WebGLCompatibilityTest::validateTexImageExtensionFormat(GLenum format,
                                                             const std::string &extName)
{
    // Verify texture format fails by default.
    glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, format, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable(extName))
    {
        // Verify texture format is allowed once extension is enabled.
        glRequestExtensionANGLE(extName.c_str());
        EXPECT_TRUE(IsGLExtensionEnabled(extName));

        glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, format, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();
    }
}

// Test enabling various non-compressed texture format extensions
TEST_P(WebGLCompatibilityTest, EnableTextureFormatExtensions)
{
    ANGLE_SKIP_TEST_IF(IsOzone());
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() != 2);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());

    // Verify valid format is allowed.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // Verify invalid format fails.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA32F, GL_UNSIGNED_BYTE, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Verify formats from enableable extensions.
    if (!IsOpenGLES())
    {
        validateTexImageExtensionFormat(GL_RED_EXT, "GL_EXT_texture_rg");
    }

    validateTexImageExtensionFormat(GL_SRGB_EXT, "GL_EXT_texture_sRGB");
    validateTexImageExtensionFormat(GL_BGRA_EXT, "GL_EXT_texture_format_BGRA8888");
}

void WebGLCompatibilityTest::validateCompressedTexImageExtensionFormat(GLenum format,
                                                                       GLsizei width,
                                                                       GLsizei height,
                                                                       GLsizei blockSize,
                                                                       const std::string &extName,
                                                                       bool subImageAllowed)
{
    std::vector<GLubyte> data(blockSize, 0u);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());

    // Verify texture format fails by default.
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, blockSize, data.data());
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (IsGLExtensionRequestable(extName))
    {
        // Verify texture format is allowed once extension is enabled.
        glRequestExtensionANGLE(extName.c_str());
        EXPECT_TRUE(IsGLExtensionEnabled(extName));

        glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, blockSize, data.data());
        EXPECT_GL_NO_ERROR();

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, blockSize,
                                  data.data());
        if (subImageAllowed)
        {
            EXPECT_GL_NO_ERROR();
        }
        else
        {
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        }
    }
}

// Test enabling GL_EXT_texture_compression_dxt1 for GL_COMPRESSED_RGB_S3TC_DXT1_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT1RGB)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 8,
                                              "GL_EXT_texture_compression_dxt1", true);
}

// Test enabling GL_EXT_texture_compression_dxt1 for GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT1RGBA)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4, 8,
                                              "GL_EXT_texture_compression_dxt1", true);
}

// Test enabling GL_ANGLE_texture_compression_dxt3
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT3)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, 4, 4, 16,
                                              "GL_ANGLE_texture_compression_dxt3", true);
}

// Test enabling GL_ANGLE_texture_compression_dxt5
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT5)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, 4, 4, 16,
                                              "GL_ANGLE_texture_compression_dxt5", true);
}

// Test enabling GL_EXT_texture_compression_s3tc_srgb for GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT1SRGB)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 4, 4, 8,
                                              "GL_EXT_texture_compression_s3tc_srgb", true);
}

// Test enabling GL_EXT_texture_compression_s3tc_srgb for GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT1SRGBA)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 4, 4, 8,
                                              "GL_EXT_texture_compression_s3tc_srgb", true);
}

// Test enabling GL_EXT_texture_compression_s3tc_srgb for GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT3SRGBA)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 4, 4, 16,
                                              "GL_EXT_texture_compression_s3tc_srgb", true);
}

// Test enabling GL_EXT_texture_compression_s3tc_srgb for GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionDXT5SRGBA)
{
    validateCompressedTexImageExtensionFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 4, 4, 16,
                                              "GL_EXT_texture_compression_s3tc_srgb", true);
}

// Test enabling GL_OES_compressed_ETC1_RGB8_texture
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionETC1)
{
    validateCompressedTexImageExtensionFormat(
        GL_ETC1_RGB8_OES, 4, 4, 8, "GL_OES_compressed_ETC1_RGB8_texture",
        IsGLExtensionEnabled("GL_EXT_compressed_ETC1_RGB8_sub_texture"));
}

// Test enabling GL_ANGLE_lossy_etc_decode
TEST_P(WebGLCompatibilityTest, EnableCompressedTextureExtensionLossyDecode)
{
    validateCompressedTexImageExtensionFormat(GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, 4, 4, 8,
                                              "GL_ANGLE_lossy_etc_decode", true);
}

// Linking should fail when corresponding vertex/fragment uniform blocks have different precision
// qualifiers.
TEST_P(WebGL2CompatibilityTest, UniformBlockPrecisionMismatch)
{
    constexpr char kVS[] =
        R"(#version 300 es
uniform Block { mediump vec4 val; };
void main() { gl_Position = val; })";
    constexpr char kFS[] =
        R"(#version 300 es
uniform Block { highp vec4 val; };
out highp vec4 out_FragColor;
void main() { out_FragColor = val; })";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
    ASSERT_NE(0u, vs);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
    ASSERT_NE(0u, fs);

    GLuint program = glCreateProgram();

    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    ASSERT_EQ(0, linkStatus);

    glDeleteProgram(program);
}

// Test no attribute vertex shaders
TEST_P(WebGL2CompatibilityTest, NoAttributeVertexShader)
{
    constexpr char kVS[] =
        R"(#version 300 es
void main()
{

    ivec2 xy = ivec2(gl_VertexID % 2, (gl_VertexID / 2 + gl_VertexID / 3) % 2);
    gl_Position = vec4(vec2(xy) * 2. - 1., 0, 1);
})";

    ANGLE_GL_PROGRAM(program, kVS, essl3_shaders::fs::Red());
    glUseProgram(program);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Tests bindAttribLocations for length limit
TEST_P(WebGL2CompatibilityTest, BindAttribLocationLimitation)
{
    constexpr int maxLocStringLength = 1024;
    const std::string tooLongString(maxLocStringLength + 1, '_');

    glBindAttribLocation(0, 0, static_cast<const GLchar *>(tooLongString.c_str()));

    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Covers a bug in transform feedback loop detection.
TEST_P(WebGL2CompatibilityTest, TransformFeedbackCheckNullDeref)
{
    constexpr char kVS[] = R"(attribute vec4 color; void main() { color.r; })";
    constexpr char kFS[] = R"(void main(){})";
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, 1);

    // This should fail because it is trying to pull a vertex with no buffer.
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    // This should fail because it is trying to pull a vertex from an empty buffer.
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// We should forbid two transform feedback outputs going to the same buffer.
TEST_P(WebGL2CompatibilityTest, TransformFeedbackDoubleBinding)
{
    constexpr char kVS[] =
        R"(attribute float a; varying float b; varying float c; void main() { b = a; c = a; })";
    constexpr char kFS[] = R"(void main(){})";
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    static const char *varyings[] = {"b", "c"};
    glTransformFeedbackVaryings(program, 2, varyings, GL_SEPARATE_ATTRIBS);
    glLinkProgram(program);
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    // Bind the transform feedback varyings to non-overlapping regions of the same buffer.
    GLBuffer buffer;
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer, 0, 4);
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, buffer, 4, 4);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 8, nullptr, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();
    // Two varyings bound to the same buffer should be an error.
    glBeginTransformFeedback(GL_POINTS);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Check the return type of a given parameter upon getting the active uniforms.
TEST_P(WebGL2CompatibilityTest, UniformVariablesReturnTypes)
{
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());

    std::vector<GLuint> validUniformIndices = {0};
    std::vector<GLint> uniformNameLengthBuf(validUniformIndices.size());

    // This should fail because GL_UNIFORM_NAME_LENGTH cannot be used in WebGL2.
    glGetActiveUniformsiv(program, static_cast<GLsizei>(validUniformIndices.size()),
                          &validUniformIndices[0], GL_UNIFORM_NAME_LENGTH,
                          &uniformNameLengthBuf[0]);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(WebGLCompatibilityTest);

ANGLE_INSTANTIATE_TEST_ES3(WebGL2CompatibilityTest);
}  // namespace angle

//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebGPUDeviceTest.cpp:
//   Tests that the WebGPU backend creates its WebGPU device with what the adapter supports, and
//   that the GL limits derived from the device can be used.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#if defined(ANGLE_ENABLE_WGPU)

#    include <dawn/native/DawnNative.h>
#    include <webgpu/webgpu.h>

#    include <sstream>

using namespace angle;

namespace
{

class WebGPUDeviceTest : public ANGLETest<>
{
  protected:
    WebGPUDeviceTest()
    {
        setWindowWidth(16);
        setWindowHeight(16);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    const DawnProcTable &wgpu() const { return dawn::native::GetProcs(); }

    EGLAttrib queryDeviceAttrib(EGLint attribute)
    {
        EXPECT_TRUE(IsEGLClientExtensionEnabled("EGL_EXT_device_query"));
        EGLAttrib eglDevice = 0;
        EXPECT_EGL_TRUE(
            eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &eglDevice));
        EXPECT_TRUE(IsEGLDeviceExtensionEnabled(reinterpret_cast<EGLDeviceEXT>(eglDevice),
                                                "EGL_ANGLE_device_webgpu"));
        EGLAttrib value = 0;
        EXPECT_EGL_TRUE(
            eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(eglDevice), attribute, &value));
        return value;
    }

    // The limits of the adapter that the backend created its device from.
    WGPULimits getAdapterLimits()
    {
        WGPUAdapter adapter =
            reinterpret_cast<WGPUAdapter>(queryDeviceAttrib(EGL_WEBGPU_ADAPTER_ANGLE));
        WGPULimits limits = WGPU_LIMITS_INIT;
        if (adapter)
        {
            EXPECT_EQ(WGPUStatus_Success, wgpu().adapterGetLimits(adapter, &limits));
            wgpu().adapterRelease(adapter);
        }
        return limits;
    }

    // The limits of the backend's device.
    WGPULimits getDeviceLimits()
    {
        WGPUDevice device =
            reinterpret_cast<WGPUDevice>(queryDeviceAttrib(EGL_WEBGPU_DEVICE_ANGLE));
        WGPULimits limits = WGPU_LIMITS_INIT;
        if (device)
        {
            EXPECT_EQ(WGPUStatus_Success, wgpu().deviceGetLimits(device, &limits));
            wgpu().deviceRelease(device);
        }
        return limits;
    }

    // Renders red to the last texel and green to the others of a width x height RGBA8 texture,
    // and checks the result by reading it back and by sampling it.
    void testRenderToAndSampleTexture(GLsizei width, GLsizei height)
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        constexpr char kMarkLastTexelFS[] = R"(precision highp float;
uniform vec2 lastTexel;
void main()
{
    gl_FragColor = all(equal(floor(gl_FragCoord.xy), lastTexel)) ? vec4(1, 0, 0, 1)
                                                                 : vec4(0, 1, 0, 1);
})";
        ANGLE_GL_PROGRAM(markLastTexel, essl1_shaders::vs::Simple(), kMarkLastTexelFS);
        glUseProgram(markLastTexel);
        glUniform2f(glGetUniformLocation(markLastTexel, "lastTexel"), static_cast<float>(width - 1),
                    static_cast<float>(height - 1));
        glViewport(0, 0, width, height);
        drawQuad(markLastTexel, essl1_shaders::PositionAttrib(), 0.5f);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(width - 1, height - 1, GLColor::red);
        if (width > 1)
        {
            EXPECT_PIXEL_COLOR_EQ(width - 2, height - 1, GLColor::green);
        }
        if (height > 1)
        {
            EXPECT_PIXEL_COLOR_EQ(width - 1, height - 2, GLColor::green);
        }

        constexpr char kSampleTexelFS[] = R"(precision highp float;
uniform sampler2D tex;
uniform vec2 texel;
uniform vec2 size;
void main()
{
    gl_FragColor = texture2D(tex, (texel + 0.5) / size);
})";
        ANGLE_GL_PROGRAM(sampleTexel, essl1_shaders::vs::Simple(), kSampleTexelFS);
        glUseProgram(sampleTexel);
        glUniform1i(glGetUniformLocation(sampleTexel, "tex"), 0);
        glUniform2f(glGetUniformLocation(sampleTexel, "size"), static_cast<float>(width),
                    static_cast<float>(height));
        GLint texelLocation = glGetUniformLocation(sampleTexel, "texel");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, getWindowWidth(), getWindowHeight());

        glUniform2f(texelLocation, static_cast<float>(width - 1), static_cast<float>(height - 1));
        drawQuad(sampleTexel, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::red);

        glUniform2f(texelLocation, 0.0f, 0.0f);
        drawQuad(sampleTexel, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::green);
        ASSERT_GL_NO_ERROR();
    }
};

class WebGPUDeviceTestES3 : public WebGPUDeviceTest
{
  protected:
    // Draws green to the default framebuffer if the texel (0, 0) of the given unsigned integer
    // texture is the expected value, and red if it isn't.
    void drawUintTexelMatches(GLuint texture, const GLuint expected[4])
    {
        constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform highp usampler2D tex;
uniform highp uvec4 expected;
out vec4 color;
void main()
{
    uvec4 texel = texelFetch(tex, ivec2(0), 0);
    color = texel == expected ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
})";
        ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(program, "tex"), 0);
        glUniform4uiv(glGetUniformLocation(program, "expected"), 1, expected);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, getWindowWidth(), getWindowHeight());
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
    }
};

// Tests that the backend creates its device with the limits of the adapter rather than with the
// WebGPU default limits.
TEST_P(WebGPUDeviceTest, DeviceHasAdapterLimits)
{
    const WGPULimits adapterLimits = getAdapterLimits();
    const WGPULimits deviceLimits  = getDeviceLimits();

    EXPECT_EQ(adapterLimits.maxTextureDimension1D, deviceLimits.maxTextureDimension1D);
    EXPECT_EQ(adapterLimits.maxTextureDimension2D, deviceLimits.maxTextureDimension2D);
    EXPECT_EQ(adapterLimits.maxTextureDimension3D, deviceLimits.maxTextureDimension3D);
    EXPECT_EQ(adapterLimits.maxTextureArrayLayers, deviceLimits.maxTextureArrayLayers);
    EXPECT_EQ(adapterLimits.maxBindGroups, deviceLimits.maxBindGroups);
    EXPECT_EQ(adapterLimits.maxBindingsPerBindGroup, deviceLimits.maxBindingsPerBindGroup);
    EXPECT_EQ(adapterLimits.maxSampledTexturesPerShaderStage,
              deviceLimits.maxSampledTexturesPerShaderStage);
    EXPECT_EQ(adapterLimits.maxSamplersPerShaderStage, deviceLimits.maxSamplersPerShaderStage);
    EXPECT_EQ(adapterLimits.maxUniformBuffersPerShaderStage,
              deviceLimits.maxUniformBuffersPerShaderStage);
    EXPECT_EQ(adapterLimits.maxUniformBufferBindingSize, deviceLimits.maxUniformBufferBindingSize);
    EXPECT_EQ(adapterLimits.maxStorageBufferBindingSize, deviceLimits.maxStorageBufferBindingSize);
    EXPECT_EQ(adapterLimits.maxBufferSize, deviceLimits.maxBufferSize);
    EXPECT_EQ(adapterLimits.maxVertexBuffers, deviceLimits.maxVertexBuffers);
    EXPECT_EQ(adapterLimits.maxVertexAttributes, deviceLimits.maxVertexAttributes);
    EXPECT_EQ(adapterLimits.maxVertexBufferArrayStride, deviceLimits.maxVertexBufferArrayStride);
    EXPECT_EQ(adapterLimits.maxInterStageShaderVariables,
              deviceLimits.maxInterStageShaderVariables);
    EXPECT_EQ(adapterLimits.maxColorAttachments, deviceLimits.maxColorAttachments);
    EXPECT_EQ(adapterLimits.maxColorAttachmentBytesPerSample,
              deviceLimits.maxColorAttachmentBytesPerSample);
}

// Tests that the backend creates its device with the optional features that the adapter supports
// and that the backend can use.
TEST_P(WebGPUDeviceTest, DeviceHasAdapterOptionalFeatures)
{
    constexpr std::pair<WGPUFeatureName, const char *> kFeatures[] = {
        {WGPUFeatureName_CoreFeaturesAndLimits, "core-features-and-limits"},
        {WGPUFeatureName_DepthClipControl, "depth-clip-control"},
        {WGPUFeatureName_Depth32FloatStencil8, "depth32float-stencil8"},
        {WGPUFeatureName_TextureCompressionBC, "texture-compression-bc"},
        {WGPUFeatureName_TextureCompressionBCSliced3D, "texture-compression-bc-sliced-3d"},
        {WGPUFeatureName_TextureCompressionETC2, "texture-compression-etc2"},
        {WGPUFeatureName_TextureCompressionASTC, "texture-compression-astc"},
        {WGPUFeatureName_TextureCompressionASTCSliced3D, "texture-compression-astc-sliced-3d"},
        {WGPUFeatureName_TimestampQuery, "timestamp-query"},
        {WGPUFeatureName_IndirectFirstInstance, "indirect-first-instance"},
        {WGPUFeatureName_ShaderF16, "shader-f16"},
        {WGPUFeatureName_RG11B10UfloatRenderable, "rg11b10ufloat-renderable"},
        {WGPUFeatureName_Float32Filterable, "float32-filterable"},
        {WGPUFeatureName_Float32Blendable, "float32-blendable"},
        {WGPUFeatureName_ClipDistances, "clip-distances"},
        {WGPUFeatureName_DualSourceBlending, "dual-source-blending"},
        {WGPUFeatureName_TextureFormatsTier1, "texture-formats-tier1"},
        {WGPUFeatureName_TextureComponentSwizzle, "texture-component-swizzle"},
    };

    WGPUAdapter adapter =
        reinterpret_cast<WGPUAdapter>(queryDeviceAttrib(EGL_WEBGPU_ADAPTER_ANGLE));
    WGPUDevice device = reinterpret_cast<WGPUDevice>(queryDeviceAttrib(EGL_WEBGPU_DEVICE_ANGLE));
    ASSERT_NE(adapter, nullptr);
    ASSERT_NE(device, nullptr);

    for (const auto &[feature, name] : kFeatures)
    {
        EXPECT_EQ(wgpu().adapterHasFeature(adapter, feature),
                  wgpu().deviceHasFeature(device, feature))
            << name;
    }

    wgpu().deviceRelease(device);
    wgpu().adapterRelease(adapter);
}

// Tests rendering to and sampling a texture as wide and as high as the adapter supports.
TEST_P(WebGPUDeviceTest, RenderToAndSampleTextureOfAdapterMaxSize)
{
    const GLsizei maxSize = static_cast<GLsizei>(getAdapterLimits().maxTextureDimension2D);

    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    EXPECT_GE(maxTextureSize, maxSize);
    GLint maxRenderbufferSize = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    EXPECT_GE(maxRenderbufferSize, maxSize);

    testRenderToAndSampleTexture(maxSize, 1);
    testRenderToAndSampleTexture(1, maxSize);
}

// Tests a program that uses as many vec4 varyings as the adapter supports.
TEST_P(WebGPUDeviceTest, AdapterMaxVaryings)
{
    // The front end supports at most 32 varying vectors.
    constexpr uint32_t kFrontEndMaxVaryingVectors = 32;
    const uint32_t varyingCount =
        std::min(getAdapterLimits().maxInterStageShaderVariables, kFrontEndMaxVaryingVectors);

    // The backend advertises every inter-stage shader variable of the device as a varying vector,
    // and reserves none of them for built-ins.
    GLint maxVaryingVectors = 0;
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);
    EXPECT_GE(static_cast<uint32_t>(maxVaryingVectors), varyingCount);

    std::stringstream varyings;
    std::stringstream vsBody;
    std::stringstream fsBody;
    for (uint32_t i = 0; i < varyingCount; ++i)
    {
        varyings << "varying vec4 v" << i << ";\n";
        vsBody << "    v" << i << " = vec4(" << i + 1 << ".0, " << i + 2 << ".0, " << i + 3
               << ".0, " << i + 4 << ".0);\n";
        fsBody << "    ok = ok && all(lessThan(abs(v" << i << " - vec4(" << i + 1 << ".0, " << i + 2
               << ".0, " << i + 3 << ".0, " << i + 4 << ".0)), vec4(0.01)));\n";
    }

    const std::string vs = "attribute vec4 a_position;\n" + varyings.str() +
                           "void main()\n{\n    gl_Position = a_position;\n" + vsBody.str() + "}\n";
    const std::string fs = "precision highp float;\n" + varyings.str() +
                           "void main()\n{\n    bool ok = true;\n" + fsBody.str() +
                           "    gl_FragColor = ok ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n}\n";

    ANGLE_GL_PROGRAM(program, vs.c_str(), fs.c_str());
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::green);
}

// Tests rendering to and sampling the last layer of a 2D array texture with as many layers as the
// adapter supports.
TEST_P(WebGPUDeviceTestES3, RenderToAndSampleLastLayerOfAdapterMaxArrayTexture)
{
    const GLsizei layerCount = static_cast<GLsizei>(getAdapterLimits().maxTextureArrayLayers);

    GLint maxArrayTextureLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
    EXPECT_GE(maxArrayTextureLayers, layerCount);

    constexpr GLsizei kSize = 4;
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, kSize, kSize, layerCount);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(drawColor);
    GLint colorLocation = glGetUniformLocation(drawColor, essl1_shaders::ColorUniform());

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, kSize, kSize);
    const std::pair<GLint, GLColor> layerColors[] = {{0, GLColor::green},
                                                     {layerCount - 1, GLColor::red}};
    for (const auto &[layer, color] : layerColors)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, layer);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
        glUniform4fv(colorLocation, 1, color.toNormalizedVector().data());
        drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
        ASSERT_GL_NO_ERROR();
    }
    for (const auto &[layer, color] : layerColors)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, layer);
        EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, color);
    }

    constexpr char kSampleLayerFS[] = R"(#version 300 es
precision highp float;
uniform highp sampler2DArray tex;
uniform float layer;
out vec4 fragColor;
void main()
{
    fragColor = texture(tex, vec3(0.5, 0.5, layer));
})";
    ANGLE_GL_PROGRAM(sampleLayer, essl3_shaders::vs::Simple(), kSampleLayerFS);
    glUseProgram(sampleLayer);
    glUniform1i(glGetUniformLocation(sampleLayer, "tex"), 0);
    GLint layerLocation = glGetUniformLocation(sampleLayer, "layer");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    for (const auto &[layer, color] : layerColors)
    {
        glUniform1f(layerLocation, static_cast<float>(layer));
        drawQuad(sampleLayer, essl3_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), color);
    }
    ASSERT_GL_NO_ERROR();
}

// Tests uploading and sampling an RGB32F texture, which the backend implements with an RGBA32Float
// WebGPU texture.
TEST_P(WebGPUDeviceTestES3, RGB32FTexture)
{
    constexpr GLfloat kFloatData[] = {0.25f, 0.5f, 0.75f};
    GLTexture floatTexture;
    glBindTexture(GL_TEXTURE_2D, floatTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, kFloatData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    constexpr char kSampleFloatFS[] = R"(#version 300 es
precision highp float;
uniform highp sampler2D tex;
out vec4 color;
void main()
{
    color = texture(tex, vec2(0.5));
})";
    ANGLE_GL_PROGRAM(sampleFloat, essl3_shaders::vs::Simple(), kSampleFloatFS);
    drawQuad(sampleFloat, essl3_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(64, 128, 191, 255), 1);
}

// Tests uploading and sampling an RGB32UI texture, which the backend implements with an RGBA32Uint
// WebGPU texture. The alpha of the texels is the integer one.
TEST_P(WebGPUDeviceTestES3, RGB32UITexture)
{
    constexpr GLuint kData[] = {1u, 0x12345678u, 0xFFFFFFFFu};
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, 1, 1, 0, GL_RGB_INTEGER, GL_UNSIGNED_INT, kData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    constexpr GLuint kExpected[] = {1u, 0x12345678u, 0xFFFFFFFFu, 1u};
    drawUintTexelMatches(texture, kExpected);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests uploading and sampling an RGB10_A2UI texture, which the backend implements with an
// RGB10A2Uint WebGPU texture.
TEST_P(WebGPUDeviceTestES3, RGB10A2UITexture)
{
    // Red 1023, green 512, blue 3 and alpha 2.
    constexpr GLuint kData = 1023u | (512u << 10) | (3u << 20) | (2u << 30);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2UI, 1, 1, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_INT_2_10_10_10_REV, &kData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    constexpr GLuint kExpected[] = {1023u, 512u, 3u, 2u};
    drawUintTexelMatches(texture, kExpected);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGPUDeviceTest);
ANGLE_INSTANTIATE_TEST(WebGPUDeviceTest, ES2_WEBGPU(), ES3_WEBGPU());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGPUDeviceTestES3);
ANGLE_INSTANTIATE_TEST(WebGPUDeviceTestES3, ES3_WEBGPU());

#endif  // defined(ANGLE_ENABLE_WGPU)

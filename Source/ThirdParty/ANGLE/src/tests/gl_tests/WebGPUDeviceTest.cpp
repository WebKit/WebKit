//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebGPUDeviceTest.cpp:
//   Tests that the WebGPU backend creates its WebGPU device with what the adapter supports, and
//   that the GL limits, formats and extensions derived from the device can be used.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#if defined(ANGLE_ENABLE_WGPU)

#    include <dawn/native/DawnNative.h>
#    include <webgpu/webgpu.h>

#    include <algorithm>
#    include <array>
#    include <functional>
#    include <sstream>
#    include <vector>

using namespace angle;

namespace
{

// The GL extensions that the backend enables only when the device has an optional WebGPU feature.
struct FeatureGatedExtension
{
    const char *extension;
    std::vector<WGPUFeatureName> features;
};
const FeatureGatedExtension kFeatureGatedExtensions[] = {
    {"GL_OES_texture_float_linear", {WGPUFeatureName_Float32Filterable}},
    {"GL_EXT_color_buffer_float", {WGPUFeatureName_RG11B10UfloatRenderable}},
    {"GL_EXT_float_blend",
     {WGPUFeatureName_Float32Blendable, WGPUFeatureName_RG11B10UfloatRenderable}},
    {"GL_EXT_texture_norm16", {WGPUFeatureName_TextureFormatsTier1}},
    {"GL_EXT_render_snorm", {WGPUFeatureName_TextureFormatsTier1}},
};

// The ES 3.0 ETC2 and EAC formats, which need texture-compression-etc2.
constexpr GLenum kETC2Formats[] = {
    GL_COMPRESSED_R11_EAC,
    GL_COMPRESSED_SIGNED_R11_EAC,
    GL_COMPRESSED_RG11_EAC,
    GL_COMPRESSED_SIGNED_RG11_EAC,
    GL_COMPRESSED_RGB8_ETC2,
    GL_COMPRESSED_SRGB8_ETC2,
    GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    GL_COMPRESSED_RGBA8_ETC2_EAC,
    GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
};

// The features of the adapter that a WebGPU display created with HidingFeaturesProcs() does not
// see, so that its device is created without them.
std::vector<WGPUFeatureName> gHiddenAdapterFeatures;
WGPUProcAdapterHasFeature gAdapterHasFeature = nullptr;

WGPUBool AdapterHasFeatureHidingFeatures(WGPUAdapter adapter, WGPUFeatureName feature)
{
    if (std::find(gHiddenAdapterFeatures.begin(), gHiddenAdapterFeatures.end(), feature) !=
        gHiddenAdapterFeatures.end())
    {
        return false;
    }
    return gAdapterHasFeature(adapter, feature);
}

const DawnProcTable &HidingFeaturesProcs()
{
    static DawnProcTable procs = [] {
        DawnProcTable result     = dawn::native::GetProcs();
        gAdapterHasFeature       = result.adapterHasFeature;
        result.adapterHasFeature = AdapterHasFeatureHidingFeatures;
        return result;
    }();
    return procs;
}

// Creates a WebGPU EGL display from the given attributes and makes an ES 3 context of it current
// without a surface. The test's own context must be made current again afterwards.
class ScopedWebGPUDisplay final
{
  public:
    explicit ScopedWebGPUDisplay(std::vector<EGLAttrib> displayAttribs)
    {
        displayAttribs.insert(displayAttribs.begin(), {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                                                       EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE});
        displayAttribs.push_back(EGL_NONE);
        mDisplay = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                         reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY),
                                         displayAttribs.data());
        EXPECT_NE(mDisplay, EGL_NO_DISPLAY);
        EXPECT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));

        constexpr EGLint kContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                              EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE,
                                              EGL_FALSE, EGL_NONE};
        mContext = eglCreateContext(mDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, kContextAttribs);
        EXPECT_NE(mContext, EGL_NO_CONTEXT);
        EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext));
    }

    ~ScopedWebGPUDisplay()
    {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(mDisplay, mContext);
        eglTerminate(mDisplay);
    }

    bool deviceHasFeature(WGPUFeatureName feature) const
    {
        EGLAttrib eglDevice = 0;
        EXPECT_EGL_TRUE(eglQueryDisplayAttribEXT(mDisplay, EGL_DEVICE_EXT, &eglDevice));
        EGLAttrib device = 0;
        EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(eglDevice),
                                                EGL_WEBGPU_DEVICE_ANGLE, &device));
        const DawnProcTable &wgpu = dawn::native::GetProcs();
        bool hasFeature = wgpu.deviceHasFeature(reinterpret_cast<WGPUDevice>(device), feature);
        wgpu.deviceRelease(reinterpret_cast<WGPUDevice>(device));
        return hasFeature;
    }

  private:
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLContext mContext = EGL_NO_CONTEXT;
};

std::vector<GLenum> GetCompressedTextureFormats()
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
    std::vector<GLint> formats(count);
    if (count > 0)
    {
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats.data());
    }
    return std::vector<GLenum>(formats.begin(), formats.end());
}

bool HasAll(const std::vector<GLenum> &formats, const std::vector<GLenum> &expected)
{
    return std::all_of(expected.begin(), expected.end(), [&](GLenum format) {
        return std::find(formats.begin(), formats.end(), format) != formats.end();
    });
}

bool HasNone(const std::vector<GLenum> &formats, const std::vector<GLenum> &unexpected)
{
    return std::none_of(unexpected.begin(), unexpected.end(), [&](GLenum format) {
        return std::find(formats.begin(), formats.end(), format) != formats.end();
    });
}

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
    // Draws a quad with the given color to the current framebuffer, which is width x height.
    void drawUniformColor(const GLfloat color[4], GLsizei width, GLsizei height)
    {
        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);
        glUniform4fv(glGetUniformLocation(program, essl1_shaders::ColorUniform()), 1, color);
        glViewport(0, 0, width, height);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    }

    // Draws the red channel of the given texture, sampled at texture coordinate (s, 0.5), to the
    // whole default framebuffer.
    void drawTextureRed(GLuint texture, GLfloat s)
    {
        constexpr char kFS[] = R"(#version 300 es
precision highp float;
uniform highp sampler2D tex;
uniform float s;
out vec4 color;
void main()
{
    float red = texture(tex, vec2(s, 0.5)).r;
    color = vec4(red, red, red, 1.0);
})";
        ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(program, "tex"), 0);
        glUniform1f(glGetUniformLocation(program, "s"), s);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, getWindowWidth(), getWindowHeight());
        drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
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

// Tests linear filtering of the 32-bit float formats, which GL_OES_texture_float_linear enables
// with float32-filterable.
TEST_P(WebGPUDeviceTestES3, TextureFloatLinear)
{
    ASSERT_TRUE(IsGLExtensionEnabled("GL_OES_texture_float_linear"));

    // Two texels, 0 and 1 in every channel.
    constexpr GLfloat kData[] = {0, 0, 0, 0, 1, 1, 1, 1};
    struct Format
    {
        GLenum internalFormat;
        GLenum format;
        size_t componentCount;
    };
    constexpr Format kFormats[] = {{GL_R32F, GL_RED, 1},
                                   {GL_RG32F, GL_RG, 2},
                                   {GL_RGB32F, GL_RGB, 3},
                                   {GL_RGBA32F, GL_RGBA, 4}};
    for (const auto &[internalFormat, format, componentCount] : kFormats)
    {
        SCOPED_TRACE(testing::Message() << "internalFormat 0x" << std::hex << internalFormat);
        std::vector<GLfloat> data;
        for (size_t texel = 0; texel < 2; ++texel)
        {
            data.insert(data.end(), &kData[texel * 4], &kData[texel * 4 + componentCount]);
        }

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 2, 1, 0, format, GL_FLOAT, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ASSERT_GL_NO_ERROR();

        // Halfway between the texel centers.
        drawTextureRed(texture, 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(128, 128, 128, 255), 2);
    }
}

// Tests rendering to the float formats that GL_EXT_color_buffer_float makes renderable, including
// R11F_G11F_B10F, which needs rg11b10ufloat-renderable.
TEST_P(WebGPUDeviceTestES3, ColorBufferFloat)
{
    ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_color_buffer_float"));

    constexpr GLfloat kColor[]                         = {2.0f, 3.0f, 4.0f, 5.0f};
    constexpr std::pair<GLenum, GLColor32F> kFormats[] = {
        {GL_R16F, GLColor32F(2, 0, 0, 1)},           {GL_RG16F, GLColor32F(2, 3, 0, 1)},
        {GL_RGBA16F, GLColor32F(2, 3, 4, 5)},        {GL_R32F, GLColor32F(2, 0, 0, 1)},
        {GL_RG32F, GLColor32F(2, 3, 0, 1)},          {GL_RGBA32F, GLColor32F(2, 3, 4, 5)},
        {GL_R11F_G11F_B10F, GLColor32F(2, 3, 4, 1)},
    };
    for (const auto &[internalFormat, expected] : kFormats)
    {
        SCOPED_TRACE(testing::Message() << "internalFormat 0x" << std::hex << internalFormat);
        GLRenderbuffer renderbuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 4, 4);
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderbuffer);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        drawUniformColor(kColor, 4, 4);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR32F_NEAR(0, 0, expected, 0.01f);
    }
}

// Tests blending of a 32-bit float color buffer, which GL_EXT_float_blend enables with
// float32-blendable.
TEST_P(WebGPUDeviceTestES3, FloatBlend)
{
    ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_float_blend"));

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 4, 4);
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glClearColor(1.0f, 2.0f, 3.0f, 4.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    constexpr GLfloat kColor[] = {2.0f, 3.0f, 4.0f, 5.0f};
    drawUniformColor(kColor, 4, 4);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR32F_NEAR(0, 0, GLColor32F(3, 5, 7, 9), 0.01f);
}

// Tests sampling of and rendering to the 16-bit normalized formats of GL_EXT_texture_norm16, which
// need texture-formats-tier1.
TEST_P(WebGPUDeviceTestES3, TextureNorm16)
{
    ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_texture_norm16"));

    constexpr GLushort kUnormData[] = {0x8000, 0x4000, 0x2000, 0xFFFF};
    constexpr GLshort kSnormData[]  = {0x4000, -0x2000, 0x1000, 0x7FFF};
    struct Format
    {
        GLenum internalFormat;
        GLenum format;
        GLenum type;
        const void *data;
        GLColor expectedRed;
    };
    const Format kFormats[] = {
        {GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT, kUnormData, GLColor(128, 128, 128, 255)},
        {GL_RG16_EXT, GL_RG, GL_UNSIGNED_SHORT, kUnormData, GLColor(128, 128, 128, 255)},
        {GL_RGB16_EXT, GL_RGB, GL_UNSIGNED_SHORT, kUnormData, GLColor(128, 128, 128, 255)},
        {GL_RGBA16_EXT, GL_RGBA, GL_UNSIGNED_SHORT, kUnormData, GLColor(128, 128, 128, 255)},
        {GL_R16_SNORM_EXT, GL_RED, GL_SHORT, kSnormData, GLColor(128, 128, 128, 255)},
        {GL_RG16_SNORM_EXT, GL_RG, GL_SHORT, kSnormData, GLColor(128, 128, 128, 255)},
        {GL_RGB16_SNORM_EXT, GL_RGB, GL_SHORT, kSnormData, GLColor(128, 128, 128, 255)},
        {GL_RGBA16_SNORM_EXT, GL_RGBA, GL_SHORT, kSnormData, GLColor(128, 128, 128, 255)},
    };
    for (const Format &format : kFormats)
    {
        SCOPED_TRACE(testing::Message()
                     << "internalFormat 0x" << std::hex << format.internalFormat);
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, format.internalFormat, 1, 1, 0, format.format, format.type,
                     format.data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        ASSERT_GL_NO_ERROR();

        drawTextureRed(texture, 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_NEAR(0, 0, format.expectedRed, 1);
    }

    GLTexture renderTexture;
    glBindTexture(GL_TEXTURE_2D, renderTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16_EXT, 4, 4);
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    constexpr GLfloat kColor[] = {0.5f, 0.25f, 0.125f, 1.0f};
    drawUniformColor(kColor, 4, 4);
    ASSERT_GL_NO_ERROR();
    GLushort pixel[4] = {};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, pixel);
    ASSERT_GL_NO_ERROR();
    EXPECT_NEAR(pixel[0], 0x8000, 1);
    EXPECT_NEAR(pixel[1], 0x4000, 1);
    EXPECT_NEAR(pixel[2], 0x2000, 1);
    EXPECT_EQ(pixel[3], 0xFFFF);
}

// Tests rendering to the signed normalized formats, which GL_EXT_render_snorm enables with
// texture-formats-tier1.
TEST_P(WebGPUDeviceTestES3, RenderSnorm)
{
    ASSERT_TRUE(IsGLExtensionEnabled("GL_EXT_render_snorm"));

    constexpr GLfloat kColor[]                                    = {-0.5f, 0.5f, -1.0f, 1.0f};
    constexpr std::pair<GLenum, std::array<GLbyte, 4>> kFormats[] = {
        {GL_R8_SNORM, {-64, 0, 0, 127}},
        {GL_RG8_SNORM, {-64, 64, 0, 127}},
        {GL_RGBA8_SNORM, {-64, 64, -127, 127}},
    };
    for (const auto &[internalFormat, expected] : kFormats)
    {
        SCOPED_TRACE(testing::Message() << "internalFormat 0x" << std::hex << internalFormat);
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 4, 4);
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        drawUniformColor(kColor, 4, 4);
        ASSERT_GL_NO_ERROR();
        GLbyte pixel[4] = {};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_BYTE, pixel);
        ASSERT_GL_NO_ERROR();
        for (size_t channel = 0; channel < 4; ++channel)
        {
            EXPECT_NEAR(pixel[channel], expected[channel], 1) << "channel " << channel;
        }
    }
}

// Checks the GL extensions and the ETC2 formats that need an optional WebGPU feature against the
// features of the device of the current context. The ETC2 formats are checked in the format queries
// only, because ES 3.0 accepts ETC2 uploads whether or not the formats are in the caps.
void ExpectFeatureGatedExtensions(const std::function<bool(WGPUFeatureName)> &deviceHasFeature)
{
    for (const FeatureGatedExtension &gated : kFeatureGatedExtensions)
    {
        const bool deviceHasFeatures =
            std::all_of(gated.features.begin(), gated.features.end(), deviceHasFeature);
        EXPECT_EQ(IsGLExtensionEnabled(gated.extension), deviceHasFeatures) << gated.extension;
    }

    const std::vector<GLenum> etc2Formats(std::begin(kETC2Formats), std::end(kETC2Formats));
    const std::vector<GLenum> compressedFormats = GetCompressedTextureFormats();
    EXPECT_GL_NO_ERROR();
    if (deviceHasFeature(WGPUFeatureName_TextureCompressionETC2))
    {
        EXPECT_TRUE(HasAll(compressedFormats, etc2Formats));
    }
    else
    {
        EXPECT_TRUE(HasNone(compressedFormats, etc2Formats));
    }
}

// Tests that the GL extensions and the ES 3.0 formats that need an optional WebGPU feature are
// exposed exactly when the device has the feature.
TEST_P(WebGPUDeviceTestES3, FeatureGatedExtensionsFollowDeviceFeatures)
{
    WGPUDevice device = reinterpret_cast<WGPUDevice>(queryDeviceAttrib(EGL_WEBGPU_DEVICE_ANGLE));
    ASSERT_NE(device, nullptr);
    ExpectFeatureGatedExtensions(
        [&](WGPUFeatureName feature) { return wgpu().deviceHasFeature(device, feature); });
    wgpu().deviceRelease(device);
}

// Tests that the GL extensions and the ES 3.0 formats that need an optional WebGPU feature are not
// exposed when the device lacks the feature, one feature at a time. The device lacks the feature
// because the backend's adapter doesn't report it.
TEST_P(WebGPUDeviceTestES3, FeatureGatedExtensionsWithoutTheirFeature)
{
    std::vector<WGPUFeatureName> features = {WGPUFeatureName_TextureCompressionETC2};
    for (const FeatureGatedExtension &gated : kFeatureGatedExtensions)
    {
        for (WGPUFeatureName feature : gated.features)
        {
            if (std::find(features.begin(), features.end(), feature) == features.end())
            {
                features.push_back(feature);
            }
        }
    }

    for (WGPUFeatureName hiddenFeature : features)
    {
        gHiddenAdapterFeatures = {hiddenFeature};
        {
            ScopedWebGPUDisplay display({EGL_PLATFORM_ANGLE_DAWN_PROC_TABLE_ANGLE,
                                         reinterpret_cast<EGLAttrib>(&HidingFeaturesProcs())});
            SCOPED_TRACE(testing::Message() << "Hidden feature " << hiddenFeature);
            EXPECT_FALSE(display.deviceHasFeature(hiddenFeature));
            ExpectFeatureGatedExtensions(
                [&](WGPUFeatureName feature) { return display.deviceHasFeature(feature); });
        }
        gHiddenAdapterFeatures.clear();
        ASSERT_TRUE(getEGLWindow()->makeCurrent());
    }
}

// Tests that the GL extensions and the ES 3.0 formats that need an optional WebGPU feature are not
// exposed for a device that the embedder creates without optional features.
TEST_P(WebGPUDeviceTestES3, FeatureGatedExtensionsWithEmbedderDeviceWithoutFeatures)
{
    // The backend's adapter has already created its device, so create the device from another
    // adapter of the same instance.
    WGPUAdapter backendAdapter =
        reinterpret_cast<WGPUAdapter>(queryDeviceAttrib(EGL_WEBGPU_ADAPTER_ANGLE));
    ASSERT_NE(backendAdapter, nullptr);
    WGPUInstance instance = wgpu().adapterGetInstance(backendAdapter);
    wgpu().adapterRelease(backendAdapter);

    WGPUAdapter adapter                            = nullptr;
    WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterCallback.mode                           = WGPUCallbackMode_WaitAnyOnly;
    adapterCallback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter result,
                                  WGPUStringView, void *userdata1, void *) {
        if (status == WGPURequestAdapterStatus_Success)
        {
            *static_cast<WGPUAdapter *>(userdata1) = result;
        }
    };
    adapterCallback.userdata1                = &adapter;
    WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    WGPUFutureWaitInfo futureWaitInfo        = {};
    futureWaitInfo.future =
        wgpu().instanceRequestAdapter(instance, &adapterOptions, adapterCallback);
    wgpu().instanceWaitAny(instance, 1, &futureWaitInfo, UINT64_MAX);
    ASSERT_NE(adapter, nullptr);

    WGPUDevice device                            = nullptr;
    WGPURequestDeviceCallbackInfo deviceCallback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceCallback.mode                          = WGPUCallbackMode_WaitAnyOnly;
    deviceCallback.callback = [](WGPURequestDeviceStatus status, WGPUDevice result, WGPUStringView,
                                 void *userdata1, void *) {
        if (status == WGPURequestDeviceStatus_Success)
        {
            *static_cast<WGPUDevice *>(userdata1) = result;
        }
    };
    deviceCallback.userdata1        = &device;
    WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
    futureWaitInfo.future = wgpu().adapterRequestDevice(adapter, &deviceDesc, deviceCallback);
    wgpu().instanceWaitAny(instance, 1, &futureWaitInfo, UINT64_MAX);
    wgpu().adapterRelease(adapter);
    wgpu().instanceRelease(instance);
    ASSERT_NE(device, nullptr);

    {
        ScopedWebGPUDisplay display(
            {EGL_PLATFORM_ANGLE_WEBGPU_DEVICE_ANGLE, reinterpret_cast<EGLAttrib>(device)});
        EXPECT_FALSE(display.deviceHasFeature(WGPUFeatureName_TextureCompressionETC2));
        for (const FeatureGatedExtension &gated : kFeatureGatedExtensions)
        {
            for (WGPUFeatureName feature : gated.features)
            {
                EXPECT_FALSE(display.deviceHasFeature(feature)) << gated.extension;
            }
        }
        ExpectFeatureGatedExtensions([](WGPUFeatureName) { return false; });
    }
    wgpu().deviceRelease(device);
    ASSERT_TRUE(getEGLWindow()->makeCurrent());
}

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGPUDeviceTest);
ANGLE_INSTANTIATE_TEST(WebGPUDeviceTest, ES2_WEBGPU(), ES3_WEBGPU());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebGPUDeviceTestES3);
ANGLE_INSTANTIATE_TEST(WebGPUDeviceTestES3, ES3_WEBGPU());

#endif  // defined(ANGLE_ENABLE_WGPU)

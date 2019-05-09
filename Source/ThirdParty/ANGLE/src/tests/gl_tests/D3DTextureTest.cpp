//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// D3DTextureTest:
//   Tests of the EGL_ANGLE_d3d_texture_client_buffer extension

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <d3d11.h>
#include <windows.h>

#include "util/EGLWindow.h"
#include "util/com_utils.h"

namespace angle
{

class D3DTextureTest : public ANGLETest
{
  protected:
    D3DTextureTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        constexpr char kVS[] =
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = position;
                texcoord = (position.xy * 0.5) + 0.5;
                texcoord.y = 1.0 - texcoord.y;
            })";

        constexpr char kTextureFS[] =
            R"(precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            })";

        constexpr char kTextureFSNoSampling[] =
            R"(precision highp float;

            void main()
            {
                gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
            })";

        mTextureProgram = CompileProgram(kVS, kTextureFS);
        ASSERT_NE(0u, mTextureProgram) << "shader compilation failed.";

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");
        ASSERT_NE(-1, mTextureUniformLocation);

        mTextureProgramNoSampling = CompileProgram(kVS, kTextureFSNoSampling);
        ASSERT_NE(0u, mTextureProgramNoSampling) << "shader compilation failed.";

        mD3D11Module = LoadLibrary(TEXT("d3d11.dll"));
        ASSERT_NE(nullptr, mD3D11Module);

        PFN_D3D11_CREATE_DEVICE createDeviceFunc = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
            GetProcAddress(mD3D11Module, "D3D11CreateDevice"));

        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        if (IsEGLDisplayExtensionEnabled(display, "EGL_EXT_device_query"))
        {
            PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT =
                reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
                    eglGetProcAddress("eglQueryDisplayAttribEXT"));
            PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT =
                reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
                    eglGetProcAddress("eglQueryDeviceAttribEXT"));

            EGLDeviceEXT device = 0;
            {
                EGLAttrib result = 0;
                EXPECT_EGL_TRUE(eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, &result));
                device = reinterpret_cast<EGLDeviceEXT>(result);
            }

            if (IsEGLDeviceExtensionEnabled(device, "EGL_ANGLE_device_d3d"))
            {
                EGLAttrib result = 0;
                if (eglQueryDeviceAttribEXT(device, EGL_D3D11_DEVICE_ANGLE, &result))
                {
                    mD3D11Device = reinterpret_cast<ID3D11Device *>(result);
                    mD3D11Device->AddRef();
                }
                else if (eglQueryDeviceAttribEXT(device, EGL_D3D9_DEVICE_ANGLE, &result))
                {
                    mD3D9Device = reinterpret_cast<IDirect3DDevice9 *>(result);
                    mD3D9Device->AddRef();
                }
            }
        }
        else
        {
            ASSERT_TRUE(
                SUCCEEDED(createDeviceFunc(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                                           0, D3D11_SDK_VERSION, &mD3D11Device, nullptr, nullptr)));
        }
    }

    void TearDown() override
    {
        glDeleteProgram(mTextureProgram);

        if (mD3D11Device)
        {
            mD3D11Device->Release();
            mD3D11Device = nullptr;
        }

        FreeLibrary(mD3D11Module);
        mD3D11Module = nullptr;

        if (mD3D9Device)
        {
            mD3D9Device->Release();
            mD3D9Device = nullptr;
        }

        ANGLETest::TearDown();
    }

    EGLSurface createD3D11PBuffer(size_t width,
                                  size_t height,
                                  UINT sampleCount,
                                  UINT sampleQuality,
                                  UINT bindFlags,
                                  DXGI_FORMAT format,
                                  const EGLint *attribs)
    {
        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        EGLConfig config   = window->getConfig();

        EXPECT_TRUE(mD3D11Device != nullptr);
        ID3D11Texture2D *texture = nullptr;
        CD3D11_TEXTURE2D_DESC desc(format, static_cast<UINT>(width), static_cast<UINT>(height), 1,
                                   1, bindFlags);
        desc.SampleDesc.Count   = sampleCount;
        desc.SampleDesc.Quality = sampleQuality;
        EXPECT_TRUE(SUCCEEDED(mD3D11Device->CreateTexture2D(&desc, nullptr, &texture)));

        EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE,
                                                              texture, config, attribs);

        texture->Release();

        return pbuffer;
    }

    EGLSurface createD3D11PBuffer(size_t width,
                                  size_t height,
                                  EGLint eglTextureFormat,
                                  EGLint eglTextureTarget,
                                  UINT sampleCount,
                                  UINT sampleQuality,
                                  UINT bindFlags,
                                  DXGI_FORMAT format)
    {
        EGLint attribs[] = {
            EGL_TEXTURE_FORMAT, eglTextureFormat, EGL_TEXTURE_TARGET,
            eglTextureTarget,   EGL_NONE,         EGL_NONE,
        };
        return createD3D11PBuffer(width, height, sampleCount, sampleQuality, bindFlags, format,
                                  attribs);
    }

    EGLSurface createPBuffer(size_t width,
                             size_t height,
                             EGLint eglTextureFormat,
                             EGLint eglTextureTarget,
                             UINT sampleCount,
                             UINT sampleQuality)
    {
        if (mD3D11Device)
        {
            return createD3D11PBuffer(
                width, height, eglTextureFormat, eglTextureTarget, sampleCount, sampleQuality,
                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_R8G8B8A8_UNORM);
        }

        if (mD3D9Device)
        {
            EGLWindow *window  = getEGLWindow();
            EGLDisplay display = window->getDisplay();
            EGLConfig config   = window->getConfig();

            EGLint attribs[] = {
                EGL_TEXTURE_FORMAT, eglTextureFormat, EGL_TEXTURE_TARGET,
                eglTextureTarget,   EGL_NONE,         EGL_NONE,
            };

            // Multisampled textures are not supported on D3D9.
            EXPECT_TRUE(sampleCount <= 1);
            EXPECT_TRUE(sampleQuality == 0);

            IDirect3DTexture9 *texture = nullptr;
            EXPECT_TRUE(SUCCEEDED(mD3D9Device->CreateTexture(
                static_cast<UINT>(width), static_cast<UINT>(height), 1, D3DUSAGE_RENDERTARGET,
                D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr)));

            EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE,
                                                                  texture, config, attribs);

            texture->Release();

            return pbuffer;
        }
        else
        {
            return EGL_NO_SURFACE;
        }
    }

    bool valid() const
    {
        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_d3d_texture_client_buffer"))
        {
            std::cout << "Test skipped due to missing EGL_ANGLE_d3d_texture_client_buffer"
                      << std::endl;
            return false;
        }

        if (!mD3D11Device && !mD3D9Device)
        {
            std::cout << "Test skipped due to no D3D devices being available." << std::endl;
            return false;
        }

        if (IsWindows() && IsAMD() && IsOpenGL())
        {
            std::cout << "Test skipped on Windows AMD OpenGL." << std::endl;
            return false;
        }

        if (IsWindows() && IsIntel() && IsOpenGL())
        {
            std::cout << "Test skipped on Windows Intel OpenGL." << std::endl;
            return false;
        }
        return true;
    }

    void testTextureSamplesAs50PercentGreen(GLuint texture)
    {
        GLFramebuffer scratchFbo;
        glBindFramebuffer(GL_FRAMEBUFFER, scratchFbo);
        GLTexture scratchTexture;
        glBindTexture(GL_TEXTURE_2D, scratchTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scratchTexture,
                               0);

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(mTextureProgram);
        glUniform1i(mTextureUniformLocation, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        drawQuad(mTextureProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 127u, 0u, 255u), 2);
    }

    GLuint mTextureProgram;
    GLuint mTextureProgramNoSampling;
    GLint mTextureUniformLocation;

    HMODULE mD3D11Module       = nullptr;
    ID3D11Device *mD3D11Device = nullptr;

    IDirect3DDevice9 *mD3D9Device = nullptr;
};

// Test creating pbuffer from textures with several different DXGI formats.
TEST_P(D3DTextureTest, TestD3D11SupportedFormatsSurface)
{
    bool srgbSupported = IsGLExtensionEnabled("GL_EXT_sRGB") || getClientMajorVersion() == 3;
    ANGLE_SKIP_TEST_IF(!valid() || !mD3D11Device || !srgbSupported);

    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                   DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB};
    for (size_t i = 0; i < 4; ++i)
    {
        if (formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            if (IsOpenGL())
            {
                // This generates an invalid format error when calling wglDXRegisterObjectNV().
                // Reproducible at least on NVIDIA driver 390.65 on Windows 10.
                std::cout << "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB subtest skipped: IsOpenGL().\n";
                continue;
            }
        }

        EGLSurface pbuffer = createD3D11PBuffer(32, 32, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 1, 0,
                                                D3D11_BIND_RENDER_TARGET, formats[i]);
        ASSERT_EGL_SUCCESS();
        ASSERT_NE(EGL_NO_SURFACE, pbuffer);

        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();

        EGLint colorspace = EGL_NONE;
        eglQuerySurface(display, pbuffer, EGL_GL_COLORSPACE, &colorspace);

        if (formats[i] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
            formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            EXPECT_EQ(EGL_GL_COLORSPACE_SRGB, colorspace);
        }
        else
        {
            EXPECT_EQ(EGL_GL_COLORSPACE_LINEAR, colorspace);
        }

        eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
        ASSERT_EGL_SUCCESS();
        window->makeCurrent();
        eglDestroySurface(display, pbuffer);
    }
}

// Test binding a pbuffer created from a D3D texture as a texture image with several different DXGI
// formats. The test renders to and samples from the pbuffer.
TEST_P(D3DTextureTest, TestD3D11SupportedFormatsTexture)
{
    bool srgb8alpha8TextureAttachmentSupported = getClientMajorVersion() >= 3;
    ANGLE_SKIP_TEST_IF(!valid() || !mD3D11Device || !srgb8alpha8TextureAttachmentSupported);

    bool srgbWriteControlSupported =
        IsGLExtensionEnabled("GL_EXT_sRGB_write_control") && !IsOpenGL();

    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                                   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB};
    for (size_t i = 0; i < 4; ++i)
    {
        if (formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            if (IsOpenGL())
            {
                // This generates an invalid format error when calling wglDXRegisterObjectNV().
                // Reproducible at least on NVIDIA driver 390.65 on Windows 10.
                std::cout << "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB subtest skipped: IsOpenGL().\n";
                continue;
            }
        }

        SCOPED_TRACE(std::string("Test case:") + std::to_string(i));
        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();

        EGLSurface pbuffer =
            createD3D11PBuffer(32, 32, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 1, 0,
                               D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, formats[i]);
        ASSERT_EGL_SUCCESS();
        ASSERT_NE(EGL_NO_SURFACE, pbuffer);

        EGLint colorspace = EGL_NONE;
        eglQuerySurface(display, pbuffer, EGL_GL_COLORSPACE, &colorspace);

        GLuint texture = 0u;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        EGLBoolean result = eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
        ASSERT_EGL_SUCCESS();
        ASSERT_EGL_TRUE(result);

        GLuint fbo = 0u;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glViewport(0, 0, 32, 32);

        GLint colorEncoding = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT,
                                              &colorEncoding);

        if (formats[i] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
            formats[i] == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            EXPECT_EQ(EGL_GL_COLORSPACE_SRGB, colorspace);
            EXPECT_EQ(GL_SRGB_EXT, colorEncoding);
        }
        else
        {
            EXPECT_EQ(EGL_GL_COLORSPACE_LINEAR, colorspace);
            EXPECT_EQ(GL_LINEAR, colorEncoding);
        }

        // Clear the texture with 50% green and check that the color value written is correct.
        glClearColor(0.0f, 0.5f, 0.0f, 1.0f);

        if (colorEncoding == GL_SRGB_EXT)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 188u, 0u, 255u), 2);
            // Disable SRGB and run the non-sRGB test case.
            if (srgbWriteControlSupported)
                glDisable(GL_FRAMEBUFFER_SRGB_EXT);
        }

        if (colorEncoding == GL_LINEAR || srgbWriteControlSupported)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 127u, 0u, 255u), 2);
        }

        // Draw with the texture to a linear framebuffer and check that the color value written is
        // correct.
        testTextureSamplesAs50PercentGreen(texture);

        glBindFramebuffer(GL_FRAMEBUFFER, 0u);
        glBindTexture(GL_TEXTURE_2D, 0u);
        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
        eglDestroySurface(display, pbuffer);
    }
}

// Test binding a pbuffer created from a D3D texture as a texture image with typeless texture
// formats.
TEST_P(D3DTextureTest, TestD3D11TypelessTexture)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    ANGLE_SKIP_TEST_IF(!valid());

    // Typeless formats are optional in the spec and currently only supported on D3D11 backend.
    ANGLE_SKIP_TEST_IF(!IsD3D11());

    // GL_SRGB8_ALPHA8 texture attachment support is required.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    const std::array<EGLint, 2> eglGlColorspaces = {EGL_GL_COLORSPACE_LINEAR,
                                                    EGL_GL_COLORSPACE_SRGB};
    const std::array<DXGI_FORMAT, 2> dxgiFormats = {DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                                    DXGI_FORMAT_B8G8R8A8_TYPELESS};
    for (auto eglGlColorspace : eglGlColorspaces)
    {
        for (auto dxgiFormat : dxgiFormats)
        {
            SCOPED_TRACE(std::string("Test case:") + std::to_string(eglGlColorspace) + " / " +
                         std::to_string(dxgiFormat));

            EGLint attribs[] = {
                EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
                EGL_GL_COLORSPACE,  eglGlColorspace,  EGL_NONE,           EGL_NONE,
            };

            EGLSurface pbuffer = createD3D11PBuffer(
                32, 32, 1, 0, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, dxgiFormat,
                attribs);

            ASSERT_EGL_SUCCESS();
            ASSERT_NE(EGL_NO_SURFACE, pbuffer);

            EGLint colorspace = EGL_NONE;
            eglQuerySurface(display, pbuffer, EGL_GL_COLORSPACE, &colorspace);

            GLuint texture = 0u;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            EGLBoolean result = eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
            ASSERT_EGL_SUCCESS();
            ASSERT_EGL_TRUE(result);

            GLuint fbo = 0u;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            glViewport(0, 0, 32, 32);

            GLint colorEncoding = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT,
                                                  &colorEncoding);

            if (eglGlColorspace == EGL_GL_COLORSPACE_LINEAR)
            {
                EXPECT_EQ(EGL_GL_COLORSPACE_LINEAR, colorspace);
                EXPECT_EQ(GL_LINEAR, colorEncoding);
            }
            else
            {
                EXPECT_EQ(EGL_GL_COLORSPACE_SRGB, colorspace);
                EXPECT_EQ(GL_SRGB_EXT, colorEncoding);
            }

            // Clear the texture with 50% green and check that the color value written is correct.
            glClearColor(0.0f, 0.5f, 0.0f, 1.0f);

            if (colorEncoding == GL_SRGB_EXT)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 188u, 0u, 255u), 2);
            }
            if (colorEncoding == GL_LINEAR)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 127u, 0u, 255u), 2);
            }

            // Draw with the texture to a linear framebuffer and check that the color value written
            // is correct.
            testTextureSamplesAs50PercentGreen(texture);

            glBindFramebuffer(GL_FRAMEBUFFER, 0u);
            glBindTexture(GL_TEXTURE_2D, 0u);
            glDeleteTextures(1, &texture);
            glDeleteFramebuffers(1, &fbo);
            eglDestroySurface(display, pbuffer);
        }
    }
}

class D3DTextureTestES3 : public D3DTextureTest
{
  protected:
    D3DTextureTestES3() : D3DTextureTest() {}
};

// Test swizzling a pbuffer created from a D3D texture as a texture image with typeless texture
// formats.
TEST_P(D3DTextureTestES3, TestD3D11TypelessTextureSwizzle)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    ANGLE_SKIP_TEST_IF(!valid());

    // Typeless formats are optional in the spec and currently only supported on D3D11 backend.
    ANGLE_SKIP_TEST_IF(!IsD3D11());

    const std::array<EGLint, 2> eglGlColorspaces = {EGL_GL_COLORSPACE_LINEAR,
                                                    EGL_GL_COLORSPACE_SRGB};
    const std::array<DXGI_FORMAT, 2> dxgiFormats = {DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                                    DXGI_FORMAT_B8G8R8A8_TYPELESS};
    for (auto eglGlColorspace : eglGlColorspaces)
    {
        for (auto dxgiFormat : dxgiFormats)
        {
            SCOPED_TRACE(std::string("Test case:") + std::to_string(eglGlColorspace) + " / " +
                         std::to_string(dxgiFormat));

            EGLint attribs[] = {
                EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
                EGL_GL_COLORSPACE,  eglGlColorspace,  EGL_NONE,           EGL_NONE,
            };

            EGLSurface pbuffer = createD3D11PBuffer(
                32, 32, 1, 0, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, dxgiFormat,
                attribs);

            ASSERT_EGL_SUCCESS();
            ASSERT_NE(EGL_NO_SURFACE, pbuffer);

            EGLint colorspace = EGL_NONE;
            eglQuerySurface(display, pbuffer, EGL_GL_COLORSPACE, &colorspace);

            GLuint texture = 0u;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            EGLBoolean result = eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
            ASSERT_EGL_SUCCESS();
            ASSERT_EGL_TRUE(result);

            GLuint fbo = 0u;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            glViewport(0, 0, 32, 32);

            GLint colorEncoding = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT,
                                                  &colorEncoding);

            // Clear the texture with 50% blue and check that the color value written is correct.
            glClearColor(0.0f, 0.0f, 0.5f, 1.0f);

            if (colorEncoding == GL_SRGB_EXT)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 0u, 188u, 255u), 2);
            }
            if (colorEncoding == GL_LINEAR)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor(0u, 0u, 127u, 255u), 2);
            }

            // Swizzle the green channel to be sampled from the blue channel of the texture and vice
            // versa.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_BLUE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_GREEN);
            ASSERT_GL_NO_ERROR();

            // Draw with the texture to a linear framebuffer and check that the color value written
            // is correct.
            testTextureSamplesAs50PercentGreen(texture);

            glBindFramebuffer(GL_FRAMEBUFFER, 0u);
            glBindTexture(GL_TEXTURE_2D, 0u);
            glDeleteTextures(1, &texture);
            glDeleteFramebuffers(1, &fbo);
            eglDestroySurface(display, pbuffer);
        }
    }
}

// Test that EGL_GL_COLORSPACE attrib is not allowed for typed D3D textures.
TEST_P(D3DTextureTest, GlColorspaceNotAllowedForTypedD3DTexture)
{
    ANGLE_SKIP_TEST_IF(!valid());

    // D3D11 device is required to be able to create the texture.
    ANGLE_SKIP_TEST_IF(!mD3D11Device);

    // SRGB support is required.
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3);

    EGLint attribsExplicitColorspace[] = {
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,       EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_GL_COLORSPACE,  EGL_GL_COLORSPACE_SRGB, EGL_NONE,           EGL_NONE,
    };
    EGLSurface pbuffer =
        createD3D11PBuffer(32, 32, 1, 0, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
                           DXGI_FORMAT_R8G8B8A8_UNORM, attribsExplicitColorspace);

    ASSERT_EGL_ERROR(EGL_BAD_MATCH);
    ASSERT_EQ(EGL_NO_SURFACE, pbuffer);
}

// Test that trying to create a pbuffer from a typeless texture fails as expected on the backends
// where they are known not to be supported.
TEST_P(D3DTextureTest, TypelessD3DTextureNotSupported)
{
    ANGLE_SKIP_TEST_IF(!valid());

    // D3D11 device is required to be able to create the texture.
    ANGLE_SKIP_TEST_IF(!mD3D11Device);

    // Currently typeless textures are supported on the D3D11 backend. We're testing the backends
    // where there is no support.
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // SRGB support is required.
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3);

    EGLint attribs[] = {
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, EGL_TEXTURE_TARGET,
        EGL_TEXTURE_2D,     EGL_NONE,         EGL_NONE,
    };
    EGLSurface pbuffer =
        createD3D11PBuffer(32, 32, 1, 0, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
                           DXGI_FORMAT_R8G8B8A8_TYPELESS, attribs);
    ASSERT_EGL_ERROR(EGL_BAD_PARAMETER);
    ASSERT_EQ(EGL_NO_SURFACE, pbuffer);
}

// Test creating a pbuffer with unnecessary EGL_WIDTH and EGL_HEIGHT attributes because that's what
// Chromium does. This is a regression test for crbug.com/794086
TEST_P(D3DTextureTest, UnnecessaryWidthHeightAttributes)
{
    ANGLE_SKIP_TEST_IF(!valid() || !IsD3D11());
    ASSERT_TRUE(mD3D11Device != nullptr);
    ID3D11Texture2D *texture = nullptr;
    CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, D3D11_BIND_RENDER_TARGET);
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    EXPECT_TRUE(SUCCEEDED(mD3D11Device->CreateTexture2D(&desc, nullptr, &texture)));

    EGLint attribs[] = {
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_WIDTH,          1,
        EGL_HEIGHT,         1,
        EGL_NONE,           EGL_NONE,
    };

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();
    EGLConfig config   = window->getConfig();

    EGLSurface pbuffer =
        eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE, texture, config, attribs);

    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    texture->Release();

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test creating a pbuffer from a d3d surface and clearing it
TEST_P(D3DTextureTest, Clear)
{
    if (!valid())
    {
        return;
    }

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    const size_t bufferSize = 32;

    EGLSurface pbuffer =
        createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 1, 0);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    // Apply the Pbuffer and clear it to purple and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2, 255, 0,
                    255, 255);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test creating a pbuffer with a D3D texture and depth stencil bits in the EGL config creates keeps
// its depth stencil buffer
TEST_P(D3DTextureTest, DepthStencil)
{
    if (!valid())
    {
        return;
    }

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    const size_t bufferSize = 32;

    EGLSurface pbuffer =
        createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 1, 0);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    // Apply the Pbuffer and clear it to purple and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
    glClearDepthf(0.5f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    // Draw a quad that will fail the depth test and verify that the buffer is unchanged
    drawQuad(mTextureProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::cyan);

    // Draw a quad that will pass the depth test and verify that the buffer is green
    drawQuad(mTextureProgram, "position", -1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::green);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test creating a pbuffer from a d3d surface and binding it to a texture
TEST_P(D3DTextureTest, BindTexImage)
{
    if (!valid())
    {
        return;
    }

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    const size_t bufferSize = 32;

    EGLSurface pbuffer =
        createPBuffer(bufferSize, bufferSize, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 1, 0);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    // Apply the Pbuffer and clear it to purple
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2, 255, 0,
                    255, 255);

    // Apply the window surface
    eglMakeCurrent(display, window->getSurface(), window->getSurface(), window->getContext());

    // Create a texture and bind the Pbuffer to it
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Draw a quad and verify that it is purple
    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Unbind the texture
    eglReleaseTexImage(display, pbuffer, EGL_BACK_BUFFER);
    ASSERT_EGL_SUCCESS();

    // Verify that purple was drawn
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255, 0, 255, 255);

    glDeleteTextures(1, &texture);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Verify that creating a pbuffer with a multisampled texture will fail on a non-multisampled
// window.
TEST_P(D3DTextureTest, CheckSampleMismatch)
{
    if (!valid())
    {
        return;
    }

    // Multisampling is not supported on D3D9 or OpenGL.
    ANGLE_SKIP_TEST_IF(IsD3D9() || IsOpenGL());

    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 2,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    EXPECT_EQ(pbuffer, nullptr);
}

// Tests what happens when we make a PBuffer that isn't shader-readable.
TEST_P(D3DTextureTest, NonReadablePBuffer)
{
    ANGLE_SKIP_TEST_IF(!valid() || !IsD3D11());

    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer =
        createD3D11PBuffer(bufferSize, bufferSize, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 1, 0,
                           D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_R8G8B8A8_UNORM);

    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));

    // Clear to green.
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Copy the green color to a texture.
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, bufferSize, bufferSize, 0);
    ASSERT_GL_NO_ERROR();

    // Clear to red.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw with the texture and expect green.
    draw2DTexturedQuad(0.5f, 1.0f, false);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

class D3DTextureTestMS : public D3DTextureTest
{
  protected:
    D3DTextureTestMS() : D3DTextureTest()
    {
        setSamples(4);
        setMultisampleEnabled(true);
    }
};

// Test creating a pbuffer from a multisampled d3d surface and clearing it.
TEST_P(D3DTextureTestMS, Clear)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    constexpr size_t bufferSize = 32;
    constexpr UINT testpoint    = bufferSize / 2;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    // Apply the Pbuffer and clear it to magenta and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(testpoint, testpoint, GLColor::magenta);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test creating a pbuffer from a multisampled d3d surface and drawing with a program.
TEST_P(D3DTextureTestMS, DrawProgram)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(pbuffer, EGL_NO_SURFACE);

    // Apply the Pbuffer and clear it to magenta
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    constexpr GLint testPoint = bufferSize / 2;
    EXPECT_PIXEL_COLOR_EQ(testPoint, testPoint, GLColor::magenta);

    // Apply the window surface
    eglMakeCurrent(display, window->getSurface(), window->getSurface(), window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Draw a quad and verify that it is magenta
    glUseProgram(mTextureProgramNoSampling);
    EXPECT_GL_NO_ERROR();

    drawQuad(mTextureProgramNoSampling, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Verify that magenta was drawn
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::magenta);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test for failure when creating a pbuffer from a multisampled d3d surface to bind to a texture.
TEST_P(D3DTextureTestMS, BindTexture)
{
    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));

    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    EXPECT_EQ(pbuffer, nullptr);
}

// Verify that creating a pbuffer from a multisampled texture with a multisampled window will fail
// when the sample counts do not match.
TEST_P(D3DTextureTestMS, CheckSampleMismatch)
{
    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 2,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));

    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    EXPECT_EQ(pbuffer, nullptr);
}

// Test creating a pbuffer with a D3D texture and depth stencil bits in the EGL config creates keeps
// its depth stencil buffer
TEST_P(D3DTextureTestMS, DepthStencil)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    const size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, pbuffer);

    // Apply the Pbuffer and clear it to purple and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
    glClearDepthf(0.5f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    // Draw a quad that will fail the depth test and verify that the buffer is unchanged
    drawQuad(mTextureProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::cyan);

    // Draw a quad that will pass the depth test and verify that the buffer is green
    drawQuad(mTextureProgram, "position", -1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::green);

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test copyTexImage2D with a multisampled resource
TEST_P(D3DTextureTestMS, CopyTexImage2DTest)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, pbuffer);

    // Apply the Pbuffer and clear it to magenta and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    // Specify a 2D texture and set it to green
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    // Copy from the multisampled framebuffer to the 2D texture
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0);

    // Draw a quad and verify the color is magenta, not green
    drawQuad(mTextureProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Test copyTexSubImage2D with a multisampled resource
TEST_P(D3DTextureTestMS, CopyTexSubImage2DTest)
{
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    constexpr size_t bufferSize = 32;

    EGLSurface pbuffer = createPBuffer(bufferSize, bufferSize, EGL_NO_TEXTURE, EGL_NO_TEXTURE, 4,
                                       static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN));
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, pbuffer);

    // Apply the Pbuffer and clear it to magenta and verify
    eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(bufferSize), static_cast<GLsizei>(bufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    // Specify a 2D texture and set it to green
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);

    // Copy from the multisampled framebuffer to the 2D texture
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);

    // Draw a quad and verify the color is magenta, not green
    drawQuad(mTextureProgram, "position", 1.0f);
    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(bufferSize) / 2, static_cast<GLint>(bufferSize) / 2,
                          GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Make current with fixture EGL to ensure the Surface can be released immediately.
    getEGLWindow()->makeCurrent();
    eglDestroySurface(display, pbuffer);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(D3DTextureTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL(), ES2_VULKAN());
ANGLE_INSTANTIATE_TEST(D3DTextureTestES3, ES3_D3D11(), ES3_OPENGL());
ANGLE_INSTANTIATE_TEST(D3DTextureTestMS, ES2_D3D11());
}  // namespace angle

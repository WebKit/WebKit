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

#include "com_utils.h"

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

        const std::string vsSource =
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = position;
                texcoord = (position.xy * 0.5) + 0.5;
                texcoord.y = 1.0 - texcoord.y;
            })";

        const std::string textureFSSource =
            R"(precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            })";

        const std::string textureFSSourceNoSampling =
            R"(precision highp float;

            void main()
            {
                gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
            })";

        mTextureProgram = CompileProgram(vsSource, textureFSSource);
        ASSERT_NE(0u, mTextureProgram) << "shader compilation failed.";

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");
        ASSERT_NE(-1, mTextureUniformLocation);

        mTextureProgramNoSampling = CompileProgram(vsSource, textureFSSourceNoSampling);
        ASSERT_NE(0u, mTextureProgramNoSampling) << "shader compilation failed.";

        mD3D11Module = LoadLibrary(TEXT("d3d11.dll"));
        ASSERT_NE(nullptr, mD3D11Module);

        PFN_D3D11_CREATE_DEVICE createDeviceFunc = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
            GetProcAddress(mD3D11Module, "D3D11CreateDevice"));

        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        if (eglDisplayExtensionEnabled(display, "EGL_EXT_device_query"))
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

            if (eglDeviceExtensionEnabled(device, "EGL_ANGLE_device_d3d"))
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
                                  EGLint eglTextureFormat,
                                  EGLint eglTextureTarget,
                                  UINT sampleCount,
                                  UINT sampleQuality,
                                  UINT bindFlags,
                                  DXGI_FORMAT format)
    {
        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        EGLConfig config   = window->getConfig();

        EGLint attribs[] = {
            EGL_TEXTURE_FORMAT, eglTextureFormat, EGL_TEXTURE_TARGET,
            eglTextureTarget,   EGL_NONE,         EGL_NONE,
        };

        ASSERT(mD3D11Device);
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
            ASSERT(sampleCount <= 1);
            ASSERT(sampleQuality == 0);

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
        if (!eglDisplayExtensionEnabled(display, "EGL_ANGLE_d3d_texture_client_buffer"))
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

    GLuint mTextureProgram;
    GLuint mTextureProgramNoSampling;
    GLint mTextureUniformLocation;

    HMODULE mD3D11Module       = nullptr;
    ID3D11Device *mD3D11Device = nullptr;

    IDirect3DDevice9 *mD3D9Device = nullptr;
};

// Test creating pbuffer from textures with several
// different DXGI formats.
TEST_P(D3DTextureTest, TestD3D11SupportedFormats)
{
    ANGLE_SKIP_TEST_IF(!valid() || !IsD3D11());

    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                   DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB};
    for (size_t i = 0; i < 4; ++i)
    {
        EGLSurface pbuffer = createD3D11PBuffer(32, 32, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 1, 0,
                                                D3D11_BIND_RENDER_TARGET, formats[i]);
        ASSERT_EGL_SUCCESS();
        ASSERT_NE(pbuffer, EGL_NO_SURFACE);

        EGLWindow *window  = getEGLWindow();
        EGLDisplay display = window->getDisplay();
        eglMakeCurrent(display, pbuffer, pbuffer, window->getContext());
        ASSERT_EGL_SUCCESS();

        window->makeCurrent();
        eglDestroySurface(display, pbuffer);
    }
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

    EGLWindow *window = getEGLWindow();
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
    if (IsD3D9() || IsOpenGL())
    {
        return;
    }

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

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(D3DTextureTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());
ANGLE_INSTANTIATE_TEST(D3DTextureTestMS, ES2_D3D11());
}  // namespace

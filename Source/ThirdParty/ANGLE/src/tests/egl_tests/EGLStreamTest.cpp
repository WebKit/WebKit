//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLStreamTest:
//   Tests pertaining to egl::Stream.
//

#include <gtest/gtest.h>

#include <vector>

#include "media/yuvtest.inl"
#include "OSWindow.h"
#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class EGLStreamTest : public ANGLETest
{
  protected:
    EGLStreamTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }
};

// Tests validation of the stream API
TEST_P(EGLStreamTest, StreamValidationTest)
{
    EGLWindow *window            = getEGLWindow();
    EGLDisplay display           = window->getDisplay();
    const char *extensionsString = eglQueryString(display, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_KHR_stream") == nullptr)
    {
        std::cout << "Stream extension not supported" << std::endl;
        return;
    }

    const EGLint streamAttributesBad[] = {
        EGL_STREAM_STATE_KHR,
        0,
        EGL_NONE,
        EGL_PRODUCER_FRAME_KHR,
        0,
        EGL_NONE,
        EGL_CONSUMER_FRAME_KHR,
        0,
        EGL_NONE,
        EGL_CONSUMER_LATENCY_USEC_KHR,
        -1,
        EGL_NONE,
        EGL_RED_SIZE,
        EGL_DONT_CARE,
        EGL_NONE,
    };

    // Validate create stream attributes
    EGLStreamKHR stream = eglCreateStreamKHR(display, &streamAttributesBad[0]);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    stream = eglCreateStreamKHR(display, &streamAttributesBad[3]);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    stream = eglCreateStreamKHR(display, &streamAttributesBad[6]);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    stream = eglCreateStreamKHR(display, &streamAttributesBad[9]);
    ASSERT_EGL_ERROR(EGL_BAD_PARAMETER);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    stream = eglCreateStreamKHR(display, &streamAttributesBad[12]);
    ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    const EGLint streamAttributes[] = {
        EGL_CONSUMER_LATENCY_USEC_KHR, 0, EGL_NONE,
    };

    stream = eglCreateStreamKHR(EGL_NO_DISPLAY, streamAttributes);
    ASSERT_EGL_ERROR(EGL_BAD_DISPLAY);
    ASSERT_EQ(EGL_NO_STREAM_KHR, stream);

    // Create an actual stream
    stream = eglCreateStreamKHR(display, streamAttributes);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_STREAM_KHR, stream);

    // Assert it is in the created state
    EGLint state;
    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_CREATED_KHR, state);

    // Test getting and setting the latency
    EGLint latency = 10;
    eglStreamAttribKHR(display, stream, EGL_CONSUMER_LATENCY_USEC_KHR, latency);
    ASSERT_EGL_SUCCESS();
    eglQueryStreamKHR(display, stream, EGL_CONSUMER_LATENCY_USEC_KHR, &latency);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(10, latency);
    eglStreamAttribKHR(display, stream, EGL_CONSUMER_LATENCY_USEC_KHR, -1);
    ASSERT_EGL_ERROR(EGL_BAD_PARAMETER);
    ASSERT_EQ(10, latency);

    // Test the 64-bit queries
    EGLuint64KHR value;
    eglQueryStreamu64KHR(display, stream, EGL_CONSUMER_FRAME_KHR, &value);
    ASSERT_EGL_SUCCESS();
    eglQueryStreamu64KHR(display, stream, EGL_PRODUCER_FRAME_KHR, &value);
    ASSERT_EGL_SUCCESS();

    // Destroy the stream
    eglDestroyStreamKHR(display, stream);
    ASSERT_EGL_SUCCESS();
}

// Tests validation of stream consumer gltexture API
TEST_P(EGLStreamTest, StreamConsumerGLTextureValidationTest)
{
    EGLWindow *window            = getEGLWindow();
    EGLDisplay display           = window->getDisplay();
    const char *extensionsString = eglQueryString(display, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_KHR_stream_consumer_gltexture") == nullptr)
    {
        std::cout << "Stream consumer gltexture extension not supported" << std::endl;
        return;
    }

    const EGLint streamAttributes[] = {
        EGL_CONSUMER_LATENCY_USEC_KHR, 0, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0, EGL_NONE,
    };

    EGLStreamKHR stream = eglCreateStreamKHR(display, streamAttributes);
    ASSERT_EGL_SUCCESS();

    EGLBoolean result = eglStreamConsumerGLTextureExternalKHR(display, stream);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
    result = eglStreamConsumerGLTextureExternalKHR(display, stream);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLint state;
    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_CONNECTING_KHR, state);

    eglDestroyStreamKHR(display, stream);
    ASSERT_EGL_SUCCESS();
}

// Tests validation of stream consumer gltexture yuv API
TEST_P(EGLStreamTest, StreamConsumerGLTextureYUVValidationTest)
{
    EGLWindow *window            = getEGLWindow();
    EGLDisplay display           = window->getDisplay();
    const char *extensionsString = eglQueryString(display, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_NV_stream_consumer_gltexture_yuv") == nullptr)
    {
        std::cout << "Stream consumer gltexture yuv extension not supported" << std::endl;
        return;
    }

    const EGLint streamAttributes[] = {
        EGL_CONSUMER_LATENCY_USEC_KHR, 0, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0, EGL_NONE,
    };

    EGLStreamKHR stream = eglCreateStreamKHR(display, streamAttributes);
    ASSERT_EGL_SUCCESS();

    EGLAttrib consumerAttributesBad[] = {
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,  // 0
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        0,
        EGL_NONE,
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,  // 5
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        1,
        EGL_NONE,
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,  // 10
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        1,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        9999,
        EGL_NONE,
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,  // 17
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        1,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        EGL_NONE,
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,  // 26
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        0,
        EGL_NONE,
    };

    EGLAttrib consumerAttributes[] = {
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        EGL_NONE,
    };

    EGLBoolean result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, &consumerAttributesBad[0]);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_MATCH);

    result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, &consumerAttributesBad[5]);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_MATCH);

    result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, &consumerAttributesBad[10]);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);

    result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, &consumerAttributesBad[17]);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_MATCH);

    result = eglStreamConsumerGLTextureExternalAttribsNV(display, stream, consumerAttributes);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);

    GLuint tex[2];
    glGenTextures(2, tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[1]);

    result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, &consumerAttributesBad[26]);
    ASSERT_EGL_FALSE(result);
    ASSERT_EGL_ERROR(EGL_BAD_ACCESS);

    result = eglStreamConsumerGLTextureExternalAttribsNV(display, stream, consumerAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLint state;
    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_CONNECTING_KHR, state);

    eglDestroyStreamKHR(display, stream);
    ASSERT_EGL_SUCCESS();
}

// Tests that deleting a texture invalidates the associated stream
TEST_P(EGLStreamTest, StreamConsumerGLTextureYUVDeletionTest)
{
    EGLWindow *window            = getEGLWindow();
    EGLDisplay display           = window->getDisplay();
    const char *extensionsString = eglQueryString(display, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_ANGLE_stream_producer_d3d_texture_nv12") == nullptr)
    {
        std::cout << "Stream producer d3d nv12 texture not supported" << std::endl;
        return;
    }

    const EGLint streamAttributes[] = {
        EGL_CONSUMER_LATENCY_USEC_KHR, 0, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0, EGL_NONE,
    };

    EGLStreamKHR stream = eglCreateStreamKHR(display, streamAttributes);
    ASSERT_EGL_SUCCESS();

    EGLAttrib consumerAttributes[] = {
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        EGL_NONE,
    };

    GLuint tex[2];
    glGenTextures(2, tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[1]);

    EGLBoolean result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, consumerAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLAttrib producerAttributes[] = {
        EGL_NONE,
    };

    result = eglCreateStreamProducerD3DTextureNV12ANGLE(display, stream, producerAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLint state;
    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_EMPTY_KHR, state);

    // Delete the first texture, which should be enough to invalidate the stream
    glDeleteTextures(1, tex);

    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_DISCONNECTED_KHR, state);

    eglDestroyStreamKHR(display, stream);
    ASSERT_EGL_SUCCESS();
}

// End2end test for rendering an NV12 texture. Renders a YUV quad, reads back the RGB values, and
// ensures they are correct
TEST_P(EGLStreamTest, StreamProducerTextureNV12End2End)
{
    EGLWindow *window            = getEGLWindow();
    EGLDisplay display           = window->getDisplay();
    if (!eglDisplayExtensionEnabled(display, "EGL_ANGLE_stream_producer_d3d_texture_nv12"))
    {
        std::cout << "Stream producer d3d nv12 texture not supported" << std::endl;
        return;
    }

    bool useESSL3Shaders =
        getClientMajorVersion() >= 3 && extensionEnabled("GL_OES_EGL_image_external_essl3");

    // yuv to rgb conversion shader using Microsoft's given conversion formulas
    std::string yuvVS, yuvPS;
    if (useESSL3Shaders)
    {
        yuvVS =
            "#version 300 es\n"
            "in highp vec4 position;\n"
            "out vec2 texcoord;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = position;\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "    texcoord.y = 1.0 - texcoord.y;\n"
            "}\n";
        yuvPS =
            "#version 300 es\n"
            "#extension GL_OES_EGL_image_external_essl3 : require\n"
            "#extension GL_NV_EGL_stream_consumer_external : require\n"
            "precision highp float;\n"
            "in vec2 texcoord;\n"
            "out vec4 color;\n"
            "uniform samplerExternalOES y;\n"
            "uniform samplerExternalOES uv\n;"
            "void main(void)\n"
            "{\n"
            "    float c = texture(y, texcoord).r - (16.0 / 256.0);\n"
            "    float d = texture(uv, texcoord).r - 0.5;\n"
            "    float e = texture(uv, texcoord).g - 0.5;\n"
            "    float r = 1.164383 * c + 1.596027 * e;\n"
            "    float g = 1.164383 * c - 0.391762 * d - 0.812968 * e;\n"
            "    float b = 1.164383 * c + 2.017232 * d;\n"
            "    color = vec4(r, g, b, 1.0);\n"
            "}\n";
    }
    else
    {
        yuvVS =
            "attribute highp vec4 position;\n"
            "varying vec2 texcoord;\n"
            "void main(void)\n"
            "{\n"
            "    gl_Position = position;\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "    texcoord.y = 1.0 - texcoord.y;\n"
            "}\n";

        yuvPS =
            "#extension GL_NV_EGL_stream_consumer_external : require\n"
            "precision highp float;\n"
            "varying vec2 texcoord;\n"
            "uniform samplerExternalOES y;\n"
            "uniform samplerExternalOES uv\n;"
            "void main(void)\n"
            "{\n"
            "    float c = texture2D(y, texcoord).r - (16.0 / 256.0);\n"
            "    float d = texture2D(uv, texcoord).r - 0.5;\n"
            "    float e = texture2D(uv, texcoord).g - 0.5;\n"
            "    float r = 1.164383 * c + 1.596027 * e;\n"
            "    float g = 1.164383 * c - 0.391762 * d - 0.812968 * e;\n"
            "    float b = 1.164383 * c + 2.017232 * d;\n"
            "    gl_FragColor = vec4(r, g, b, 1.0);\n"
            "}\n";
    }

    GLuint program = CompileProgram(yuvVS, yuvPS);
    ASSERT_NE(0u, program);
    GLuint yUniform  = glGetUniformLocation(program, "y");
    GLuint uvUniform = glGetUniformLocation(program, "uv");

    // Fetch the D3D11 device
    EGLDeviceEXT eglDevice;
    eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, (EGLAttrib *)&eglDevice);
    ID3D11Device *device;
    eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, (EGLAttrib *)&device);

    // Create the NV12 D3D11 texture
    HRESULT res;
    D3D11_TEXTURE2D_DESC desc;
    desc.Width              = yuvtest_width;
    desc.Height             = yuvtest_height;
    desc.Format             = DXGI_FORMAT_NV12;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;

    D3D11_SUBRESOURCE_DATA subres;
    subres.pSysMem          = yuvtest_data;
    subres.SysMemPitch      = yuvtest_width;
    subres.SysMemSlicePitch = yuvtest_width * yuvtest_height * 3 / 2;

    ID3D11Texture2D *texture = nullptr;
    res                      = device->CreateTexture2D(&desc, &subres, &texture);

    // Create the stream
    const EGLint streamAttributes[] = {
        EGL_CONSUMER_LATENCY_USEC_KHR, 0, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0, EGL_NONE,
    };

    EGLStreamKHR stream = eglCreateStreamKHR(display, streamAttributes);
    ASSERT_EGL_SUCCESS();

    EGLAttrib consumerAttributes[] = {
        EGL_COLOR_BUFFER_TYPE,
        EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,
        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
        0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
        1,
        EGL_NONE,
    };

    GLuint tex[2];
    glGenTextures(2, tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[0]);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex[1]);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ASSERT_GL_NO_ERROR();

    EGLBoolean result =
        eglStreamConsumerGLTextureExternalAttribsNV(display, stream, consumerAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLAttrib producerAttributes[] = {
        EGL_NONE,
    };

    result = eglCreateStreamProducerD3DTextureNV12ANGLE(display, stream, producerAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    // Insert the frame
    EGLAttrib frameAttributes[] = {
        EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0, EGL_NONE,
    };
    result = eglStreamPostD3DTextureNV12ANGLE(display, stream, (void *)texture, frameAttributes);
    ASSERT_EGL_TRUE(result);
    ASSERT_EGL_SUCCESS();

    EGLint state;
    eglQueryStreamKHR(display, stream, EGL_STREAM_STATE_KHR, &state);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR, state);

    eglStreamConsumerAcquireKHR(display, stream);
    ASSERT_EGL_SUCCESS();

    glUseProgram(program);
    glUniform1i(yUniform, 0);
    glUniform1i(uvUniform, 1);
    drawQuad(program, "position", 0.0f);
    ASSERT_GL_NO_ERROR();

    eglStreamConsumerReleaseKHR(display, stream);
    ASSERT_EGL_SUCCESS();

    eglSwapBuffers(display, window->getSurface());
    SafeRelease(texture);
}

ANGLE_INSTANTIATE_TEST(EGLStreamTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL());
}  // anonymous namespace

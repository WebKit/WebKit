//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageTest:
//   Tests the correctness of eglImage.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#include "common/android_util.h"

#if defined(ANGLE_PLATFORM_ANDROID) && __ANDROID_API__ >= 26
#    define ANGLE_AHARDWARE_BUFFER_SUPPORT
// NDK header file for access to Android Hardware Buffers
#    include <android/hardware_buffer.h>
#endif

namespace angle
{
namespace
{
constexpr char kOESExt[]                         = "GL_OES_EGL_image";
constexpr char kExternalExt[]                    = "GL_OES_EGL_image_external";
constexpr char kExternalESSL3Ext[]               = "GL_OES_EGL_image_external_essl3";
constexpr char kBaseExt[]                        = "EGL_KHR_image_base";
constexpr char k2DTextureExt[]                   = "EGL_KHR_gl_texture_2D_image";
constexpr char k3DTextureExt[]                   = "EGL_KHR_gl_texture_3D_image";
constexpr char kPixmapExt[]                      = "EGL_KHR_image_pixmap";
constexpr char kRenderbufferExt[]                = "EGL_KHR_gl_renderbuffer_image";
constexpr char kCubemapExt[]                     = "EGL_KHR_gl_texture_cubemap_image";
constexpr char kImageGLColorspaceExt[]           = "EGL_EXT_image_gl_colorspace";
constexpr char kEGLImageArrayExt[]               = "GL_EXT_EGL_image_array";
constexpr char kEGLAndroidImageNativeBufferExt[] = "EGL_ANDROID_image_native_buffer";
constexpr EGLint kDefaultAttribs[]               = {
    EGL_IMAGE_PRESERVED,
    EGL_TRUE,
    EGL_NONE,
};
constexpr EGLint kColorspaceAttribs[] = {
    EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE,
};
}  // anonymous namespace

class ImageTest : public ANGLETest
{
  protected:
    ImageTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void testSetUp() override
    {
        constexpr char kVS[] =
            "precision highp float;\n"
            "attribute vec4 position;\n"
            "varying vec2 texcoord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = position;\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "    texcoord.y = 1.0 - texcoord.y;\n"
            "}\n";
        constexpr char kVS2DArray[] =
            "#version 300 es\n"
            "out vec2 texcoord;\n"
            "in vec4 position;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "}\n";
        constexpr char kVSESSL3[] =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec4 position;\n"
            "out vec2 texcoord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = position;\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "    texcoord.y = 1.0 - texcoord.y;\n"
            "}\n";

        constexpr char kTextureFS[] =
            "precision highp float;\n"
            "uniform sampler2D tex;\n"
            "varying vec2 texcoord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture2D(tex, texcoord);\n"
            "}\n";
        constexpr char kTexture2DArrayFS[] =
            "#version 300 es\n"
            "precision highp float;\n"
            "uniform highp sampler2DArray tex2DArray;\n"
            "in vec2 texcoord;\n"
            "out vec4 fragColor;\n"
            "void main()\n"
            "{\n"
            "    fragColor = texture(tex2DArray, vec3(texcoord.x, texcoord.y, 0.0));\n"
            "}\n";
        constexpr char kTextureExternalFS[] =
            "#extension GL_OES_EGL_image_external : require\n"
            "precision highp float;\n"
            "uniform samplerExternalOES tex;\n"
            "varying vec2 texcoord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture2D(tex, texcoord);\n"
            "}\n";
        constexpr char kTextureExternalESSL3FS[] =
            "#version 300 es\n"
            "#extension GL_OES_EGL_image_external_essl3 : require\n"
            "precision highp float;\n"
            "uniform samplerExternalOES tex;\n"
            "in vec2 texcoord;\n"
            "out vec4 color;"
            "\n"
            "void main()\n"
            "{\n"
            "    color = texture(tex, texcoord);\n"
            "}\n";

        mTextureProgram = CompileProgram(kVS, kTextureFS);
        if (mTextureProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");

        if (getClientMajorVersion() >= 3)
        {
            m2DArrayTextureProgram = CompileProgram(kVS2DArray, kTexture2DArrayFS);
            if (m2DArrayTextureProgram == 0)
            {
                FAIL() << "shader compilation failed.";
            }

            m2DArrayTextureUniformLocation =
                glGetUniformLocation(m2DArrayTextureProgram, "tex2DArray");
        }

        if (IsGLExtensionEnabled("GL_OES_EGL_image_external"))
        {
            mTextureExternalProgram = CompileProgram(kVS, kTextureExternalFS);
            ASSERT_NE(0u, mTextureExternalProgram) << "shader compilation failed.";

            mTextureExternalUniformLocation = glGetUniformLocation(mTextureExternalProgram, "tex");
        }

        if (IsGLExtensionEnabled("GL_OES_EGL_image_external_essl3"))
        {
            mTextureExternalESSL3Program = CompileProgram(kVSESSL3, kTextureExternalESSL3FS);
            ASSERT_NE(0u, mTextureExternalESSL3Program) << "shader compilation failed.";

            mTextureExternalESSL3UniformLocation =
                glGetUniformLocation(mTextureExternalESSL3Program, "tex");
        }

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteProgram(mTextureProgram);
        glDeleteProgram(mTextureExternalProgram);
        glDeleteProgram(mTextureExternalESSL3Program);
    }

    void createEGLImage2DTextureSource(size_t width,
                                       size_t height,
                                       GLenum format,
                                       GLenum type,
                                       const EGLint *attribs,
                                       void *data,
                                       GLuint *outSourceTexture,
                                       EGLImageKHR *outSourceImage)
    {
        // Create a source 2D texture
        GLuint source;
        glGenTextures(1, &source);
        glBindTexture(GL_TEXTURE_2D, source);

        glTexImage2D(GL_TEXTURE_2D, 0, format, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, format, type, data);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        // Create an image from the source texture
        EGLWindow *window = getEGLWindow();

        EGLImageKHR image =
            eglCreateImageKHR(window->getDisplay(), window->getContext(), EGL_GL_TEXTURE_2D_KHR,
                              reinterpretHelper<EGLClientBuffer>(source), attribs);

        ASSERT_EGL_SUCCESS();

        *outSourceTexture = source;
        *outSourceImage   = image;
    }

    void createEGLImageCubemapTextureSource(size_t width,
                                            size_t height,
                                            GLenum format,
                                            GLenum type,
                                            const EGLint *attribs,
                                            uint8_t *data,
                                            size_t dataStride,
                                            EGLenum imageTarget,
                                            GLuint *outSourceTexture,
                                            EGLImageKHR *outSourceImage)
    {
        // Create a source cube map texture
        GLuint source;
        glGenTextures(1, &source);
        glBindTexture(GL_TEXTURE_CUBE_MAP, source);

        for (GLenum faceIdx = 0; faceIdx < 6; faceIdx++)
        {
            glTexImage2D(faceIdx + GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, format,
                         static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, format, type,
                         data + (faceIdx * dataStride));
        }

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        // Create an image from the source texture
        EGLWindow *window = getEGLWindow();

        EGLImageKHR image =
            eglCreateImageKHR(window->getDisplay(), window->getContext(), imageTarget,
                              reinterpretHelper<EGLClientBuffer>(source), attribs);

        ASSERT_EGL_SUCCESS();

        *outSourceTexture = source;
        *outSourceImage   = image;
    }

    void createEGLImage3DTextureSource(size_t width,
                                       size_t height,
                                       size_t depth,
                                       GLenum format,
                                       GLenum type,
                                       const EGLint *attribs,
                                       void *data,
                                       GLuint *outSourceTexture,
                                       EGLImageKHR *outSourceImage)
    {
        // Create a source 3D texture
        GLuint source;
        glGenTextures(1, &source);
        glBindTexture(GL_TEXTURE_3D, source);

        glTexImage3D(GL_TEXTURE_3D, 0, format, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), static_cast<GLsizei>(depth), 0, format, type,
                     data);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        // Create an image from the source texture
        EGLWindow *window = getEGLWindow();

        EGLImageKHR image =
            eglCreateImageKHR(window->getDisplay(), window->getContext(), EGL_GL_TEXTURE_3D_KHR,
                              reinterpretHelper<EGLClientBuffer>(source), attribs);

        ASSERT_EGL_SUCCESS();

        *outSourceTexture = source;
        *outSourceImage   = image;
    }

    void createEGLImageRenderbufferSource(size_t width,
                                          size_t height,
                                          GLenum internalFormat,
                                          const EGLint *attribs,
                                          GLubyte data[4],
                                          GLuint *outSourceRenderbuffer,
                                          EGLImageKHR *outSourceImage)
    {
        // Create a source renderbuffer
        GLuint source;
        glGenRenderbuffers(1, &source);
        glBindRenderbuffer(GL_RENDERBUFFER, source);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, static_cast<GLsizei>(width),
                              static_cast<GLsizei>(height));

        // Create a framebuffer and clear it to set the data
        GLuint framebuffer;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, source);

        glClearColor(data[0] / 255.0f, data[1] / 255.0f, data[2] / 255.0f, data[3] / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glDeleteFramebuffers(1, &framebuffer);

        ASSERT_GL_NO_ERROR();

        // Create an image from the source renderbuffer
        EGLWindow *window = getEGLWindow();

        EGLImageKHR image =
            eglCreateImageKHR(window->getDisplay(), window->getContext(), EGL_GL_RENDERBUFFER_KHR,
                              reinterpretHelper<EGLClientBuffer>(source), attribs);

        ASSERT_EGL_SUCCESS();

        *outSourceRenderbuffer = source;
        *outSourceImage        = image;
    }

    void createEGLImageTargetTexture2D(EGLImageKHR image, GLuint *outTargetTexture)
    {
        // Create a target texture from the image
        GLuint target;
        glGenTextures(1, &target);
        glBindTexture(GL_TEXTURE_2D, target);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        *outTargetTexture = target;
    }

    void createEGLImageTargetTexture2DArray(EGLImageKHR image, GLuint *outTargetTexture)
    {
        // Create a target texture from the image
        GLuint target;
        glGenTextures(1, &target);
        glBindTexture(GL_TEXTURE_2D_ARRAY, target);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D_ARRAY, image);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        *outTargetTexture = target;
    }

    void createEGLImageTargetTextureExternal(EGLImageKHR image, GLuint *outTargetTexture)
    {
        // Create a target texture from the image
        GLuint target;
        glGenTextures(1, &target);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, target);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

        // Disable mipmapping
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        ASSERT_GL_NO_ERROR();

        *outTargetTexture = target;
    }

    AHardwareBuffer *createAndroidHardwareBuffer(size_t width,
                                                 size_t height,
                                                 size_t depth,
                                                 int androidFormat,
                                                 const GLubyte *data,
                                                 size_t bytesPerPixel)
    {
#if defined(ANGLE_AHARDWARE_BUFFER_SUPPORT)
        // The height and width are number of pixels of size format
        AHardwareBuffer_Desc aHardwareBufferDescription = {};
        aHardwareBufferDescription.width                = width;
        aHardwareBufferDescription.height               = height;
        aHardwareBufferDescription.layers               = depth;
        aHardwareBufferDescription.format               = androidFormat;
        aHardwareBufferDescription.usage =
            AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        aHardwareBufferDescription.stride = 0;
        aHardwareBufferDescription.rfu0   = 0;
        aHardwareBufferDescription.rfu1   = 0;

        // Allocate memory from Android Hardware Buffer
        AHardwareBuffer *aHardwareBuffer = nullptr;
        EXPECT_EQ(0, AHardwareBuffer_allocate(&aHardwareBufferDescription, &aHardwareBuffer));

        void *mappedMemory = nullptr;
        EXPECT_EQ(0, AHardwareBuffer_lock(aHardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                                          -1, nullptr, &mappedMemory));

        // Need to grab the stride the implementation might have enforced
        AHardwareBuffer_describe(aHardwareBuffer, &aHardwareBufferDescription);
        const uint32_t stride = aHardwareBufferDescription.stride;

        const uint32_t rowSize = bytesPerPixel * width;
        for (uint32_t i = 0; i < height; i++)
        {
            uint32_t dstPtrOffset = stride * i * bytesPerPixel;
            uint32_t srcPtrOffset = width * i * bytesPerPixel;

            void *dst = reinterpret_cast<uint8_t *>(mappedMemory) + dstPtrOffset;
            memcpy(dst, data + srcPtrOffset, rowSize);
        }

        EXPECT_EQ(0, AHardwareBuffer_unlock(aHardwareBuffer, nullptr));
        return aHardwareBuffer;
#else
        return nullptr;
#endif  // ANGLE_PLATFORM_ANDROID
    }

    void destroyAndroidHardwareBuffer(AHardwareBuffer *aHardwarebuffer)
    {
#if defined(ANGLE_AHARDWARE_BUFFER_SUPPORT)
        AHardwareBuffer_release(aHardwarebuffer);
#endif
    }

    void createEGLImageAndroidHardwareBufferSource(size_t width,
                                                   size_t height,
                                                   size_t depth,
                                                   GLenum sizedInternalFormat,
                                                   const EGLint *attribs,
                                                   const GLubyte *data,
                                                   size_t bytesPerPixel,
                                                   AHardwareBuffer **outSourceAHB,
                                                   EGLImageKHR *outSourceImage)
    {
        // Set Android Memory
        AHardwareBuffer *aHardwareBuffer = createAndroidHardwareBuffer(
            width, height, depth,
            angle::android::GLInternalFormatToNativePixelFormat(sizedInternalFormat), data,
            bytesPerPixel);
        EXPECT_NE(aHardwareBuffer, nullptr);

        // Create an image from the source AHB
        EGLWindow *window = getEGLWindow();

        EGLImageKHR image = eglCreateImageKHR(
            window->getDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
            angle::android::AHardwareBufferToClientBuffer(aHardwareBuffer), attribs);
        ASSERT_EGL_SUCCESS();

        *outSourceAHB   = aHardwareBuffer;
        *outSourceImage = image;
    }
    void createEGLImageTargetRenderbuffer(EGLImageKHR image, GLuint *outTargetRenderbuffer)
    {
        // Create a target texture from the image
        GLuint target;
        glGenRenderbuffers(1, &target);
        glBindRenderbuffer(GL_RENDERBUFFER, target);
        glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);

        ASSERT_GL_NO_ERROR();

        *outTargetRenderbuffer = target;
    }

    void ValidationGLEGLImage_helper(const EGLint *attribs);
    void SourceAHBTarget2D_helper(const EGLint *attribs);
    void SourceAHBTarget2DArray_helper(const EGLint *attribs);
    void SourceAHBTargetExternal_helper(const EGLint *attribs);
    void SourceAHBTargetExternalESSL3_helper(const EGLint *attribs);
    void Source2DTarget2D_helper(const EGLint *attribs);
    void Source2DTarget2DArray_helper(const EGLint *attribs);
    void Source2DTargetRenderbuffer_helper(const EGLint *attribs);
    void Source2DTargetExternal_helper(const EGLint *attribs);
    void Source2DTargetExternalESSL3_helper(const EGLint *attribs);
    void SourceCubeTarget2D_helper(const EGLint *attribs);
    void SourceCubeTargetRenderbuffer_helper(const EGLint *attribs);
    void SourceCubeTargetExternal_helper(const EGLint *attribs);
    void SourceCubeTargetExternalESSL3_helper(const EGLint *attribs);
    void Source3DTargetTexture_helper(const bool withColorspace);
    void Source3DTargetRenderbuffer_helper(const bool withColorspace);
    void Source3DTargetExternal_helper(const bool withColorspace);
    void Source3DTargetExternalESSL3_helper(const bool withColorspace);
    void SourceRenderbufferTargetTexture_helper(const EGLint *attribs);
    void SourceRenderbufferTargetTextureExternal_helper(const EGLint *attribs);
    void SourceRenderbufferTargetRenderbuffer_helper(const EGLint *attribs);
    void SourceRenderbufferTargetTextureExternalESSL3_helper(const EGLint *attribs);

    void verifyResultsTexture(GLuint texture,
                              GLubyte data[4],
                              GLenum textureTarget,
                              GLuint program,
                              GLuint textureUniform)
    {
        // Draw a quad with the target texture
        glUseProgram(program);
        glBindTexture(textureTarget, texture);
        glUniform1i(textureUniform, 0);

        drawQuad(program, "position", 0.5f);

        // Expect that the rendered quad has the same color as the source texture
        EXPECT_PIXEL_EQ(0, 0, data[0], data[1], data[2], data[3]);
    }

    void verifyResults2D(GLuint texture, GLubyte data[4])
    {
        verifyResultsTexture(texture, data, GL_TEXTURE_2D, mTextureProgram,
                             mTextureUniformLocation);
    }

    void verifyResults2DArray(GLuint texture, GLubyte data[4])
    {
        verifyResultsTexture(texture, data, GL_TEXTURE_2D_ARRAY, m2DArrayTextureProgram,
                             m2DArrayTextureUniformLocation);
    }

    void verifyResultsExternal(GLuint texture, GLubyte data[4])
    {
        verifyResultsTexture(texture, data, GL_TEXTURE_EXTERNAL_OES, mTextureExternalProgram,
                             mTextureExternalUniformLocation);
    }

    void verifyResultsExternalESSL3(GLuint texture, GLubyte data[4])
    {
        verifyResultsTexture(texture, data, GL_TEXTURE_EXTERNAL_OES, mTextureExternalESSL3Program,
                             mTextureExternalESSL3UniformLocation);
    }

    void verifyResultsRenderbuffer(GLuint renderbuffer, GLubyte data[4])
    {
        // Bind the renderbuffer to a framebuffer
        GLuint framebuffer;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderbuffer);

        // Expect that the rendered quad has the same color as the source texture
        EXPECT_PIXEL_EQ(0, 0, data[0], data[1], data[2], data[3]);

        glDeleteFramebuffers(1, &framebuffer);
    }

    void verifyResultAHB(AHardwareBuffer *source,
                         GLubyte *referenceData,
                         size_t dataSize,
                         size_t bytesPerPixel)
    {
#if defined(ANGLE_AHARDWARE_BUFFER_SUPPORT)
        void *mappedMemory = nullptr;
        GLubyte externalMemoryData[dataSize];
        AHardwareBuffer_Desc aHardwareBufferDescription = {};

        ASSERT_EQ(0, AHardwareBuffer_lock(source, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1,
                                          nullptr, &mappedMemory));

        AHardwareBuffer_describe(source, &aHardwareBufferDescription);
        const uint32_t width    = aHardwareBufferDescription.width;
        const uint32_t height   = aHardwareBufferDescription.height;
        const uint32_t stride   = aHardwareBufferDescription.stride;
        const uint32_t rowSize  = bytesPerPixel * width;
        const size_t bufferSize = rowSize * height;

        EXPECT_EQ(dataSize, bufferSize);

        for (uint32_t i = 0; i < height; i++)
        {
            uint32_t srcPtrOffset = stride * i * bytesPerPixel;
            uint32_t dstPtrOffset = width * i * bytesPerPixel;
            size_t copySize       = rowSize;

            if (dstPtrOffset > dataSize)
            {
                // Current destination ptr offset is out of range
                break;
            }
            else if (dstPtrOffset + copySize > dataSize)
            {
                // Copy data only until the end of the buffer
                copySize = dataSize - dstPtrOffset;
            }

            void *src = reinterpret_cast<uint8_t *>(mappedMemory) + srcPtrOffset;
            memcpy(externalMemoryData + dstPtrOffset, src, copySize);
        }

        ASSERT_EQ(0, AHardwareBuffer_unlock(source, nullptr));

        for (uint32_t i = 0; i < dataSize; i++)
        {
            EXPECT_EQ(externalMemoryData[i], referenceData[i]);
        }
#endif
    }

    template <typename destType, typename sourcetype>
    destType reinterpretHelper(sourcetype source)
    {
        static_assert(sizeof(destType) == sizeof(size_t),
                      "destType should be the same size as a size_t");
        size_t sourceSizeT = static_cast<size_t>(source);
        return reinterpret_cast<destType>(sourceSizeT);
    }

    bool hasImageGLColorspaceExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kImageGLColorspaceExt);
    }

    bool hasAndroidImageNativeBufferExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(),
                                            kEGLAndroidImageNativeBufferExt);
    }

    bool hasAndroidHardwareBufferSupport() const
    {
#if defined(ANGLE_AHARDWARE_BUFFER_SUPPORT)
        return true;
#else
        return false;
#endif
    }

    bool hasEglImageArrayExt() const { return IsGLExtensionEnabled(kEGLImageArrayExt); }

    bool hasOESExt() const { return IsGLExtensionEnabled(kOESExt); }

    bool hasExternalExt() const { return IsGLExtensionEnabled(kExternalExt); }

    bool hasExternalESSL3Ext() const { return IsGLExtensionEnabled(kExternalESSL3Ext); }

    bool hasBaseExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kBaseExt);
    }

    bool has2DTextureExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), k2DTextureExt);
    }

    bool has3DTextureExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), k3DTextureExt);
    }

    bool hasPixmapExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kPixmapExt);
    }

    bool hasRenderbufferExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kRenderbufferExt);
    }

    bool hasCubemapExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kCubemapExt);
    }

    EGLint *get3DAttributes(const bool withColorspace = false, EGLint layer = 0)
    {
        if (!withColorspace)
        {
            default3DAttribs[1] = static_cast<EGLint>(layer);
            return default3DAttribs;
        }

        colorspace3DAttribs[1] = static_cast<EGLint>(layer);
        return colorspace3DAttribs;
    }

    EGLint default3DAttribs[5] = {
        EGL_GL_TEXTURE_ZOFFSET_KHR, static_cast<EGLint>(0), EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE,
    };
    EGLint colorspace3DAttribs[7] = {
        EGL_GL_TEXTURE_ZOFFSET_KHR,
        static_cast<EGLint>(0),
        EGL_IMAGE_PRESERVED,
        EGL_TRUE,
        EGL_GL_COLORSPACE,
        EGL_GL_COLORSPACE_SRGB_KHR,
        EGL_NONE,
    };
    GLuint mTextureProgram;
    GLuint m2DArrayTextureProgram;
    GLint mTextureUniformLocation;
    GLuint m2DArrayTextureUniformLocation;

    GLuint mTextureExternalProgram        = 0;
    GLint mTextureExternalUniformLocation = -1;

    GLuint mTextureExternalESSL3Program        = 0;
    GLint mTextureExternalESSL3UniformLocation = -1;
};

class ImageTestES3 : public ImageTest
{};

// Tests that the extension is exposed on the platforms we think it should be. Please modify this as
// you change extension availability.
TEST_P(ImageTest, ANGLEExtensionAvailability)
{
    // EGL support is based on driver extension availability.
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsAndroid());
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsOzone());

    if (IsD3D11() || IsD3D9())
    {
        EXPECT_TRUE(hasOESExt());
        EXPECT_TRUE(hasExternalExt());
        EXPECT_TRUE(hasBaseExt());
        EXPECT_TRUE(has2DTextureExt());
        EXPECT_TRUE(hasRenderbufferExt());

        if (IsD3D11())
        {
            EXPECT_TRUE(hasCubemapExt());

            if (getClientMajorVersion() >= 3)
            {
                EXPECT_TRUE(hasExternalESSL3Ext());
            }
            else
            {
                EXPECT_FALSE(hasExternalESSL3Ext());
            }
        }
        else
        {
            EXPECT_FALSE(hasCubemapExt());
            EXPECT_FALSE(hasExternalESSL3Ext());
        }
    }
    else if (IsVulkan())
    {
        EXPECT_TRUE(hasOESExt());
        EXPECT_TRUE(hasExternalExt());
        EXPECT_TRUE(hasBaseExt());
        EXPECT_TRUE(has2DTextureExt());
        EXPECT_TRUE(hasCubemapExt());
        EXPECT_TRUE(hasRenderbufferExt());
        if (getClientMajorVersion() >= 3)
        {
            EXPECT_TRUE(hasExternalESSL3Ext());
        }
        else
        {
            EXPECT_FALSE(hasExternalESSL3Ext());
        }
    }
    else
    {
        EXPECT_FALSE(hasOESExt());
        EXPECT_FALSE(hasExternalExt());
        EXPECT_FALSE(hasExternalESSL3Ext());
        EXPECT_FALSE(hasBaseExt());
        EXPECT_FALSE(has2DTextureExt());
        EXPECT_FALSE(has3DTextureExt());
        EXPECT_FALSE(hasRenderbufferExt());
    }

    // These extensions are not yet available on any platform.
    EXPECT_FALSE(hasPixmapExt());
    EXPECT_FALSE(has3DTextureExt());
}

// Check validation from the EGL_KHR_image_base extension
TEST_P(ImageTest, ValidationImageBase)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLuint glTexture2D;
    glGenTextures(1, &glTexture2D);
    glBindTexture(GL_TEXTURE_2D, glTexture2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    EGLDisplay display        = window->getDisplay();
    EGLContext context        = window->getContext();
    EGLConfig config          = window->getConfig();
    EGLImageKHR image         = EGL_NO_IMAGE_KHR;
    EGLClientBuffer texture2D = reinterpretHelper<EGLClientBuffer>(glTexture2D);

    // Test validation of eglCreateImageKHR

    // If <dpy> is not the handle of a valid EGLDisplay object, the error EGL_BAD_DISPLAY is
    // generated.
    image = eglCreateImageKHR(reinterpretHelper<EGLDisplay>(0xBAADF00D), context,
                              EGL_GL_TEXTURE_2D_KHR, texture2D, nullptr);
    EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    // If <ctx> is neither the handle of a valid EGLContext object on <dpy> nor EGL_NO_CONTEXT, the
    // error EGL_BAD_CONTEXT is generated.
    image = eglCreateImageKHR(display, reinterpretHelper<EGLContext>(0xBAADF00D),
                              EGL_GL_TEXTURE_2D_KHR, texture2D, nullptr);
    EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_ERROR(EGL_BAD_CONTEXT);

    // Test EGL_NO_CONTEXT with a 2D texture target which does require a context.
    image = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_GL_TEXTURE_2D_KHR, texture2D, nullptr);
    EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_ERROR(EGL_BAD_CONTEXT);

    // If an attribute specified in <attrib_list> is not one of the attributes listed in Table bbb,
    // the error EGL_BAD_PARAMETER is generated.
    EGLint badAttributes[] = {
        static_cast<EGLint>(0xDEADBEEF),
        0,
        EGL_NONE,
    };

    image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, texture2D, badAttributes);
    EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // If the resource specified by <dpy>, <ctx>, <target>, <buffer> and <attrib_list> has an off -
    // screen buffer bound to it(e.g., by a
    // previous call to eglBindTexImage), the error EGL_BAD_ACCESS is generated.
    EGLint surfaceType = 0;
    eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &surfaceType);

    EGLint bindToTextureRGBA = 0;
    eglGetConfigAttrib(display, config, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
    if ((surfaceType & EGL_PBUFFER_BIT) != 0 && bindToTextureRGBA == EGL_TRUE)
    {
        EGLint pbufferAttributes[] = {
            EGL_WIDTH,          1,
            EGL_HEIGHT,         1,
            EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
            EGL_NONE,           EGL_NONE,
        };
        EGLSurface pbuffer = eglCreatePbufferSurface(display, config, pbufferAttributes);
        ASSERT_NE(pbuffer, EGL_NO_SURFACE);
        EXPECT_EGL_SUCCESS();

        eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
        EXPECT_EGL_SUCCESS();

        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, texture2D, nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_ACCESS);

        eglReleaseTexImage(display, pbuffer, EGL_BACK_BUFFER);
        eglDestroySurface(display, pbuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_EGL_SUCCESS();
        EXPECT_GL_NO_ERROR();
    }

    // If the resource specified by <dpy>, <ctx>, <target>, <buffer> and
    // <attrib_list> is itself an EGLImage sibling, the error EGL_BAD_ACCESS is generated.
    image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, texture2D, nullptr);
    EXPECT_NE(image, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_SUCCESS();

    /* TODO(geofflang): Enable this validation when it passes.
    EGLImageKHR image2 = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
    reinterpret_cast<EGLClientBuffer>(texture2D), nullptr);
    EXPECT_EQ(image2, EGL_NO_IMAGE_KHR);
    EXPECT_EGL_ERROR(EGL_BAD_ACCESS);
    */

    // Test validation of eglDestroyImageKHR
    // Note: image is now a valid EGL image
    EGLBoolean result = EGL_FALSE;

    // If <dpy> is not the handle of a valid EGLDisplay object, the error EGL_BAD_DISPLAY is
    // generated.
    result = eglDestroyImageKHR(reinterpretHelper<EGLDisplay>(0xBAADF00D), image);
    EXPECT_EQ(result, static_cast<EGLBoolean>(EGL_FALSE));
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    // If <image> is not a valid EGLImageKHR object created with respect to <dpy>, the error
    // EGL_BAD_PARAMETER is generated.
    result = eglDestroyImageKHR(display, reinterpretHelper<EGLImageKHR>(0xBAADF00D));
    EXPECT_EQ(result, static_cast<EGLBoolean>(EGL_FALSE));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Clean up and validate image is destroyed
    result = eglDestroyImageKHR(display, image);
    EXPECT_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
    EXPECT_EGL_SUCCESS();

    glDeleteTextures(1, &glTexture2D);
    EXPECT_GL_NO_ERROR();
}

// Check validation from the EGL_KHR_gl_texture_2D_image, EGL_KHR_gl_texture_cubemap_image,
// EGL_KHR_gl_texture_3D_image and EGL_KHR_gl_renderbuffer_image extensions
TEST_P(ImageTest, ValidationGLImage)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());

    EGLDisplay display = window->getDisplay();
    EGLContext context = window->getContext();
    EGLImageKHR image  = EGL_NO_IMAGE_KHR;

    if (has2DTextureExt())
    {
        // If <target> is EGL_GL_TEXTURE_2D_KHR, EGL_GL_TEXTURE_CUBE_MAP_*_KHR or
        // EGL_GL_TEXTURE_3D_KHR and <buffer> is not the name of a texture object of type <target>,
        // the error EGL_BAD_PARAMETER is generated.
        GLuint textureCube;
        glGenTextures(1, &textureCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureCube);
        for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
             face++)
        {
            glTexImage2D(face, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }

        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(textureCube), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        // If EGL_GL_TEXTURE_LEVEL_KHR is 0, <target> is EGL_GL_TEXTURE_2D_KHR,
        // EGL_GL_TEXTURE_CUBE_MAP_*_KHR or EGL_GL_TEXTURE_3D_KHR, <buffer> is the name of an
        // incomplete GL texture object, and any mipmap levels other than mipmap level 0 are
        // specified, the error EGL_BAD_PARAMETER is generated.
        GLuint incompleteTexture;
        glGenTextures(1, &incompleteTexture);
        glBindTexture(GL_TEXTURE_2D, incompleteTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        EGLint level0Attribute[] = {
            EGL_GL_TEXTURE_LEVEL_KHR,
            0,
            EGL_NONE,
        };
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(incompleteTexture),
                                  level0Attribute);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        // If EGL_GL_TEXTURE_LEVEL_KHR is 0, <target> is EGL_GL_TEXTURE_2D_KHR or
        // EGL_GL_TEXTURE_3D_KHR, <buffer> is not the name of a complete GL texture object, and
        // mipmap level 0 is not specified, the error EGL_BAD_PARAMETER is generated.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(incompleteTexture), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        // If <target> is EGL_GL_TEXTURE_2D_KHR, EGL_GL_TEXTURE_CUBE_MAP_*_KHR,
        // EGL_GL_RENDERBUFFER_KHR or EGL_GL_TEXTURE_3D_KHR and <buffer> refers to the default GL
        // texture object(0) for the corresponding GL target, the error EGL_BAD_PARAMETER is
        // generated.
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, 0, nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        // If <target> is EGL_GL_TEXTURE_2D_KHR, EGL_GL_TEXTURE_CUBE_MAP_*_KHR, or
        // EGL_GL_TEXTURE_3D_KHR, and the value specified in <attr_list> for
        // EGL_GL_TEXTURE_LEVEL_KHR is not a valid mipmap level for the specified GL texture object
        // <buffer>, the error EGL_BAD_MATCH is generated.
        EGLint level2Attribute[] = {
            EGL_GL_TEXTURE_LEVEL_KHR,
            2,
            EGL_NONE,
        };
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(incompleteTexture),
                                  level2Attribute);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }
    else
    {
        GLuint texture2D;
        glGenTextures(1, &texture2D);
        glBindTexture(GL_TEXTURE_2D, texture2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // From EGL_KHR_image_base:
        // If <target> is not one of the values in Table aaa, the error EGL_BAD_PARAMETER is
        // generated.
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(texture2D), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }

    if (hasCubemapExt())
    {
        // If EGL_GL_TEXTURE_LEVEL_KHR is 0, <target> is EGL_GL_TEXTURE_CUBE_MAP_*_KHR, <buffer> is
        // not the name of a complete GL texture object, and one or more faces do not have mipmap
        // level 0 specified, the error EGL_BAD_PARAMETER is generated.
        GLuint incompleteTextureCube;
        glGenTextures(1, &incompleteTextureCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, incompleteTextureCube);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);

        EGLint level0Attribute[] = {
            EGL_GL_TEXTURE_LEVEL_KHR,
            0,
            EGL_NONE,
        };
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR,
                                  reinterpretHelper<EGLClientBuffer>(incompleteTextureCube),
                                  level0Attribute);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }
    else
    {
        GLuint textureCube;
        glGenTextures(1, &textureCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureCube);
        for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
             face++)
        {
            glTexImage2D(face, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }

        // From EGL_KHR_image_base:
        // If <target> is not one of the values in Table aaa, the error EGL_BAD_PARAMETER is
        // generated.
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR,
                                  reinterpretHelper<EGLClientBuffer>(textureCube), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }

    if (has3DTextureExt() && getClientMajorVersion() >= 3)
    {
        // If <target> is EGL_GL_TEXTURE_3D_KHR, and the value specified in <attr_list> for
        // EGL_GL_TEXTURE_ZOFFSET_KHR exceeds the depth of the specified mipmap level - of - detail
        // in <buffer>, the error EGL_BAD_PARAMETER is generated.
        GLuint texture3D;
        glGenTextures(1, &texture3D);
        glBindTexture(GL_TEXTURE_3D, texture3D);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        EGLint zOffset3Parameter[] = {
            EGL_GL_TEXTURE_ZOFFSET_KHR,
            3,
            EGL_NONE,
        };
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_3D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(texture3D), zOffset3Parameter);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        EGLint zOffsetNegative1Parameter[] = {
            EGL_GL_TEXTURE_ZOFFSET_KHR,
            -1,
            EGL_NONE,
        };
        image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_3D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(texture3D),
                                  zOffsetNegative1Parameter);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }
    else
    {
        if (has2DTextureExt())
        {
            GLuint texture2D;
            glGenTextures(1, &texture2D);
            glBindTexture(GL_TEXTURE_2D, texture2D);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            // Verify EGL_GL_TEXTURE_ZOFFSET_KHR is not a valid parameter
            EGLint zOffset0Parameter[] = {
                EGL_GL_TEXTURE_ZOFFSET_KHR,
                0,
                EGL_NONE,
            };

            image =
                eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                                  reinterpretHelper<EGLClientBuffer>(texture2D), zOffset0Parameter);
            EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
            EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
        }

        if (getClientMajorVersion() >= 3)
        {
            GLuint texture3D;
            glGenTextures(1, &texture3D);
            glBindTexture(GL_TEXTURE_3D, texture3D);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            // From EGL_KHR_image_base:
            // If <target> is not one of the values in Table aaa, the error EGL_BAD_PARAMETER is
            // generated.
            image = eglCreateImageKHR(display, context, EGL_GL_TEXTURE_3D_KHR,
                                      reinterpretHelper<EGLClientBuffer>(texture3D), nullptr);
            EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
            EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
        }
    }

    if (hasRenderbufferExt())
    {
        // If <target> is EGL_GL_RENDERBUFFER_KHR and <buffer> is not the name of a renderbuffer
        // object, or if <buffer> is the name of a multisampled renderbuffer object, the error
        // EGL_BAD_PARAMETER is generated.
        image = eglCreateImageKHR(display, context, EGL_GL_RENDERBUFFER_KHR,
                                  reinterpret_cast<EGLClientBuffer>(0), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

        if (IsGLExtensionEnabled("GL_ANGLE_framebuffer_multisample"))
        {
            GLuint renderbuffer;
            glGenRenderbuffers(1, &renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
            glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 1, GL_RGBA8, 1, 1);
            EXPECT_GL_NO_ERROR();

            image = eglCreateImageKHR(display, context, EGL_GL_RENDERBUFFER_KHR,
                                      reinterpret_cast<EGLClientBuffer>(0), nullptr);
            EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
            EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
        }
    }
    else
    {
        GLuint renderbuffer;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);

        // From EGL_KHR_image_base:
        // If <target> is not one of the values in Table aaa, the error EGL_BAD_PARAMETER is
        // generated.
        image = eglCreateImageKHR(display, context, EGL_GL_RENDERBUFFER_KHR,
                                  reinterpretHelper<EGLClientBuffer>(renderbuffer), nullptr);
        EXPECT_EQ(image, EGL_NO_IMAGE_KHR);
        EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
    }
}

// Check validation from the GL_OES_EGL_image extension
TEST_P(ImageTest, ValidationGLEGLImage)
{
    ValidationGLEGLImage_helper(kDefaultAttribs);
}

TEST_P(ImageTest, ValidationGLEGLImage_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    ValidationGLEGLImage_helper(kColorspaceAttribs);
}

void ImageTest::ValidationGLEGLImage_helper(const EGLint *attribs)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // If <target> is not TEXTURE_2D, the error INVALID_ENUM is generated.
    glEGLImageTargetTexture2DOES(GL_TEXTURE_CUBE_MAP_POSITIVE_X, image);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // If <image> does not refer to a valid eglImageOES object, the error INVALID_VALUE is
    // generated.
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, reinterpretHelper<GLeglImageOES>(0xBAADF00D));
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // <target> must be RENDERBUFFER_OES, and <image> must be the handle of a valid EGLImage
    // resource, cast into the type
    // eglImageOES.
    glEGLImageTargetRenderbufferStorageOES(GL_TEXTURE_2D, image);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // If the GL is unable to create a renderbuffer using the specified eglImageOES, the error
    // INVALID_OPERATION is generated.If <image>
    // does not refer to a valid eglImageOES object, the error INVALID_VALUE is generated.
    GLuint renderbuffer;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
                                           reinterpretHelper<GLeglImageOES>(0xBAADF00D));
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(getEGLWindow()->getDisplay(), image);
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &renderbuffer);
}

// Check validation from the GL_OES_EGL_image_external extension
TEST_P(ImageTest, ValidationGLEGLImageExternal)
{
    ANGLE_SKIP_TEST_IF(!hasExternalExt());

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);

    // In the initial state of a TEXTURE_EXTERNAL_OES texture object, the value assigned to
    // TEXTURE_MIN_FILTER and TEXTURE_MAG_FILTER is LINEAR, and the s and t wrap modes are both set
    // to CLAMP_TO_EDGE
    auto getTexParam = [](GLenum target, GLenum pname) {
        GLint value = 0;
        glGetTexParameteriv(target, pname, &value);
        EXPECT_GL_NO_ERROR();
        return value;
    };
    EXPECT_GLENUM_EQ(GL_LINEAR, getTexParam(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER));
    EXPECT_GLENUM_EQ(GL_LINEAR, getTexParam(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER));
    EXPECT_GLENUM_EQ(GL_CLAMP_TO_EDGE, getTexParam(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S));
    EXPECT_GLENUM_EQ(GL_CLAMP_TO_EDGE, getTexParam(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T));

    // "When <target> is TEXTURE_EXTERNAL_OES only NEAREST and LINEAR are accepted as
    // TEXTURE_MIN_FILTER, only CLAMP_TO_EDGE is accepted as TEXTURE_WRAP_S and TEXTURE_WRAP_T, and
    // only FALSE is accepted as GENERATE_MIPMAP. Attempting to set other values for
    // TEXTURE_MIN_FILTER, TEXTURE_WRAP_S, TEXTURE_WRAP_T, or GENERATE_MIPMAP will result in an
    // INVALID_ENUM error.
    GLenum validMinFilters[]{
        GL_NEAREST,
        GL_LINEAR,
    };
    for (auto minFilter : validMinFilters)
    {
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, minFilter);
        EXPECT_GL_NO_ERROR();
    }

    GLenum invalidMinFilters[]{
        GL_NEAREST_MIPMAP_LINEAR,
        GL_NEAREST_MIPMAP_NEAREST,
        GL_LINEAR_MIPMAP_LINEAR,
        GL_LINEAR_MIPMAP_NEAREST,
    };
    for (auto minFilter : invalidMinFilters)
    {
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, minFilter);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }

    GLenum validWrapModes[]{
        GL_CLAMP_TO_EDGE,
    };
    for (auto wrapMode : validWrapModes)
    {
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, wrapMode);
        EXPECT_GL_NO_ERROR();
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, wrapMode);
        EXPECT_GL_NO_ERROR();
    }

    if (IsGLExtensionEnabled("GL_EXT_EGL_image_external_wrap_modes"))
    {
        GLenum validWrapModesEXT[]{GL_REPEAT, GL_MIRRORED_REPEAT};
        for (auto wrapMode : validWrapModesEXT)
        {
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, wrapMode);
            EXPECT_GL_NO_ERROR();
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, wrapMode);
            EXPECT_GL_NO_ERROR();
        }
    }
    else
    {
        GLenum invalidWrapModes[]{
            GL_REPEAT,
            GL_MIRRORED_REPEAT,
        };
        for (auto wrapMode : invalidWrapModes)
        {
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, wrapMode);
            EXPECT_GL_ERROR(GL_INVALID_ENUM);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, wrapMode);
            EXPECT_GL_ERROR(GL_INVALID_ENUM);
        }
    }

    // When <target> is set to TEXTURE_EXTERNAL_OES, GenerateMipmap always fails and generates an
    // INVALID_ENUM error.
    glGenerateMipmap(GL_TEXTURE_EXTERNAL_OES);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glDeleteTextures(1, &texture);
}

// Check validation from the GL_OES_EGL_image_external_essl3 extension
TEST_P(ImageTest, ValidationGLEGLImageExternalESSL3)
{
    ANGLE_SKIP_TEST_IF(!hasExternalESSL3Ext());

    // Make sure this extension is not exposed without ES3.
    ASSERT_GE(getClientMajorVersion(), 3);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);

    // It is an INVALID_OPERATION error to set the TEXTURE_BASE_LEVEL to a value other than zero.
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_BASE_LEVEL, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_BASE_LEVEL, 10);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_BASE_LEVEL, 0);
    EXPECT_GL_NO_ERROR();

    glDeleteTextures(1, &texture);
}

TEST_P(ImageTest, Source2DTarget2D)
{
    Source2DTarget2D_helper(kDefaultAttribs);
}

TEST_P(ImageTest, Source2DTarget2D_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source2DTarget2D_helper(kColorspaceAttribs);
}

void ImageTest::Source2DTarget2D_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Expect that the target texture has the same color as the source texture
    verifyResults2D(target, data);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Testing source 2D texture, target 2D array texture
TEST_P(ImageTest, Source2DTarget2DArray)
{
    Source2DTarget2DArray_helper(kDefaultAttribs);
}

// Testing source 2D texture with colorspace, target 2D array texture
TEST_P(ImageTest, Source2DTarget2DArray_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source2DTarget2DArray_helper(kColorspaceAttribs);
}

void ImageTest::Source2DTarget2DArray_helper(const EGLint *attribs)
{
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() ||
                       !hasEglImageArrayExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2DArray(image, &target);

    // Expect that the target texture has the same color as the source texture
    verifyResults2DArray(target, data);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Testing source AHB EGL image, target 2D texture
TEST_P(ImageTest, SourceAHBTarget2D)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    SourceAHBTarget2D_helper(kDefaultAttribs);
}

// Testing source AHB EGL image with colorspace, target 2D texture
TEST_P(ImageTest, SourceAHBTarget2D_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceAHBTarget2D_helper(kColorspaceAttribs);
}

void ImageTest::SourceAHBTarget2D_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();

    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    GLubyte data[4] = {7, 51, 197, 231};

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(1, 1, 1, GL_RGBA8, attribs, data, 4, &source, &image);

    // Create a texture target to bind the egl image
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResults2D(target, data);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
    glDeleteTextures(1, &target);
}

// Testing source AHB EGL image, target 2D array texture
TEST_P(ImageTest, SourceAHBTarget2DArray)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    SourceAHBTarget2DArray_helper(kDefaultAttribs);
}

// Testing source AHB EGL image with colorspace, target 2D array texture
TEST_P(ImageTest, SourceAHBTarget2DArray_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceAHBTarget2DArray_helper(kColorspaceAttribs);
}

void ImageTest::SourceAHBTarget2DArray_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();

    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() ||
                       !hasEglImageArrayExt());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    GLubyte data[4] = {7, 51, 197, 231};

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(1, 1, 1, GL_RGBA8, attribs, data, 4, &source, &image);

    // Create a texture target to bind the egl image
    GLuint target;
    createEGLImageTargetTexture2DArray(image, &target);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResults2DArray(target, data);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
    glDeleteTextures(1, &target);
}

// Testing source AHB EGL image, target external texture
TEST_P(ImageTest, SourceAHBTargetExternal)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    SourceAHBTargetExternal_helper(kDefaultAttribs);
}

// Testing source AHB EGL image with colorspace, target external texture
TEST_P(ImageTest, SourceAHBTargetExternal_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceAHBTargetExternal_helper(kColorspaceAttribs);
}

void ImageTest::SourceAHBTargetExternal_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() || !hasExternalExt());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    // Ozone only supports external target for images created with EGL_EXT_image_dma_buf_import
    ANGLE_SKIP_TEST_IF(IsOzone());

    GLubyte data[4] = {7, 51, 197, 231};

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(1, 1, 1, GL_RGBA8, attribs, data, 4, &source, &image);

    // Create a texture target to bind the egl image
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResultsExternal(target, data);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
    glDeleteTextures(1, &target);
}

// Testing source AHB EGL image, target external ESSL3 texture
TEST_P(ImageTestES3, SourceAHBTargetExternalESSL3)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    SourceAHBTargetExternalESSL3_helper(kDefaultAttribs);
}

// Testing source AHB EGL image with colorspace, target external ESSL3 texture
TEST_P(ImageTestES3, SourceAHBTargetExternalESSL3_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceAHBTargetExternalESSL3_helper(kColorspaceAttribs);
}

void ImageTest::SourceAHBTargetExternalESSL3_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() ||
                       !hasExternalESSL3Ext());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    GLubyte data[4] = {7, 51, 197, 231};

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(1, 1, 1, GL_RGBA8, attribs, data, 4, &source, &image);

    // Create a texture target to bind the egl image
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResultsExternalESSL3(target, data);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
    glDeleteTextures(1, &target);
}

// Create a depth format AHB backed EGL image and verify that the image's aspect is honored
TEST_P(ImageTest, SourceAHBTarget2DDepth)
{
    // TODO - Support for depth formats in AHB is missing (http://anglebug.com/4818)
    ANGLE_SKIP_TEST_IF(true);

    EGLWindow *window = getEGLWindow();

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    GLint level             = 0;
    GLsizei width           = 1;
    GLsizei height          = 1;
    GLsizei depth           = 1;
    GLint depthStencilValue = 0;

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(
        width, height, depth, GL_DEPTH_COMPONENT24, kDefaultAttribs,
        reinterpret_cast<GLubyte *>(&depthStencilValue), 3, &source, &image);

    // Create a texture target to bind the egl image
    GLuint depthTextureTarget;
    createEGLImageTargetTexture2D(image, &depthTextureTarget);

    // Create a color texture and fill it with red
    GLTexture colorTexture;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 GLColor::red.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_NO_ERROR();

    // Attach the color and depth texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureTarget,
                           0);
    EXPECT_GL_NO_ERROR();

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Clear the color texture to red
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Enable Depth test but disable depth writes. The depth function is set to ">".
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_GREATER);

    // Fill any fragment of the color attachment with blue if it passes the depth test.
    ANGLE_GL_PROGRAM(colorFillProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    drawQuad(colorFillProgram, essl1_shaders::PositionAttrib(), 1.0f, 1.0f);

    // Since 1.0f > 0.0f, all fragments of the color attachment should be blue.
    EXPECT_PIXEL_EQ(0, 0, 0, 0, 255, 255);

    // Clean up
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
}

TEST_P(ImageTest, Source2DTargetRenderbuffer)
{
    Source2DTargetRenderbuffer_helper(kDefaultAttribs);
}

TEST_P(ImageTest, Source2DTargetRenderbuffer_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source2DTargetRenderbuffer_helper(kColorspaceAttribs);
}

void ImageTest::Source2DTargetRenderbuffer_helper(const EGLint *attribs)
{

    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetRenderbuffer(image, &target);

    // Expect that the target renderbuffer has the same color as the source texture
    verifyResultsRenderbuffer(target, data);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteRenderbuffers(1, &target);
}

TEST_P(ImageTest, Source2DTargetExternal)
{
    Source2DTargetExternal_helper(kDefaultAttribs);
}

TEST_P(ImageTest, Source2DTargetExternal_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source2DTargetExternal_helper(kColorspaceAttribs);
}

void ImageTest::Source2DTargetExternal_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() || !hasExternalExt());

    // Ozone only supports external target for images created with EGL_EXT_image_dma_buf_import
    ANGLE_SKIP_TEST_IF(IsOzone());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Expect that the target renderbuffer has the same color as the source texture
    verifyResultsExternal(target, data);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteRenderbuffers(1, &target);
}

TEST_P(ImageTestES3, Source2DTargetExternalESSL3)
{
    Source2DTargetExternalESSL3_helper(kDefaultAttribs);
}

TEST_P(ImageTestES3, Source2DTargetExternalESSL3_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source2DTargetExternalESSL3_helper(kColorspaceAttribs);
}

void ImageTest::Source2DTargetExternalESSL3_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt() ||
                       !hasExternalESSL3Ext());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Expect that the target renderbuffer has the same color as the source texture
    verifyResultsExternalESSL3(target, data);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteRenderbuffers(1, &target);
}

TEST_P(ImageTest, SourceCubeTarget2D)
{
    SourceCubeTarget2D_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceCubeTarget2D_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceCubeTarget2D_helper(kColorspaceAttribs);
}

void ImageTest::SourceCubeTarget2D_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !hasCubemapExt());

    GLubyte data[24] = {
        255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255,
        0,   0, 255, 255, 0,   255, 0,   255, 0,   0, 0, 255,
    };

    for (EGLenum faceIdx = 0; faceIdx < 6; faceIdx++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImageCubemapTextureSource(
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, reinterpret_cast<uint8_t *>(data),
            sizeof(GLubyte) * 4, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR + faceIdx, &source, &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTexture2D(image, &target);

        // Expect that the target texture has the same color as the source texture
        verifyResults2D(target, &data[faceIdx * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &target);
    }
}

TEST_P(ImageTest, SourceCubeTargetRenderbuffer)
{
    SourceCubeTargetRenderbuffer_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceCubeTargetRenderbuffer_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceCubeTargetRenderbuffer_helper(kColorspaceAttribs);
}

void ImageTest::SourceCubeTargetRenderbuffer_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !hasCubemapExt());

    // http://anglebug.com/3145
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsIntel() && IsFuchsia());

    GLubyte data[24] = {
        255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255,
        0,   0, 255, 255, 0,   255, 0,   255, 0,   0, 0, 255,
    };

    for (EGLenum faceIdx = 0; faceIdx < 6; faceIdx++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImageCubemapTextureSource(
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, reinterpret_cast<uint8_t *>(data),
            sizeof(GLubyte) * 4, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR + faceIdx, &source, &image);

        // Create the target
        GLuint target;
        createEGLImageTargetRenderbuffer(image, &target);

        // Expect that the target texture has the same color as the source texture
        verifyResultsRenderbuffer(target, &data[faceIdx * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteRenderbuffers(1, &target);
    }
}

// Test cubemap -> external texture EGL images.
TEST_P(ImageTest, SourceCubeTargetExternal)
{
    SourceCubeTargetExternal_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceCubeTargetExternal_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceCubeTargetExternal_helper(kColorspaceAttribs);
}

void ImageTest::SourceCubeTargetExternal_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !hasCubemapExt() || !hasExternalExt());

    // Ozone only supports external target for images created with EGL_EXT_image_dma_buf_import
    ANGLE_SKIP_TEST_IF(IsOzone());

    GLubyte data[24] = {
        255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255,
        0,   0, 255, 255, 0,   255, 0,   255, 0,   0, 0, 255,
    };

    for (EGLenum faceIdx = 0; faceIdx < 6; faceIdx++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImageCubemapTextureSource(
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, reinterpret_cast<uint8_t *>(data),
            sizeof(GLubyte) * 4, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR + faceIdx, &source, &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTextureExternal(image, &target);

        // Expect that the target texture has the same color as the source texture
        verifyResultsExternal(target, &data[faceIdx * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteRenderbuffers(1, &target);
    }
}

// Test cubemap -> external texture EGL images using ESSL3 shaders.
TEST_P(ImageTestES3, SourceCubeTargetExternalESSL3)
{
    SourceCubeTargetExternalESSL3_helper(kDefaultAttribs);
}

TEST_P(ImageTestES3, SourceCubeTargetExternalESSL3_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceCubeTargetExternalESSL3_helper(kColorspaceAttribs);
}

void ImageTest::SourceCubeTargetExternalESSL3_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasExternalESSL3Ext() || !hasBaseExt() || !hasCubemapExt());

    GLubyte data[24] = {
        255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255,
        0,   0, 255, 255, 0,   255, 0,   255, 0,   0, 0, 255,
    };

    for (EGLenum faceIdx = 0; faceIdx < 6; faceIdx++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImageCubemapTextureSource(
            1, 1, GL_RGBA, GL_UNSIGNED_BYTE, attribs, reinterpret_cast<uint8_t *>(data),
            sizeof(GLubyte) * 4, EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR + faceIdx, &source, &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTextureExternal(image, &target);

        // Expect that the target texture has the same color as the source texture
        verifyResultsExternalESSL3(target, &data[faceIdx * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteRenderbuffers(1, &target);
    }
}

TEST_P(ImageTest, Source3DTargetTexture)
{
    Source3DTargetTexture_helper(false);
}

TEST_P(ImageTest, Source3DTargetTexture_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source3DTargetTexture_helper(true);
}

void ImageTest::Source3DTargetTexture_helper(const bool withColorspace)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has3DTextureExt());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_OES_texture_3D"));

    const size_t depth      = 2;
    GLubyte data[4 * depth] = {
        255, 0, 255, 255, 255, 255, 0, 255,
    };

    for (size_t layer = 0; layer < depth; layer++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImage3DTextureSource(1, 1, depth, GL_RGBA, GL_UNSIGNED_BYTE,
                                      get3DAttributes(withColorspace, layer), data, &source,
                                      &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTexture2D(image, &target);

        // Expect that the target renderbuffer has the same color as the source texture
        verifyResults2D(target, &data[layer * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &target);
    }
}

TEST_P(ImageTest, Source3DTargetRenderbuffer)
{
    Source3DTargetRenderbuffer_helper(false);
}

TEST_P(ImageTest, Source3DTargetRenderbuffer_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source3DTargetRenderbuffer_helper(true);
}

void ImageTest::Source3DTargetRenderbuffer_helper(const bool withColorspace)
{
    // Qualcom drivers appear to always bind the 0 layer of the source 3D texture when the
    // target is a renderbuffer. They work correctly when the target is a 2D texture.
    // http://anglebug.com/2745
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has3DTextureExt());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_OES_texture_3D"));

    const size_t depth      = 2;
    GLubyte data[4 * depth] = {
        255, 0, 255, 255, 255, 255, 0, 255,
    };

    for (size_t layer = 0; layer < depth; layer++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;

        createEGLImage3DTextureSource(1, 1, depth, GL_RGBA, GL_UNSIGNED_BYTE,
                                      get3DAttributes(withColorspace, layer), data, &source,
                                      &image);

        // Create the target
        GLuint target;
        createEGLImageTargetRenderbuffer(image, &target);

        // Expect that the target renderbuffer has the same color as the source texture
        verifyResultsRenderbuffer(target, &data[layer * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &target);
    }
}

// Test 3D -> external texture EGL images.
TEST_P(ImageTest, Source3DTargetExternal)
{
    Source3DTargetExternal_helper(false);
}

TEST_P(ImageTest, Source3DTargetExternal_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source3DTargetExternal_helper(true);
}

void ImageTest::Source3DTargetExternal_helper(const bool withColorspace)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasExternalExt() || !hasBaseExt() || !has3DTextureExt());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_OES_texture_3D"));

    // Ozone only supports external target for images created with EGL_EXT_image_dma_buf_import
    ANGLE_SKIP_TEST_IF(IsOzone());

    const size_t depth      = 2;
    GLubyte data[4 * depth] = {
        255, 0, 255, 255, 255, 255, 0, 255,
    };

    for (size_t layer = 0; layer < depth; layer++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;
        createEGLImage3DTextureSource(1, 1, depth, GL_RGBA, GL_UNSIGNED_BYTE,
                                      get3DAttributes(withColorspace, layer), data, &source,
                                      &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTextureExternal(image, &target);

        // Expect that the target renderbuffer has the same color as the source texture
        verifyResultsExternal(target, &data[layer * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &target);
    }
}

// Test 3D -> external texture EGL images using ESSL3 shaders.
TEST_P(ImageTestES3, Source3DTargetExternalESSL3)
{
    Source3DTargetExternalESSL3_helper(false);
}

TEST_P(ImageTestES3, Source3DTargetExternalESSL3_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    Source3DTargetExternalESSL3_helper(true);
}

void ImageTest::Source3DTargetExternalESSL3_helper(const bool withColorspace)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasExternalESSL3Ext() || !hasBaseExt() ||
                       !has3DTextureExt());

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_OES_texture_3D"));

    const size_t depth      = 2;
    GLubyte data[4 * depth] = {
        255, 0, 255, 255, 255, 255, 0, 255,
    };

    for (size_t layer = 0; layer < depth; layer++)
    {
        // Create the Image
        GLuint source;
        EGLImageKHR image;

        createEGLImage3DTextureSource(1, 1, depth, GL_RGBA, GL_UNSIGNED_BYTE,
                                      get3DAttributes(withColorspace, layer), data, &source,
                                      &image);

        // Create the target
        GLuint target;
        createEGLImageTargetTextureExternal(image, &target);

        // Expect that the target renderbuffer has the same color as the source texture
        verifyResultsExternalESSL3(target, &data[layer * 4]);

        // Clean up
        glDeleteTextures(1, &source);
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &target);
    }
}

TEST_P(ImageTest, SourceRenderbufferTargetTexture)
{
    SourceRenderbufferTargetTexture_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceRenderbufferTargetTexture_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceRenderbufferTargetTexture_helper(kColorspaceAttribs);
}

void ImageTest::SourceRenderbufferTargetTexture_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !hasRenderbufferExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImageRenderbufferSource(1, 1, GL_RGBA8_OES, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Expect that the target texture has the same color as the source texture
    verifyResults2D(target, data);

    // Clean up
    glDeleteRenderbuffers(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Test renderbuffer -> external texture EGL images.
TEST_P(ImageTest, SourceRenderbufferTargetTextureExternal)
{
    SourceRenderbufferTargetTextureExternal_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceRenderbufferTargetTextureExternal_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceRenderbufferTargetTextureExternal_helper(kColorspaceAttribs);
}

void ImageTest::SourceRenderbufferTargetTextureExternal_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasExternalExt() || !hasBaseExt() || !hasRenderbufferExt());

    // Ozone only supports external target for images created with EGL_EXT_image_dma_buf_import
    ANGLE_SKIP_TEST_IF(IsOzone());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImageRenderbufferSource(1, 1, GL_RGBA8_OES, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Expect that the target texture has the same color as the source texture
    verifyResultsExternal(target, data);

    // Clean up
    glDeleteRenderbuffers(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Test renderbuffer -> external texture EGL images using ESSL3 shaders.
TEST_P(ImageTestES3, SourceRenderbufferTargetTextureExternalESSL3)
{
    SourceRenderbufferTargetTextureExternalESSL3_helper(kDefaultAttribs);
}

TEST_P(ImageTestES3, SourceRenderbufferTargetTextureExternalESSL3_Colorspace)
{
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceRenderbufferTargetTextureExternalESSL3_helper(kColorspaceAttribs);
}

void ImageTest::SourceRenderbufferTargetTextureExternalESSL3_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasExternalESSL3Ext() || !hasBaseExt() ||
                       !hasRenderbufferExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImageRenderbufferSource(1, 1, GL_RGBA8_OES, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTextureExternal(image, &target);

    // Expect that the target texture has the same color as the source texture
    verifyResultsExternalESSL3(target, data);

    // Clean up
    glDeleteRenderbuffers(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

TEST_P(ImageTest, SourceRenderbufferTargetRenderbuffer)
{
    SourceRenderbufferTargetRenderbuffer_helper(kDefaultAttribs);
}

TEST_P(ImageTest, SourceRenderbufferTargetRenderbuffer_Colorspace)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 && !IsGLExtensionEnabled("GL_EXT_sRGB"));
    ANGLE_SKIP_TEST_IF(!hasImageGLColorspaceExt());
    SourceRenderbufferTargetRenderbuffer_helper(kColorspaceAttribs);
}

void ImageTest::SourceRenderbufferTargetRenderbuffer_helper(const EGLint *attribs)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !hasRenderbufferExt());

    GLubyte data[4] = {255, 0, 255, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImageRenderbufferSource(1, 1, GL_RGBA8_OES, attribs, data, &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetRenderbuffer(image, &target);

    // Expect that the target renderbuffer has the same color as the source texture
    verifyResultsRenderbuffer(target, data);

    // Clean up
    glDeleteRenderbuffers(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteRenderbuffers(1, &target);
}

// Delete the source texture and EGL image.  The image targets should still have the same data
// because
// they hold refs to the image.
TEST_P(ImageTest, Deletion)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte originalData[4] = {255, 0, 255, 255};
    GLubyte updateData[4]   = {0, 255, 0, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create multiple targets
    GLuint targetTexture;
    createEGLImageTargetTexture2D(image, &targetTexture);

    GLuint targetRenderbuffer;
    createEGLImageTargetRenderbuffer(image, &targetRenderbuffer);

    // Delete the source texture
    glDeleteTextures(1, &source);
    source = 0;

    // Expect that both the targets have the original data
    verifyResults2D(targetTexture, originalData);
    verifyResultsRenderbuffer(targetRenderbuffer, originalData);

    // Update the data of the target
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that both targets have the updated data
    verifyResults2D(targetTexture, updateData);
    verifyResultsRenderbuffer(targetRenderbuffer, updateData);

    // Delete the EGL image
    eglDestroyImageKHR(window->getDisplay(), image);
    image = EGL_NO_IMAGE_KHR;

    // Update the data of the target back to the original data
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, originalData);

    // Expect that both targets have the original data again
    verifyResults2D(targetTexture, originalData);
    verifyResultsRenderbuffer(targetRenderbuffer, originalData);

    // Clean up
    glDeleteTextures(1, &targetTexture);
    glDeleteRenderbuffers(1, &targetRenderbuffer);
}

TEST_P(ImageTest, MipLevels)
{
    // Driver returns OOM in read pixels, some internal error.
    ANGLE_SKIP_TEST_IF(IsOzone() && IsOpenGLES());
    // Also fails on NVIDIA Shield TV bot.
    // http://anglebug.com/3850
    ANGLE_SKIP_TEST_IF(IsNVIDIAShield() && IsOpenGLES());
    // On Vulkan, the clear operation in the loop is optimized with a render pass loadOp=Clear.  On
    // Linux/Intel, that operation is mistakenly clearing the rest of the mips to 0.
    // http://anglebug.com/3284
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsLinux() && IsIntel());

    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    const size_t mipLevels   = 3;
    const size_t textureSize = 4;
    std::vector<GLColor> mip0Data(textureSize * textureSize, GLColor::red);
    std::vector<GLColor> mip1Data(mip0Data.size() << 1, GLColor::green);
    std::vector<GLColor> mip2Data(mip0Data.size() << 2, GLColor::blue);
    GLubyte *data[mipLevels] = {
        reinterpret_cast<GLubyte *>(&mip0Data[0]),
        reinterpret_cast<GLubyte *>(&mip1Data[0]),
        reinterpret_cast<GLubyte *>(&mip2Data[0]),
    };

    GLuint source;
    glGenTextures(1, &source);
    glBindTexture(GL_TEXTURE_2D, source);

    for (size_t level = 0; level < mipLevels; level++)
    {
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA, textureSize >> level,
                     textureSize >> level, 0, GL_RGBA, GL_UNSIGNED_BYTE, data[level]);
    }

    ASSERT_GL_NO_ERROR();

    for (size_t level = 0; level < mipLevels; level++)
    {
        // Create the Image
        EGLint attribs[] = {
            EGL_GL_TEXTURE_LEVEL_KHR,
            static_cast<EGLint>(level),
            EGL_NONE,
        };
        EGLImageKHR image =
            eglCreateImageKHR(window->getDisplay(), window->getContext(), EGL_GL_TEXTURE_2D_KHR,
                              reinterpretHelper<EGLClientBuffer>(source), attribs);
        ASSERT_EGL_SUCCESS();

        // Create a texture and renderbuffer target
        GLuint textureTarget;
        createEGLImageTargetTexture2D(image, &textureTarget);

        // Disable mipmapping
        glBindTexture(GL_TEXTURE_2D, textureTarget);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        GLuint renderbufferTarget;
        createEGLImageTargetRenderbuffer(image, &renderbufferTarget);

        // Expect that the targets have the same color as the source texture
        verifyResults2D(textureTarget, data[level]);
        verifyResultsRenderbuffer(renderbufferTarget, data[level]);

        // Update the data by uploading data to the texture
        std::vector<GLuint> textureUpdateData(textureSize * textureSize, level);
        glBindTexture(GL_TEXTURE_2D, textureTarget);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureSize >> level, textureSize >> level, GL_RGBA,
                        GL_UNSIGNED_BYTE, textureUpdateData.data());
        ASSERT_GL_NO_ERROR();

        // Expect that both the texture and renderbuffer see the updated texture data
        verifyResults2D(textureTarget, reinterpret_cast<GLubyte *>(textureUpdateData.data()));
        verifyResultsRenderbuffer(renderbufferTarget,
                                  reinterpret_cast<GLubyte *>(textureUpdateData.data()));

        // Update the renderbuffer by clearing it
        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderbufferTarget);

        GLubyte clearValue = static_cast<GLubyte>(level);
        GLubyte renderbufferClearData[4]{clearValue, clearValue, clearValue, clearValue};
        glClearColor(renderbufferClearData[0] / 255.0f, renderbufferClearData[1] / 255.0f,
                     renderbufferClearData[2] / 255.0f, renderbufferClearData[3] / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ASSERT_GL_NO_ERROR();

        // Expect that both the texture and renderbuffer see the cleared renderbuffer data
        verifyResults2D(textureTarget, renderbufferClearData);
        verifyResultsRenderbuffer(renderbufferTarget, renderbufferClearData);

        // Clean up
        eglDestroyImageKHR(window->getDisplay(), image);
        glDeleteTextures(1, &textureTarget);
        glDeleteRenderbuffers(1, &renderbufferTarget);
    }

    // Clean up
    glDeleteTextures(1, &source);
}

// Respecify the source texture, orphaning it.  The target texture should not have updated data.
TEST_P(ImageTest, Respecification)
{
    // Respecification of textures that does not change the size of the level attached to the EGL
    // image does not cause orphaning on Qualcomm devices. http://anglebug.com/2744
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());
    ANGLE_SKIP_TEST_IF(IsOzone() && IsOpenGLES());

    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte originalData[4] = {255, 0, 255, 255};
    GLubyte updateData[4]   = {0, 255, 0, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Respecify source
    glBindTexture(GL_TEXTURE_2D, source);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that the target texture has the original data
    verifyResults2D(target, originalData);

    // Expect that the source texture has the updated data
    verifyResults2D(source, updateData);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Respecify the source texture with a different size, orphaning it.  The target texture should not
// have updated data.
TEST_P(ImageTest, RespecificationDifferentSize)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte originalData[4] = {255, 0, 255, 255};
    GLubyte updateData[16]  = {0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Respecify source
    glBindTexture(GL_TEXTURE_2D, source);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that the target texture has the original data
    verifyResults2D(target, originalData);

    // Expect that the source texture has the updated data
    verifyResults2D(source, updateData);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// First render to a target texture, then respecify the source texture, orphaning it.
// The target texture's FBO should be notified of the target texture's orphaning.
TEST_P(ImageTest, RespecificationWithFBO)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLuint program = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    ASSERT_NE(0u, program);

    GLubyte originalData[4] = {255, 0, 255, 255};
    GLubyte updateData[4]   = {0, 255, 0, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Render to the target texture
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Respecify source with same parameters. This should not change the texture storage in D3D11.
    glBindTexture(GL_TEXTURE_2D, source);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that the source texture has the updated data
    verifyResults2D(source, updateData);

    // Render to the target texture again and verify it gets the rendered pixels.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
    glDeleteProgram(program);
    glDeleteFramebuffers(1, &fbo);
}

// Test that respecifying a level of the target texture orphans it and keeps a copy of the EGLimage
// data
TEST_P(ImageTest, RespecificationOfOtherLevel)
{
    // Respecification of textures that does not change the size of the level attached to the EGL
    // image does not cause orphaning on Qualcomm devices. http://anglebug.com/2744
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    // It is undefined what happens to the mip 0 of the dest texture after it is orphaned. Some
    // backends explicitly copy the data but Vulkan does not.
    ANGLE_SKIP_TEST_IF(IsVulkan());

    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte originalData[2 * 2 * 4] = {
        255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255,
    };

    GLubyte updateData[2 * 2 * 4] = {
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
    };

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create the target
    GLuint target;
    createEGLImageTargetTexture2D(image, &target);

    // Expect that the target and source textures have the original data
    verifyResults2D(source, originalData);
    verifyResults2D(target, originalData);

    // Add a new mipLevel to the target, orphaning it
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, originalData);
    EXPECT_GL_NO_ERROR();

    // Expect that the target and source textures still have the original data
    verifyResults2D(source, originalData);
    verifyResults2D(target, originalData);

    // Update the source's data
    glBindTexture(GL_TEXTURE_2D, source);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that the target still has the original data and source has the updated data
    verifyResults2D(source, updateData);
    verifyResults2D(target, originalData);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &target);
}

// Update the data of the source and target textures.  All image siblings should have the new data.
TEST_P(ImageTest, UpdatedData)
{
    EGLWindow *window = getEGLWindow();
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());

    GLubyte originalData[4] = {255, 0, 255, 255};
    GLubyte updateData[4]   = {0, 255, 0, 255};

    // Create the Image
    GLuint source;
    EGLImageKHR image;
    createEGLImage2DTextureSource(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, kDefaultAttribs, originalData,
                                  &source, &image);

    // Create multiple targets
    GLuint targetTexture;
    createEGLImageTargetTexture2D(image, &targetTexture);

    GLuint targetRenderbuffer;
    createEGLImageTargetRenderbuffer(image, &targetRenderbuffer);

    // Expect that both the source and targets have the original data
    verifyResults2D(source, originalData);
    verifyResults2D(targetTexture, originalData);
    verifyResultsRenderbuffer(targetRenderbuffer, originalData);

    // Update the data of the source
    glBindTexture(GL_TEXTURE_2D, source);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Expect that both the source and targets have the updated data
    verifyResults2D(source, updateData);
    verifyResults2D(targetTexture, updateData);
    verifyResultsRenderbuffer(targetRenderbuffer, updateData);

    // Update the data of the target back to the original data
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, originalData);

    // Expect that both the source and targets have the original data again
    verifyResults2D(source, originalData);
    verifyResults2D(targetTexture, originalData);
    verifyResultsRenderbuffer(targetRenderbuffer, originalData);

    // Clean up
    glDeleteTextures(1, &source);
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &targetTexture);
    glDeleteRenderbuffers(1, &targetRenderbuffer);
}

// Check that the external texture is successfully updated when only glTexSubImage2D is called.
TEST_P(ImageTest, UpdatedExternalTexture)
{
    EGLWindow *window = getEGLWindow();

    ANGLE_SKIP_TEST_IF(!IsAndroid());
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt() || !has2DTextureExt());
    ANGLE_SKIP_TEST_IF(!hasAndroidImageNativeBufferExt() || !hasAndroidHardwareBufferSupport());

    GLubyte originalData[4]      = {255, 0, 255, 255};
    GLubyte updateData[4]        = {0, 255, 0, 255};
    const uint32_t bytesPerPixel = 4;

    // Create the Image
    AHardwareBuffer *source;
    EGLImageKHR image;
    createEGLImageAndroidHardwareBufferSource(1, 1, 1, GL_RGBA8, kDefaultAttribs, originalData,
                                              bytesPerPixel, &source, &image);

    // Create target
    GLuint targetTexture;
    createEGLImageTargetTexture2D(image, &targetTexture);

    // Expect that both the target have the original data
    verifyResults2D(targetTexture, originalData);

    // Update the data of the source
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, updateData);

    // Set sync object and flush the GL commands
    EGLSyncKHR fence = eglCreateSyncKHR(window->getDisplay(), EGL_SYNC_FENCE_KHR, NULL);
    ASSERT_NE(fence, EGL_NO_SYNC_KHR);
    glFlush();

    // Delete the target texture
    glDeleteTextures(1, &targetTexture);

    // Wait that the flush command is finished
    EGLint result = eglClientWaitSyncKHR(window->getDisplay(), fence, 0, 1000000000);
    ASSERT_EQ(result, EGL_CONDITION_SATISFIED_KHR);
    ASSERT_EGL_TRUE(eglDestroySyncKHR(window->getDisplay(), fence));

    // Delete the EGL image
    eglDestroyImageKHR(window->getDisplay(), image);

    // Access the android hardware buffer directly to check the data is updated
    verifyResultAHB(source, updateData, 4, bytesPerPixel);

    // Create the EGL image again
    image =
        eglCreateImageKHR(window->getDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                          angle::android::AHardwareBufferToClientBuffer(source), kDefaultAttribs);
    ASSERT_EGL_SUCCESS();

    // Create the target texture again
    createEGLImageTargetTexture2D(image, &targetTexture);

    // Expect that the target have the update data
    verifyResults2D(targetTexture, updateData);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    destroyAndroidHardwareBuffer(source);
    glDeleteTextures(1, &targetTexture);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ImageTest);
ANGLE_INSTANTIATE_TEST_ES3(ImageTestES3);
}  // namespace angle
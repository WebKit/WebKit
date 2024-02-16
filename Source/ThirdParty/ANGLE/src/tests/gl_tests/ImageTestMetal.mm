//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageTestMetal:
//   Tests the correctness of eglImage with native Metal texture extensions.
//

#include "test_utils/ANGLETest.h"

#include "common/mathutil.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Metal/Metal.h>
#include <gmock/gmock.h>
#include <span>

namespace angle
{
namespace
{
constexpr char kOESExt[]                      = "GL_OES_EGL_image";
constexpr char kBaseExt[]                     = "EGL_KHR_image_base";
constexpr char kDeviceMtlExt[]                = "EGL_ANGLE_device_metal";
constexpr char kEGLMtlImageNativeTextureExt[] = "EGL_ANGLE_metal_texture_client_buffer";
constexpr EGLint kDefaultAttribs[]            = {
    EGL_NONE,
};

template <typename T>
class ScopedMetalObjectRef : angle::NonCopyable
{
  public:
    ScopedMetalObjectRef() = default;

    explicit ScopedMetalObjectRef(T &&surface) : mObject(surface) {}

    ~ScopedMetalObjectRef()
    {
        if (mObject)
        {
            release();
            mObject = nil;
        }
    }

    T get() const { return mObject; }

    // auto cast to T
    operator T() const { return mObject; }
    ScopedMetalObjectRef(const ScopedMetalObjectRef &other)
    {
        if (mObject)
        {
            release();
        }
        mObject = other.mObject;
    }

    explicit ScopedMetalObjectRef(ScopedMetalObjectRef &&other)
    {
        if (mObject)
        {
            release();
        }
        mObject       = other.mObject;
        other.mObject = nil;
    }

    ScopedMetalObjectRef &operator=(ScopedMetalObjectRef &&other)
    {
        if (mObject)
        {
            release();
        }
        mObject       = other.mObject;
        other.mObject = nil;

        return *this;
    }

    ScopedMetalObjectRef &operator=(const ScopedMetalObjectRef &other)
    {
        if (mObject)
        {
            release();
        }
        mObject = other.mObject;

        return *this;
    }

  private:
    void release()
    {
#if !__has_feature(objc_arc)
        [mObject release];
#endif
    }

    T mObject = nil;
};

using ScopedMetalTextureRef      = ScopedMetalObjectRef<id<MTLTexture>>;
using ScopedMetalBufferRef       = ScopedMetalObjectRef<id<MTLBuffer>>;
using ScopedMetalCommandQueueRef = ScopedMetalObjectRef<id<MTLCommandQueue>>;

}  // anonymous namespace

ScopedMetalTextureRef CreateMetalTexture2D(id<MTLDevice> deviceMtl,
                                           int width,
                                           int height,
                                           MTLPixelFormat format,
                                           int arrayLength)
{
    @autoreleasepool
    {
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                                        width:width
                                                                                       height:width
                                                                                    mipmapped:NO];
        desc.usage                 = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        if (arrayLength)
        {
            desc.arrayLength = arrayLength;
            desc.textureType = MTLTextureType2DArray;
        }
        ScopedMetalTextureRef re([deviceMtl newTextureWithDescriptor:desc]);
        return re;
    }
}

class ImageTestMetal : public ANGLETest<>
{
  protected:
    ImageTestMetal()
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
        constexpr char kVS[] = "precision highp float;\n"
                               "attribute vec4 position;\n"
                               "varying vec2 texcoord;\n"
                               "\n"
                               "void main()\n"
                               "{\n"
                               "    gl_Position = position;\n"
                               "    texcoord = (position.xy * 0.5) + 0.5;\n"
                               "    texcoord.y = 1.0 - texcoord.y;\n"
                               "}\n";

        constexpr char kTextureFS[] = "precision highp float;\n"
                                      "uniform sampler2D tex;\n"
                                      "varying vec2 texcoord;\n"
                                      "\n"
                                      "void main()\n"
                                      "{\n"
                                      "    gl_FragColor = texture2D(tex, texcoord);\n"
                                      "}\n";

        mTextureProgram = CompileProgram(kVS, kTextureFS);
        if (mTextureProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mTextureProgram); }

    id<MTLDevice> getMtlDevice()
    {
        EGLAttrib angleDevice = 0;
        EGLAttrib device      = 0;
        EXPECT_EGL_TRUE(
            eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));

        EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                                EGL_METAL_DEVICE_ANGLE, &device));

        return (__bridge id<MTLDevice>)reinterpret_cast<void *>(device);
    }

    ScopedMetalTextureRef createMtlTexture2D(int width, int height, MTLPixelFormat format)
    {
        id<MTLDevice> device = getMtlDevice();

        return CreateMetalTexture2D(device, width, height, format, 0);
    }

    ScopedMetalTextureRef createMtlTexture2DArray(int width,
                                                  int height,
                                                  int arrayLength,
                                                  MTLPixelFormat format)
    {
        id<MTLDevice> device = getMtlDevice();

        return CreateMetalTexture2D(device, width, height, format, arrayLength);
    }
    void getTextureSliceBytes(id<MTLTexture> texture,
                              unsigned bytesPerRow,
                              MTLRegion region,
                              unsigned mipmapLevel,
                              unsigned slice,
                              std::span<uint8_t> sliceImage)
    {
        @autoreleasepool
        {
            id<MTLDevice> device = texture.device;
            ScopedMetalBufferRef readBuffer([device
                newBufferWithLength:sliceImage.size()
                            options:MTLResourceStorageModeShared]);
            ScopedMetalCommandQueueRef commandQueue([device newCommandQueue]);
            id<MTLCommandBuffer> commandBuffer    = [commandQueue commandBuffer];
            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
            [blitEncoder copyFromTexture:texture
                             sourceSlice:slice
                             sourceLevel:mipmapLevel
                            sourceOrigin:region.origin
                              sourceSize:region.size
                                toBuffer:readBuffer
                       destinationOffset:0
                  destinationBytesPerRow:bytesPerRow
                destinationBytesPerImage:sliceImage.size()];
            [blitEncoder endEncoding];
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            memcpy(sliceImage.data(), readBuffer.get().contents, sliceImage.size());
        }
    }
    void sourceMetalTarget2D_helper(GLubyte data[4],
                                    const EGLint *attribs,
                                    EGLImageKHR *imageOut,
                                    GLuint *textureOut);

    void verifyResultsTexture(GLuint texture,
                              const GLubyte data[4],
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
        EXPECT_PIXEL_NEAR(0, 0, data[0], data[1], data[2], data[3], 1.0);
    }

    void verifyResults2D(GLuint texture, const GLubyte data[4])
    {
        verifyResultsTexture(texture, data, GL_TEXTURE_2D, mTextureProgram,
                             mTextureUniformLocation);
    }

    template <typename destType, typename sourcetype>
    destType reinterpretHelper(sourcetype source)
    {
        static_assert(sizeof(destType) == sizeof(size_t),
                      "destType should be the same size as a size_t");
        size_t sourceSizeT = static_cast<size_t>(source);
        return reinterpret_cast<destType>(sourceSizeT);
    }

    void drawColorQuad(GLColor color)
    {
        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);
        GLint colorUniformLocation =
            glGetUniformLocation(program, angle::essl1_shaders::ColorUniform());
        ASSERT_NE(colorUniformLocation, -1);
        glUniform4fv(colorUniformLocation, 1, color.toNormalizedVector().data());
        drawQuad(program, essl1_shaders::PositionAttrib(), 0);
        glUseProgram(0);
    }

    bool hasImageNativeMetalTextureExt() const
    {
        if (!IsMetal())
        {
            return false;
        }
        EGLAttrib angleDevice = 0;
        eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice);
        if (!angleDevice)
        {
            return false;
        }
        auto extensionString = static_cast<const char *>(
            eglQueryDeviceStringEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice), EGL_EXTENSIONS));
        if (strstr(extensionString, kDeviceMtlExt) == nullptr)
        {
            return false;
        }
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(),
                                            kEGLMtlImageNativeTextureExt);
    }

    bool hasOESExt() const { return IsGLExtensionEnabled(kOESExt); }

    bool hasBaseExt() const
    {
        return IsEGLDisplayExtensionEnabled(getEGLWindow()->getDisplay(), kBaseExt);
    }

    GLuint mTextureProgram;
    GLint mTextureUniformLocation;
};

void ImageTestMetal::sourceMetalTarget2D_helper(GLubyte data[4],
                                                const EGLint *attribs,
                                                EGLImageKHR *imageOut,
                                                GLuint *textureOut)
{
    EGLWindow *window = getEGLWindow();

    // Create MTLTexture
    ScopedMetalTextureRef textureMtl = createMtlTexture2D(1, 1, MTLPixelFormatRGBA8Unorm);

    // Create image
    EGLImageKHR image =
        eglCreateImageKHR(window->getDisplay(), EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs);
    ASSERT_EGL_SUCCESS();

    // Write the data to the MTLTexture
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:0
                          withBytes:data
                        bytesPerRow:4
                      bytesPerImage:0];

    // Create a texture target to bind the egl image
    GLuint target;
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    *imageOut   = image;
    *textureOut = target;
}

// Testing source metal EGL image, target 2D texture
TEST_P(ImageTestMetal, SourceMetalTarget2D)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());

    EGLWindow *window = getEGLWindow();

    // Create the Image
    EGLImageKHR image;
    GLuint texTarget;
    GLubyte data[4] = {7, 51, 197, 231};
    sourceMetalTarget2D_helper(data, kDefaultAttribs, &image, &texTarget);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResults2D(texTarget, data);

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &texTarget);
}

// Create source metal EGL image, target 2D texture, then trigger texture respecification.
TEST_P(ImageTestMetal, SourceMetal2DTargetTextureRespecifySize)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());

    EGLWindow *window = getEGLWindow();

    // Create the Image
    EGLImageKHR image;
    GLuint texTarget;
    GLubyte data[4] = {7, 51, 197, 231};
    sourceMetalTarget2D_helper(data, kDefaultAttribs, &image, &texTarget);

    // Use texture target bound to egl image as source and render to framebuffer
    // Verify that data in framebuffer matches that in the egl image
    verifyResults2D(texTarget, data);

    // Respecify texture size and verify results
    std::array<GLubyte, 16> referenceColor;
    referenceColor.fill(127);
    glBindTexture(GL_TEXTURE_2D, texTarget);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 referenceColor.data());
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 referenceColor.data());
    ASSERT_GL_NO_ERROR();

    // Expect that the target texture has the reference color values
    verifyResults2D(texTarget, referenceColor.data());

    // Clean up
    eglDestroyImageKHR(window->getDisplay(), image);
    glDeleteTextures(1, &texTarget);
}

// Tests that OpenGL can sample from a texture bound with Metal texture slice.
TEST_P(ImageTestMetal, SourceMetalTarget2DArray)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());
    ScopedMetalTextureRef textureMtl = createMtlTexture2DArray(1, 1, 3, MTLPixelFormatRGBA8Unorm);

    GLubyte data0[4] = {93, 83, 75, 128};
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:0
                          withBytes:data0
                        bytesPerRow:4
                      bytesPerImage:4];
    GLubyte data1[4] = {7, 51, 197, 231};
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:1
                          withBytes:data1
                        bytesPerRow:4
                      bytesPerImage:4];
    GLubyte data2[4] = {33, 51, 44, 33};
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:2
                          withBytes:data2
                        bytesPerRow:4
                      bytesPerImage:4];

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLImageKHR image0 =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), nullptr);
    ASSERT_EGL_SUCCESS();
    const EGLint attribs1[] = {EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, 1, EGL_NONE};
    EGLImageKHR image1 =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs1);
    ASSERT_EGL_SUCCESS();
    const EGLint attribs2[] = {EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, 2, EGL_NONE};
    EGLImageKHR image2 =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs2);
    ASSERT_EGL_SUCCESS();

    GLTexture targetTexture;
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image0);
    verifyResults2D(targetTexture, data0);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image1);
    verifyResults2D(targetTexture, data1);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image2);
    verifyResults2D(targetTexture, data2);
    eglDestroyImageKHR(display, image0);
    eglDestroyImageKHR(display, image1);
    eglDestroyImageKHR(display, image2);
    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_SUCCESS();
}

// Test that bound slice to EGLImage is not affected by releasing the source texture.
TEST_P(ImageTestMetal, SourceMetalTarget2DArrayReleasedSourceOk)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());
    ScopedMetalTextureRef textureMtl = createMtlTexture2DArray(1, 1, 3, MTLPixelFormatRGBA8Unorm);

    GLubyte data1[4] = {7, 51, 197, 231};
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:1
                          withBytes:data1
                        bytesPerRow:4
                      bytesPerImage:4];

    EGLDisplay display      = getEGLWindow()->getDisplay();
    const EGLint attribs1[] = {EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, 1, EGL_NONE};
    EGLImageKHR image1 =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs1);
    ASSERT_EGL_SUCCESS();
    // This is being tested: release the source texture but the slice keeps working.
    textureMtl = {};
    GLTexture targetTexture;
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image1);
    verifyResults2D(targetTexture, data1);
    eglDestroyImageKHR(display, image1);
    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_SUCCESS();
}

// Tests that OpenGL can draw to a texture bound with Metal texture.
TEST_P(ImageTestMetal, DrawMetalTarget2D)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());

    EGLDisplay display = getEGLWindow()->getDisplay();

    ScopedMetalTextureRef textureMtl = createMtlTexture2D(1, 1, MTLPixelFormatRGBA8Unorm);
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:0
                          withBytes:GLColor::red.data()
                        bytesPerRow:4
                      bytesPerImage:4];

    EGLImageKHR image =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), nullptr);
    ASSERT_EGL_SUCCESS();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    GLFramebuffer targetFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, 1, 1);
    drawColorQuad(GLColor::magenta);
    EXPECT_GL_NO_ERROR();
    eglDestroyImageKHR(display, image);
    eglWaitUntilWorkScheduledANGLE(display);
    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_SUCCESS();

    GLColor result;
    getTextureSliceBytes(textureMtl.get(), 4, MTLRegionMake2D(0, 0, 1, 1), 0, 0,
                         {result.data(), 4});
    EXPECT_EQ(result, GLColor::magenta);
}

// Tests that OpenGL can draw to a texture bound with Metal texture slice.
TEST_P(ImageTestMetal, DrawMetalTarget2DArray)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());

    EGLDisplay display = getEGLWindow()->getDisplay();

    ScopedMetalTextureRef textureMtl = createMtlTexture2DArray(1, 1, 2, MTLPixelFormatRGBA8Unorm);
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:0
                          withBytes:GLColor::red.data()
                        bytesPerRow:4
                      bytesPerImage:4];
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:1
                          withBytes:GLColor::red.data()
                        bytesPerRow:4
                      bytesPerImage:4];

    const EGLint attribs[] = {EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, 1, EGL_NONE};
    EGLImageKHR image =
        eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                          reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs);
    ASSERT_EGL_SUCCESS();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    EXPECT_GL_NO_ERROR();
    GLFramebuffer targetFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, 1, 1);
    drawColorQuad(GLColor::magenta);
    EXPECT_GL_NO_ERROR();
    eglDestroyImageKHR(display, image);
    eglWaitUntilWorkScheduledANGLE(display);
    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_SUCCESS();

    GLColor result;
    getTextureSliceBytes(textureMtl.get(), 4, MTLRegionMake2D(0, 0, 1, 1), 0, 1,
                         {result.data(), 4});
    EXPECT_EQ(result, GLColor::magenta);
}

// Tests that OpenGL can blit to a texture bound with Metal texture slice.
TEST_P(ImageTestMetal, BlitMetalTarget2DArray)
{
    ANGLE_SKIP_TEST_IF(!hasOESExt() || !hasBaseExt());
    ANGLE_SKIP_TEST_IF(!hasImageNativeMetalTextureExt());

    EGLDisplay display = getEGLWindow()->getDisplay();

    GLTexture colorBuffer;
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, GLColor::green.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    GLColor::yellow.data());
    EXPECT_GL_NO_ERROR();

    GLFramebuffer sourceFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    ScopedMetalTextureRef textureMtl = createMtlTexture2DArray(1, 1, 2, MTLPixelFormatRGBA8Unorm);
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:0
                          withBytes:GLColor::red.data()
                        bytesPerRow:4
                      bytesPerImage:4];
    [textureMtl.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                        mipmapLevel:0
                              slice:1
                          withBytes:GLColor::red.data()
                        bytesPerRow:4
                      bytesPerImage:4];

    for (int slice = 0; slice < 2; ++slice)
    {
        const EGLint attribs[] = {EGL_METAL_TEXTURE_ARRAY_SLICE_ANGLE, slice, EGL_NONE};
        EGLImageKHR image =
            eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_METAL_TEXTURE_ANGLE,
                              reinterpret_cast<EGLClientBuffer>(textureMtl.get()), attribs);
        ASSERT_EGL_SUCCESS();

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        verifyResults2D(texture, GLColor::red.data());
        EXPECT_GL_NO_ERROR();

        GLFramebuffer targetFbo;
        glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, sourceFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, targetFbo);
        glBlitFramebufferANGLE(slice, 0, slice + 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        verifyResults2D(texture, slice == 0 ? GLColor::green.data() : GLColor::yellow.data());
        eglDestroyImageKHR(display, image);
        EXPECT_GL_NO_ERROR();
        EXPECT_EGL_SUCCESS();
    }
    eglWaitUntilWorkScheduledANGLE(display);
    EXPECT_EGL_SUCCESS();

    GLColor result;
    getTextureSliceBytes(textureMtl.get(), 4, MTLRegionMake2D(0, 0, 1, 1), 0, 0,
                         {result.data(), 4});
    EXPECT_EQ(result, GLColor::green);
    getTextureSliceBytes(textureMtl.get(), 4, MTLRegionMake2D(0, 0, 1, 1), 0, 1,
                         {result.data(), 4});
    EXPECT_EQ(result, GLColor::yellow);
}
// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(ImageTestMetal, ES2_METAL(), ES3_METAL());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ImageTestMetal);
}  // namespace angle

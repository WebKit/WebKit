//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLBlobCacheTest:
//   Unit tests for the EGL_ANDROID_blob_cache extension.

#include <map>
#include <vector>

#include "common/angleutils.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

constexpr char kEGLExtName[] = "EGL_ANDROID_blob_cache";

enum class CacheOpResult
{
    SET_SUCCESS,
    GET_NOT_FOUND,
    GET_MEMORY_TOO_SMALL,
    GET_SUCCESS,

    VALUE_NOT_SET,
};

namespace
{
std::map<std::vector<uint8_t>, std::vector<uint8_t>> gApplicationCache;
CacheOpResult gLastCacheOpResult = CacheOpResult::VALUE_NOT_SET;

void SetBlob(const void *key, EGLsizeiANDROID keySize, const void *value, EGLsizeiANDROID valueSize)
{
    std::vector<uint8_t> keyVec(keySize);
    memcpy(keyVec.data(), key, keySize);

    std::vector<uint8_t> valueVec(valueSize);
    memcpy(valueVec.data(), value, valueSize);

    gApplicationCache[keyVec] = valueVec;

    gLastCacheOpResult = CacheOpResult::SET_SUCCESS;
}

EGLsizeiANDROID GetBlob(const void *key,
                        EGLsizeiANDROID keySize,
                        void *value,
                        EGLsizeiANDROID valueSize)
{
    std::vector<uint8_t> keyVec(keySize);
    memcpy(keyVec.data(), key, keySize);

    auto entry = gApplicationCache.find(keyVec);
    if (entry == gApplicationCache.end())
    {
        gLastCacheOpResult = CacheOpResult::GET_NOT_FOUND;
        return 0;
    }

    if (entry->second.size() <= static_cast<size_t>(valueSize))
    {
        memcpy(value, entry->second.data(), entry->second.size());
        gLastCacheOpResult = CacheOpResult::GET_SUCCESS;
    }
    else
    {
        gLastCacheOpResult = CacheOpResult::GET_MEMORY_TOO_SMALL;
    }

    return entry->second.size();
}
}  // anonymous namespace

class EGLBlobCacheTest : public ANGLETest
{
  protected:
    EGLBlobCacheTest() : mHasBlobCache(false)
    {
        // Force disply caching off. Blob cache functions require it.
        forceNewDisplay();
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        EGLDisplay display = getEGLWindow()->getDisplay();
        mHasBlobCache      = IsEGLDisplayExtensionEnabled(display, kEGLExtName);
    }

    bool programBinaryAvailable()
    {
        return (getClientMajorVersion() >= 3 || IsGLExtensionEnabled("GL_OES_get_program_binary"));
    }

    bool mHasBlobCache;
};

// Makes sure the extension exists and works
TEST_P(EGLBlobCacheTest, Functional)
{
    EGLDisplay display = getEGLWindow()->getDisplay();

    EXPECT_TRUE(mHasBlobCache);
    eglSetBlobCacheFuncsANDROID(display, SetBlob, GetBlob);
    ASSERT_EGL_SUCCESS();

    constexpr char kVertexShaderSrc[] = R"(attribute vec4 aTest;
attribute vec2 aPosition;
varying vec4 vTest;
void main()
{
    vTest        = aTest;
    gl_Position  = vec4(aPosition, 0.0, 1.0);
    gl_PointSize = 1.0;
})";

    constexpr char kFragmentShaderSrc[] = R"(precision mediump float;
varying vec4 vTest;
void main()
{
    gl_FragColor = vTest;
})";

    constexpr char kVertexShaderSrc2[] = R"(attribute vec4 aTest;
attribute vec2 aPosition;
varying vec4 vTest;
void main()
{
    vTest        = aTest;
    gl_Position  = vec4(aPosition, 1.0, 1.0);
    gl_PointSize = 1.0;
})";

    constexpr char kFragmentShaderSrc2[] = R"(precision mediump float;
varying vec4 vTest;
void main()
{
    gl_FragColor = vTest - vec4(0.0, 1.0, 0.0, 0.0);
})";

    // Compile a shader so it puts something in the cache
    if (programBinaryAvailable())
    {
        GLuint program = CompileProgram(kVertexShaderSrc, kFragmentShaderSrc);
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::SET_SUCCESS, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::VALUE_NOT_SET;

        // Compile the same shader again, so it would try to retrieve it from the cache
        program = CompileProgram(kVertexShaderSrc, kFragmentShaderSrc);
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::GET_SUCCESS, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::VALUE_NOT_SET;

        // Compile another shader, which should create a new entry
        program = CompileProgram(kVertexShaderSrc2, kFragmentShaderSrc2);
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::SET_SUCCESS, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::VALUE_NOT_SET;

        // Compile the first shader again, which should still reside in the cache
        program = CompileProgram(kVertexShaderSrc, kFragmentShaderSrc);
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::GET_SUCCESS, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::VALUE_NOT_SET;
    }
}

// Tests error conditions of the APIs.
TEST_P(EGLBlobCacheTest, NegativeAPI)
{
    EXPECT_TRUE(mHasBlobCache);

    // Test bad display
    eglSetBlobCacheFuncsANDROID(EGL_NO_DISPLAY, nullptr, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    eglSetBlobCacheFuncsANDROID(EGL_NO_DISPLAY, SetBlob, GetBlob);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    EGLDisplay display = getEGLWindow()->getDisplay();

    // Test bad arguments
    eglSetBlobCacheFuncsANDROID(display, nullptr, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglSetBlobCacheFuncsANDROID(display, SetBlob, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglSetBlobCacheFuncsANDROID(display, nullptr, GetBlob);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Set the arguments once and test setting them again (which should fail)
    eglSetBlobCacheFuncsANDROID(display, SetBlob, GetBlob);
    ASSERT_EGL_SUCCESS();

    eglSetBlobCacheFuncsANDROID(display, SetBlob, GetBlob);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Try again with bad parameters
    eglSetBlobCacheFuncsANDROID(EGL_NO_DISPLAY, nullptr, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    eglSetBlobCacheFuncsANDROID(display, nullptr, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglSetBlobCacheFuncsANDROID(display, SetBlob, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglSetBlobCacheFuncsANDROID(display, nullptr, GetBlob);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
}

ANGLE_INSTANTIATE_TEST(EGLBlobCacheTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL(), ES2_VULKAN());

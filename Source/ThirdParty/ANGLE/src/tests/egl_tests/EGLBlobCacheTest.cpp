//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLBlobCacheTest:
//   Unit tests for the EGL_ANDROID_blob_cache extension.

// Must be included first to prevent errors with "None".
#include "test_utils/ANGLETest.h"

#include <map>
#include <vector>

#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

constexpr char kEGLExtName[] = "EGL_ANDROID_blob_cache";

enum class CacheOpResult
{
    SetSuccess,
    GetNotFound,
    GetMemoryTooSmall,
    GetSuccess,
    ValueNotSet,
    EnumCount
};

angle::PackedEnumMap<CacheOpResult, std::string> kCacheOpToString = {
    {CacheOpResult::SetSuccess, "SetSuccess"},
    {CacheOpResult::GetNotFound, "GetNotFound"},
    {CacheOpResult::GetMemoryTooSmall, "GetMemoryTooSmall"},
    {CacheOpResult::GetSuccess, "GetSuccess"},
    {CacheOpResult::ValueNotSet, "ValueNotSet"},
};

std::ostream &operator<<(std::ostream &os, CacheOpResult result)
{
    return os << kCacheOpToString[result];
}

namespace
{
std::map<std::vector<uint8_t>, std::vector<uint8_t>> gApplicationCache;
CacheOpResult gLastCacheOpResult = CacheOpResult::ValueNotSet;

void SetBlob(const void *key, EGLsizeiANDROID keySize, const void *value, EGLsizeiANDROID valueSize)
{
    std::vector<uint8_t> keyVec(keySize);
    memcpy(keyVec.data(), key, keySize);

    std::vector<uint8_t> valueVec(valueSize);
    memcpy(valueVec.data(), value, valueSize);

    gApplicationCache[keyVec] = valueVec;

    gLastCacheOpResult = CacheOpResult::SetSuccess;
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
        gLastCacheOpResult = CacheOpResult::GetNotFound;
        return 0;
    }

    if (entry->second.size() <= static_cast<size_t>(valueSize))
    {
        memcpy(value, entry->second.data(), entry->second.size());
        gLastCacheOpResult = CacheOpResult::GetSuccess;
    }
    else
    {
        gLastCacheOpResult = CacheOpResult::GetMemoryTooSmall;
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

    void testSetUp() override
    {
        EGLDisplay display = getEGLWindow()->getDisplay();
        mHasBlobCache      = IsEGLDisplayExtensionEnabled(display, kEGLExtName);
    }

    void testTearDown() override { gApplicationCache.clear(); }

    bool programBinaryAvailable() { return IsGLExtensionEnabled("GL_OES_get_program_binary"); }

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
        ANGLE_GL_PROGRAM(program, kVertexShaderSrc, kFragmentShaderSrc);
        EXPECT_EQ(CacheOpResult::SetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;

        // Compile the same shader again, so it would try to retrieve it from the cache
        program.makeRaster(kVertexShaderSrc, kFragmentShaderSrc);
        ASSERT_TRUE(program.valid());
        EXPECT_EQ(CacheOpResult::GetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;

        // Compile another shader, which should create a new entry
        program.makeRaster(kVertexShaderSrc2, kFragmentShaderSrc2);
        ASSERT_TRUE(program.valid());
        EXPECT_EQ(CacheOpResult::SetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;

        // Compile the first shader again, which should still reside in the cache
        program.makeRaster(kVertexShaderSrc, kFragmentShaderSrc);
        ASSERT_TRUE(program.valid());
        EXPECT_EQ(CacheOpResult::GetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;
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

// Regression test for including the fragment output locatins in the program key.
// http://anglebug.com/4535
TEST_P(EGLBlobCacheTest, FragmentOutputLocationKey)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_blend_func_extended") ||
                       getClientMajorVersion() < 3);

    EGLDisplay display = getEGLWindow()->getDisplay();

    EXPECT_TRUE(mHasBlobCache);
    eglSetBlobCacheFuncsANDROID(display, SetBlob, GetBlob);
    ASSERT_EGL_SUCCESS();

    // Compile a shader so it puts something in the cache
    if (programBinaryAvailable())
    {
        constexpr char kFragmentShaderSrc[] = R"(#version 300 es
#extension GL_EXT_blend_func_extended : require
precision mediump float;
uniform vec4 src;
uniform vec4 src1;
out vec4 FragData;
out vec4 SecondaryFragData;
void main() {
    FragData = src;
    SecondaryFragData = src1;
})";

        constexpr char kVertexShaderSrc[] = R"(#version 300 es
in vec4 position;
void main() {
    gl_Position = position;
})";

        GLuint program = CompileProgram(kVertexShaderSrc, kFragmentShaderSrc, [](GLuint p) {
            glBindFragDataLocationEXT(p, 0, "FragData[0]");
            glBindFragDataLocationIndexedEXT(p, 0, 1, "SecondaryFragData[0]");
        });
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::SetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;

        // Re-link the program with different fragment output bindings
        program = CompileProgram(kVertexShaderSrc, kFragmentShaderSrc, [](GLuint p) {
            glBindFragDataLocationEXT(p, 0, "FragData");
            glBindFragDataLocationIndexedEXT(p, 0, 1, "SecondaryFragData");
        });
        ASSERT_NE(0u, program);
        EXPECT_EQ(CacheOpResult::SetSuccess, gLastCacheOpResult);
        gLastCacheOpResult = CacheOpResult::ValueNotSet;
    }
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(EGLBlobCacheTest);

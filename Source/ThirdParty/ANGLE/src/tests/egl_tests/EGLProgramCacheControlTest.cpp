//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLProgramCacheControlTest:
//   Unit tests for the EGL_ANGLE_program_cache_control extension.

#include "common/angleutils.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

constexpr EGLint kEnabledCacheSize = 0x10000;
constexpr char kEGLExtName[]       = "EGL_ANGLE_program_cache_control ";

void TestCacheProgram(PlatformMethods *platform,
                      const ProgramKeyType &key,
                      size_t programSize,
                      const uint8_t *programBytes);

class EGLProgramCacheControlTest : public ANGLETest
{
  public:
    void onCache(const ProgramKeyType &key, size_t programSize, const uint8_t *programBytes)
    {
        mCachedKey = key;
        mCachedBinary.assign(&programBytes[0], &programBytes[programSize]);
    }

  protected:
    EGLProgramCacheControlTest() { setDeferContextInit(true); }

    void SetUp() override
    {
        mPlatformMethods.cacheProgram = &TestCacheProgram;

        ANGLETestBase::ANGLETestSetUp();

        if (extensionAvailable())
        {
            EGLDisplay display = getEGLWindow()->getDisplay();
            setContextProgramCacheEnabled(true);
            eglProgramCacheResizeANGLE(display, kEnabledCacheSize, EGL_PROGRAM_CACHE_RESIZE_ANGLE);
        }

        getEGLWindow()->initializeContext();
    }

    void TearDown() override { ANGLETestBase::ANGLETestTearDown(); }

    bool extensionAvailable()
    {
        EGLDisplay display = getEGLWindow()->getDisplay();
        return eglDisplayExtensionEnabled(display, kEGLExtName);
    }

    bool programBinaryAvailable()
    {
        return (getClientMajorVersion() >= 3 || extensionEnabled("GL_OES_get_program_binary"));
    }

    ProgramKeyType mCachedKey;
    std::vector<uint8_t> mCachedBinary;
};

void TestCacheProgram(PlatformMethods *platform,
                      const ProgramKeyType &key,
                      size_t programSize,
                      const uint8_t *programBytes)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    auto *testCase =
        reinterpret_cast<EGLProgramCacheControlTest *>(testPlatformContext->currentTest);
    testCase->onCache(key, programSize, programBytes);
}

// Tests error conditions of the APIs.
TEST_P(EGLProgramCacheControlTest, NegativeAPI)
{
    ANGLE_SKIP_TEST_IF(!extensionAvailable());

    constexpr char kDefaultKey[]        = "defaultMakeItLongEnough";
    constexpr char kDefaultBinary[]     = "defaultMakeItLongEnough";
    constexpr EGLint kDefaultKeySize    = static_cast<EGLint>(ArraySize(kDefaultKey));
    constexpr EGLint kDefaultBinarySize = static_cast<EGLint>(ArraySize(kDefaultBinary));

    // Test that passing an invalid display to the entry point methods fails.
    eglProgramCacheGetAttribANGLE(EGL_NO_DISPLAY, EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    eglProgramCachePopulateANGLE(EGL_NO_DISPLAY, kDefaultKey, kDefaultKeySize, kDefaultBinary,
                                 kDefaultBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    EGLint tempKeySize    = 0;
    EGLint tempBinarySize = 0;
    eglProgramCacheQueryANGLE(EGL_NO_DISPLAY, 0, nullptr, &tempKeySize, nullptr, &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    eglProgramCacheResizeANGLE(EGL_NO_DISPLAY, 0, EGL_PROGRAM_CACHE_TRIM_ANGLE);
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);

    // Test querying properties with bad parameters.
    EGLDisplay display = getEGLWindow()->getDisplay();
    eglProgramCacheGetAttribANGLE(display, EGL_PROGRAM_CACHE_RESIZE_ANGLE);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Test populating with invalid parameters.
    EGLint keySize = eglProgramCacheGetAttribANGLE(display, EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE);
    EXPECT_GT(kDefaultKeySize, keySize);
    eglProgramCachePopulateANGLE(display, kDefaultKey, keySize + 1, kDefaultBinary,
                                 kDefaultBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCachePopulateANGLE(display, kDefaultKey, keySize, kDefaultBinary, -1);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCachePopulateANGLE(display, nullptr, keySize, kDefaultBinary, kDefaultBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCachePopulateANGLE(display, kDefaultKey, keySize, nullptr, kDefaultBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    // Test querying cache entries with invalid parameters.
    eglProgramCachePopulateANGLE(display, kDefaultKey, keySize, kDefaultBinary, kDefaultBinarySize);
    ASSERT_EGL_SUCCESS();

    EGLint cacheSize = eglProgramCacheGetAttribANGLE(display, EGL_PROGRAM_CACHE_SIZE_ANGLE);
    ASSERT_EQ(1, cacheSize);

    eglProgramCacheQueryANGLE(display, -1, nullptr, &tempKeySize, nullptr, &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCacheQueryANGLE(display, 1, nullptr, &tempKeySize, nullptr, &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCacheQueryANGLE(display, 0, nullptr, nullptr, nullptr, &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCacheQueryANGLE(display, 0, nullptr, &tempKeySize, nullptr, nullptr);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCacheQueryANGLE(display, 0, nullptr, &tempKeySize, nullptr, &tempBinarySize);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(keySize, tempKeySize);
    ASSERT_EQ(kDefaultBinarySize, tempBinarySize);

    std::vector<uint8_t> tempKey(tempKeySize + 5);
    std::vector<uint8_t> tempBinary(tempBinarySize + 5);

    tempKeySize--;

    eglProgramCacheQueryANGLE(display, 0, tempKey.data(), &tempKeySize, tempBinary.data(),
                              &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    tempKeySize++;
    tempBinarySize--;

    eglProgramCacheQueryANGLE(display, 0, tempKey.data(), &tempKeySize, tempBinary.data(),
                              &tempBinarySize);
    EXPECT_EGL_ERROR(EGL_BAD_ACCESS);

    // Test resizing with invalid parameters.
    eglProgramCacheResizeANGLE(display, -1, EGL_PROGRAM_CACHE_TRIM_ANGLE);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);

    eglProgramCacheResizeANGLE(display, 0, EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE);
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
}

// Tests a basic use case.
TEST_P(EGLProgramCacheControlTest, SaveAndReload)
{
    ANGLE_SKIP_TEST_IF(!extensionAvailable() || !programBinaryAvailable());

    const std::string vertexShader =
        "attribute vec4 position; void main() { gl_Position = position; }";
    const std::string fragmentShader = "void main() { gl_FragColor = vec4(1, 0, 0, 1); }";

    // Link a program, which will miss the cache.
    {
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
        drawQuad(program, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    }

    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLint cacheSize   = eglProgramCacheGetAttribANGLE(display, EGL_PROGRAM_CACHE_SIZE_ANGLE);
    EXPECT_EQ(1, cacheSize);

    EGLint keySize    = 0;
    EGLint binarySize = 0;
    eglProgramCacheQueryANGLE(display, 0, nullptr, &keySize, nullptr, &binarySize);
    EXPECT_EQ(static_cast<EGLint>(mCachedKey.size()), keySize);
    ASSERT_EGL_SUCCESS();

    ProgramKeyType keyBuffer;
    std::vector<uint8_t> binaryBuffer(binarySize);
    eglProgramCacheQueryANGLE(display, 0, keyBuffer.data(), &keySize, binaryBuffer.data(),
                              &binarySize);
    ASSERT_EGL_SUCCESS();

    EXPECT_EQ(mCachedKey, keyBuffer);
    EXPECT_EQ(mCachedBinary, binaryBuffer);

    // Restart EGL and GL.
    TearDown();
    SetUp();

    // Warm up the cache.
    EGLint newCacheSize = eglProgramCacheGetAttribANGLE(display, EGL_PROGRAM_CACHE_SIZE_ANGLE);
    EXPECT_EQ(0, newCacheSize);
    eglProgramCachePopulateANGLE(display, keyBuffer.data(), keySize, binaryBuffer.data(),
                                 binarySize);

    mCachedBinary.clear();

    // Link a program, which will hit the cache.
    {
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
        drawQuad(program, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    }

    // Verify no new shader was compiled.
    EXPECT_TRUE(mCachedBinary.empty());
}

ANGLE_INSTANTIATE_TEST(EGLProgramCacheControlTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());

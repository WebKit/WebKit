//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETest:
//   Implementation of common ANGLE testing fixture.
//

#include "ANGLETest.h"

#include "common/platform.h"
#include "gpu_info_util/SystemInfo.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/test_utils.h"

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include <VersionHelpers.h>
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

namespace angle
{

const GLColorRGB GLColorRGB::black(0u, 0u, 0u);
const GLColorRGB GLColorRGB::blue(0u, 0u, 255u);
const GLColorRGB GLColorRGB::green(0u, 255u, 0u);
const GLColorRGB GLColorRGB::red(255u, 0u, 0u);
const GLColorRGB GLColorRGB::yellow(255u, 255u, 0);

const GLColor GLColor::black            = GLColor(0u, 0u, 0u, 255u);
const GLColor GLColor::blue             = GLColor(0u, 0u, 255u, 255u);
const GLColor GLColor::cyan             = GLColor(0u, 255u, 255u, 255u);
const GLColor GLColor::green            = GLColor(0u, 255u, 0u, 255u);
const GLColor GLColor::red              = GLColor(255u, 0u, 0u, 255u);
const GLColor GLColor::transparentBlack = GLColor(0u, 0u, 0u, 0u);
const GLColor GLColor::white            = GLColor(255u, 255u, 255u, 255u);
const GLColor GLColor::yellow           = GLColor(255u, 255u, 0, 255u);
const GLColor GLColor::magenta          = GLColor(255u, 0u, 255u, 255u);

namespace
{
float ColorNorm(GLubyte channelValue)
{
    return static_cast<float>(channelValue) / 255.0f;
}

GLubyte ColorDenorm(float colorValue)
{
    return static_cast<GLubyte>(colorValue * 255.0f);
}

void TestPlatform_logError(PlatformMethods *platform, const char *errorMessage)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->ignoreMessages)
        return;

    GTEST_NONFATAL_FAILURE_(errorMessage);

    PrintStackBacktrace();
}

void TestPlatform_logWarning(PlatformMethods *platform, const char *warningMessage)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->ignoreMessages)
        return;

    if (testPlatformContext->warningsAsErrors)
    {
        FAIL() << warningMessage;
    }
    else
    {
        std::cerr << "Warning: " << warningMessage << std::endl;
    }
}

void TestPlatform_logInfo(PlatformMethods *platform, const char *infoMessage) {}

void TestPlatform_overrideWorkaroundsD3D(PlatformMethods *platform, FeaturesD3D *featuresD3D)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->currentTest)
    {
        testPlatformContext->currentTest->overrideWorkaroundsD3D(featuresD3D);
    }
}

void TestPlatform_overrideFeaturesVk(PlatformMethods *platform, FeaturesVk *featuresVulkan)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->currentTest)
    {
        testPlatformContext->currentTest->overrideFeaturesVk(featuresVulkan);
    }
}

const std::array<Vector3, 6> kQuadVertices = {{
    Vector3(-1.0f, 1.0f, 0.5f),
    Vector3(-1.0f, -1.0f, 0.5f),
    Vector3(1.0f, -1.0f, 0.5f),
    Vector3(-1.0f, 1.0f, 0.5f),
    Vector3(1.0f, -1.0f, 0.5f),
    Vector3(1.0f, 1.0f, 0.5f),
}};

const std::array<Vector3, 4> kIndexedQuadVertices = {{
    Vector3(-1.0f, 1.0f, 0.5f),
    Vector3(-1.0f, -1.0f, 0.5f),
    Vector3(1.0f, -1.0f, 0.5f),
    Vector3(1.0f, 1.0f, 0.5f),
}};

constexpr std::array<GLushort, 6> kIndexedQuadIndices = {{0, 1, 2, 0, 2, 3}};

const char *GetColorName(GLColor color)
{
    if (color == GLColor::red)
    {
        return "Red";
    }

    if (color == GLColor::green)
    {
        return "Green";
    }

    if (color == GLColor::blue)
    {
        return "Blue";
    }

    if (color == GLColor::white)
    {
        return "White";
    }

    if (color == GLColor::black)
    {
        return "Black";
    }

    if (color == GLColor::transparentBlack)
    {
        return "Transparent Black";
    }

    if (color == GLColor::yellow)
    {
        return "Yellow";
    }

    if (color == GLColor::magenta)
    {
        return "Magenta";
    }

    if (color == GLColor::cyan)
    {
        return "Cyan";
    }

    return nullptr;
}

bool ShouldAlwaysForceNewDisplay()
{
    // We prefer to reuse config displays. This is faster and solves a driver issue where creating
    // many displays causes crashes. However this exposes other driver bugs on many other platforms.
    // Conservatively enable the feature only on Windows Intel and NVIDIA for now.
    SystemInfo *systemInfo = GetTestSystemInfo();
    return (!systemInfo || !IsWindows() || systemInfo->hasAMDGPU());
}
}  // anonymous namespace

GLColorRGB::GLColorRGB(const Vector3 &floatColor)
    : R(ColorDenorm(floatColor.x())), G(ColorDenorm(floatColor.y())), B(ColorDenorm(floatColor.z()))
{}

GLColor::GLColor(const Vector4 &floatColor)
    : R(ColorDenorm(floatColor.x())),
      G(ColorDenorm(floatColor.y())),
      B(ColorDenorm(floatColor.z())),
      A(ColorDenorm(floatColor.w()))
{}

GLColor::GLColor(GLuint colorValue) : R(0), G(0), B(0), A(0)
{
    memcpy(&R, &colorValue, sizeof(GLuint));
}

testing::AssertionResult GLColor::ExpectNear(const GLColor &expected, const GLColor &err) const
{
    testing::AssertionResult result(
        abs(int(expected.R) - this->R) <= err.R && abs(int(expected.G) - this->G) <= err.G &&
        abs(int(expected.B) - this->B) <= err.B && abs(int(expected.A) - this->A) <= err.A);
    if (!bool(result))
    {
        result << "Expected " << expected << "+/-" << err << ", was " << *this;
    }
    return result;
}

void CreatePixelCenterWindowCoords(const std::vector<Vector2> &pixelPoints,
                                   int windowWidth,
                                   int windowHeight,
                                   std::vector<Vector3> *outVertices)
{
    for (Vector2 pixelPoint : pixelPoints)
    {
        outVertices->emplace_back(Vector3((pixelPoint[0] + 0.5f) * 2.0f / windowWidth - 1.0f,
                                          (pixelPoint[1] + 0.5f) * 2.0f / windowHeight - 1.0f,
                                          0.0f));
    }
}

Vector4 GLColor::toNormalizedVector() const
{
    return Vector4(ColorNorm(R), ColorNorm(G), ColorNorm(B), ColorNorm(A));
}

GLColor ReadColor(GLint x, GLint y)
{
    GLColor actual;
    glReadPixels((x), (y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &actual.R);
    EXPECT_GL_NO_ERROR();
    return actual;
}

bool operator==(const GLColor &a, const GLColor &b)
{
    return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
}

bool operator!=(const GLColor &a, const GLColor &b)
{
    return !(a == b);
}

std::ostream &operator<<(std::ostream &ostream, const GLColor &color)
{
    const char *colorName = GetColorName(color);
    if (colorName)
    {
        return ostream << colorName;
    }

    ostream << "(" << static_cast<unsigned int>(color.R) << ", "
            << static_cast<unsigned int>(color.G) << ", " << static_cast<unsigned int>(color.B)
            << ", " << static_cast<unsigned int>(color.A) << ")";
    return ostream;
}

bool operator==(const GLColor32F &a, const GLColor32F &b)
{
    return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
}

std::ostream &operator<<(std::ostream &ostream, const GLColor32F &color)
{
    ostream << "(" << color.R << ", " << color.G << ", " << color.B << ", " << color.A << ")";
    return ostream;
}

GLColor32F ReadColor32F(GLint x, GLint y)
{
    GLColor32F actual;
    glReadPixels((x), (y), 1, 1, GL_RGBA, GL_FLOAT, &actual.R);
    EXPECT_GL_NO_ERROR();
    return actual;
}
}  // namespace angle

using namespace angle;

PlatformMethods gDefaultPlatformMethods;

namespace
{
TestPlatformContext gPlatformContext;

// After a fixed number of iterations we reset the test window. This works around some driver bugs.
constexpr uint32_t kWindowReuseLimit = 50;

constexpr char kUseConfig[]                = "--use-config=";
constexpr char kSeparateProcessPerConfig[] = "--separate-process-per-config";

bool RunSeparateProcessesForEachConfig(int *argc, char *argv[])
{
    std::vector<const char *> commonArgs;
    for (int argIndex = 0; argIndex < *argc; ++argIndex)
    {
        if (strncmp(argv[argIndex], kSeparateProcessPerConfig, strlen(kSeparateProcessPerConfig)) !=
            0)
        {
            commonArgs.push_back(argv[argIndex]);
        }
    }

    // Force GoogleTest init now so that we hit the test config init in angle_test_instantiate.cpp.
    // After instantiation is finished we can gather a full list of enabled configs. Then we can
    // iterate the list of configs to spawn a child process for each enabled config.
    testing::InitGoogleTest(argc, argv);

    std::vector<std::string> configNames = GetAvailableTestPlatformNames();

    bool success = true;

    for (const std::string &config : configNames)
    {
        std::stringstream strstr;
        strstr << kUseConfig << config;

        std::string configStr = strstr.str();

        std::vector<const char *> childArgs = commonArgs;
        childArgs.push_back(configStr.c_str());

        ProcessHandle process(childArgs, false, false);
        if (!process->started() || !process->finish())
        {
            std::cerr << "Launching child config " << config << " failed.\n";
        }
        else if (process->getExitCode() != 0)
        {
            std::cerr << "Child config " << config << " failed with exit code "
                      << process->getExitCode() << ".\n";
            success = false;
        }
    }
    return success;
}
}  // anonymous namespace

// static
std::array<Vector3, 6> ANGLETestBase::GetQuadVertices()
{
    return kQuadVertices;
}

// static
std::array<GLushort, 6> ANGLETestBase::GetQuadIndices()
{
    return kIndexedQuadIndices;
}

// static
std::array<Vector3, 4> ANGLETestBase::GetIndexedQuadVertices()
{
    return kIndexedQuadVertices;
}

ANGLETestBase::ANGLETestBase(const PlatformParameters &params)
    : mWidth(16),
      mHeight(16),
      mIgnoreD3D11SDKLayersWarnings(false),
      mQuadVertexBuffer(0),
      mQuadIndexBuffer(0),
      m2DTexturedQuadProgram(0),
      m3DTexturedQuadProgram(0),
      mDeferContextInit(false),
      mAlwaysForceNewDisplay(ShouldAlwaysForceNewDisplay()),
      mForceNewDisplay(mAlwaysForceNewDisplay),
      mSetUpCalled(false),
      mTearDownCalled(false),
      mCurrentParams(nullptr),
      mFixture(nullptr)
{
    // Override the default platform methods with the ANGLE test methods pointer.
    PlatformParameters withMethods            = params;
    withMethods.eglParameters.platformMethods = &gDefaultPlatformMethods;

    auto iter = gFixtures.find(withMethods);
    if (iter != gFixtures.end())
    {
        mCurrentParams = &iter->first;

        if (!params.noFixture)
        {
            mFixture = &iter->second;
            mFixture->configParams.reset();
        }
        return;
    }

    TestFixture platform;
    auto insertIter = gFixtures.emplace(withMethods, platform);
    mCurrentParams  = &insertIter.first->first;

    if (!params.noFixture)
    {
        mFixture = &insertIter.first->second;
        initOSWindow();
    }
}

void ANGLETestBase::initOSWindow()
{
    std::stringstream windowNameStream;
    windowNameStream << "ANGLE Tests - " << *mCurrentParams;
    std::string windowName = windowNameStream.str();

    if (mAlwaysForceNewDisplay)
    {
        mFixture->osWindow = mOSWindowSingleton;
    }

    if (!mFixture->osWindow)
    {
        mFixture->osWindow = OSWindow::New();
        if (!mFixture->osWindow->initialize(windowName.c_str(), 128, 128))
        {
            std::cerr << "Failed to initialize OS Window.";
        }

        mOSWindowSingleton = mFixture->osWindow;
    }

    // On Linux we must keep the test windows visible. On Windows it doesn't seem to need it.
    mFixture->osWindow->setVisible(!IsWindows());

    switch (mCurrentParams->driver)
    {
        case GLESDriverType::AngleEGL:
        {
            mFixture->eglWindow =
                EGLWindow::New(mCurrentParams->majorVersion, mCurrentParams->minorVersion);
            break;
        }

        case GLESDriverType::SystemEGL:
        {
            std::cerr << "Unsupported driver." << std::endl;
            break;
        }

        case GLESDriverType::SystemWGL:
        {
            // WGL tests are currently disabled.
            std::cerr << "Unsupported driver." << std::endl;
            break;
        }
    }
}

ANGLETestBase::~ANGLETestBase()
{
    if (mQuadVertexBuffer)
    {
        glDeleteBuffers(1, &mQuadVertexBuffer);
    }
    if (mQuadIndexBuffer)
    {
        glDeleteBuffers(1, &mQuadIndexBuffer);
    }
    if (m2DTexturedQuadProgram)
    {
        glDeleteProgram(m2DTexturedQuadProgram);
    }
    if (m3DTexturedQuadProgram)
    {
        glDeleteProgram(m3DTexturedQuadProgram);
    }

    if (!mSetUpCalled)
    {
        GTEST_NONFATAL_FAILURE_("SetUp not called.");
    }

    if (!mTearDownCalled)
    {
        GTEST_NONFATAL_FAILURE_("TearDown not called.");
    }
}

void ANGLETestBase::ANGLETestSetUp()
{
    mSetUpCalled = true;

    gDefaultPlatformMethods.overrideWorkaroundsD3D = TestPlatform_overrideWorkaroundsD3D;
    gDefaultPlatformMethods.overrideFeaturesVk     = TestPlatform_overrideFeaturesVk;
    gDefaultPlatformMethods.logError               = TestPlatform_logError;
    gDefaultPlatformMethods.logWarning             = TestPlatform_logWarning;
    gDefaultPlatformMethods.logInfo                = TestPlatform_logInfo;
    gDefaultPlatformMethods.context                = &gPlatformContext;

    gPlatformContext.ignoreMessages   = false;
    gPlatformContext.warningsAsErrors = false;
    gPlatformContext.currentTest      = this;

    // TODO(geofflang): Nexus6P generates GL errors during initialization. Suppress error messages
    // temporarily until enough logging is in place to figure out exactly which calls generate
    // errors.  http://crbug.com/998503
    if (IsNexus6P())
    {
        gPlatformContext.ignoreMessages = true;
    }

    if (IsWindows())
    {
        const auto &info = testing::UnitTest::GetInstance()->current_test_info();
        WriteDebugMessage("Entering %s.%s\n", info->test_case_name(), info->name());
    }

    if (mCurrentParams->noFixture)
    {
#if defined(ANGLE_USE_UTIL_LOADER)
        PFNEGLGETPROCADDRESSPROC getProcAddress;
        ANGLETestEnvironment::GetEGLLibrary()->getAs("eglGetProcAddress", &getProcAddress);
        ASSERT_NE(nullptr, getProcAddress);

        LoadEGL(getProcAddress);
        LoadGLES(getProcAddress);
#endif  // defined(ANGLE_USE_UTIL_LOADER)
        return;
    }

    // Resize the window before creating the context so that the first make current
    // sets the viewport and scissor box to the right size.
    bool needSwap = false;
    if (mFixture->osWindow->getWidth() != mWidth || mFixture->osWindow->getHeight() != mHeight)
    {
        if (!mFixture->osWindow->resize(mWidth, mHeight))
        {
            FAIL() << "Failed to resize ANGLE test window.";
        }
        needSwap = true;
    }

    // WGL tests are currently disabled.
    if (mFixture->wglWindow)
    {
        FAIL() << "Unsupported driver.";
    }
    else
    {
        if (mForceNewDisplay || !mFixture->eglWindow->isDisplayInitialized())
        {
            mFixture->eglWindow->destroyGL();
            if (!mFixture->eglWindow->initializeDisplay(mFixture->osWindow,
                                                        ANGLETestEnvironment::GetEGLLibrary(),
                                                        mCurrentParams->eglParameters))
            {
                FAIL() << "EGL Display init failed.";
            }
        }
        else if (mCurrentParams->eglParameters != mFixture->eglWindow->getPlatform())
        {
            FAIL() << "Internal parameter conflict error.";
        }

        if (!mFixture->eglWindow->initializeSurface(
                mFixture->osWindow, ANGLETestEnvironment::GetEGLLibrary(), mFixture->configParams))
        {
            FAIL() << "egl surface init failed.";
        }

        if (!mDeferContextInit && !mFixture->eglWindow->initializeContext())
        {
            FAIL() << "GL Context init failed.";
        }
    }

    if (needSwap)
    {
        // Swap the buffers so that the default framebuffer picks up the resize
        // which will allow follow-up test code to assume the framebuffer covers
        // the whole window.
        swapBuffers();
    }

    // This Viewport command is not strictly necessary but we add it so that programs
    // taking OpenGL traces can guess the size of the default framebuffer and show it
    // in their UIs
    glViewport(0, 0, mWidth, mHeight);
}

void ANGLETestBase::ANGLETestTearDown()
{
    mTearDownCalled              = true;
    gPlatformContext.currentTest = nullptr;

    if (IsWindows())
    {
        const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
        WriteDebugMessage("Exiting %s.%s\n", info->test_case_name(), info->name());
    }

    if (mCurrentParams->noFixture)
    {
        return;
    }

    swapBuffers();
    mFixture->osWindow->messageLoop();

    if (mFixture->eglWindow)
    {
        checkD3D11SDKLayersMessages();
    }

    if (mFixture->reuseCounter++ >= kWindowReuseLimit || mForceNewDisplay)
    {
        mFixture->reuseCounter = 0;
        getGLWindow()->destroyGL();
    }
    else
    {
        mFixture->eglWindow->destroyContext();
        mFixture->eglWindow->destroySurface();
    }

    // Check for quit message
    Event myEvent;
    while (mFixture->osWindow->popEvent(&myEvent))
    {
        if (myEvent.Type == Event::EVENT_CLOSED)
        {
            exit(0);
        }
    }
}

void ANGLETestBase::ReleaseFixtures()
{
    for (auto it = gFixtures.begin(); it != gFixtures.end(); it++)
    {
        if (it->second.eglWindow)
        {
            it->second.eglWindow->destroyGL();
        }
    }
}

void ANGLETestBase::swapBuffers()
{
    if (getGLWindow()->isGLInitialized())
    {
        getGLWindow()->swap();

        if (mFixture->eglWindow)
        {
            EXPECT_EGL_SUCCESS();
        }
    }
}

void ANGLETestBase::setupQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale)
{
    if (mQuadVertexBuffer == 0)
    {
        glGenBuffers(1, &mQuadVertexBuffer);
    }

    auto quadVertices = GetQuadVertices();
    for (Vector3 &vertex : quadVertices)
    {
        vertex.x() *= positionAttribXYScale;
        vertex.y() *= positionAttribXYScale;
        vertex.z() = positionAttribZ;
    }

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(), GL_STATIC_DRAW);
}

void ANGLETestBase::setupIndexedQuadVertexBuffer(GLfloat positionAttribZ,
                                                 GLfloat positionAttribXYScale)
{
    if (mQuadVertexBuffer == 0)
    {
        glGenBuffers(1, &mQuadVertexBuffer);
    }

    auto quadVertices = kIndexedQuadVertices;
    for (Vector3 &vertex : quadVertices)
    {
        vertex.x() *= positionAttribXYScale;
        vertex.y() *= positionAttribXYScale;
        vertex.z() = positionAttribZ;
    }

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 4, quadVertices.data(), GL_STATIC_DRAW);
}

void ANGLETestBase::setupIndexedQuadIndexBuffer()
{
    if (mQuadIndexBuffer == 0)
    {
        glGenBuffers(1, &mQuadIndexBuffer);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mQuadIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndexedQuadIndices), kIndexedQuadIndices.data(),
                 GL_STATIC_DRAW);
}

// static
void ANGLETestBase::drawQuad(GLuint program,
                             const std::string &positionAttribName,
                             GLfloat positionAttribZ)
{
    drawQuad(program, positionAttribName, positionAttribZ, 1.0f);
}

// static
void ANGLETestBase::drawQuad(GLuint program,
                             const std::string &positionAttribName,
                             GLfloat positionAttribZ,
                             GLfloat positionAttribXYScale)
{
    drawQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale, false);
}

void ANGLETestBase::drawQuad(GLuint program,
                             const std::string &positionAttribName,
                             GLfloat positionAttribZ,
                             GLfloat positionAttribXYScale,
                             bool useVertexBuffer)
{
    drawQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale, useVertexBuffer,
             false, 0u);
}

void ANGLETestBase::drawQuadInstanced(GLuint program,
                                      const std::string &positionAttribName,
                                      GLfloat positionAttribZ,
                                      GLfloat positionAttribXYScale,
                                      bool useVertexBuffer,
                                      GLuint numInstances)
{
    drawQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale, useVertexBuffer,
             true, numInstances);
}

void ANGLETestBase::drawQuad(GLuint program,
                             const std::string &positionAttribName,
                             GLfloat positionAttribZ,
                             GLfloat positionAttribXYScale,
                             bool useVertexBuffer,
                             bool useInstancedDrawCalls,
                             GLuint numInstances)
{
    GLint previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    if (previousProgram != static_cast<GLint>(program))
    {
        glUseProgram(program);
    }

    GLint previousBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);

    GLint positionLocation = glGetAttribLocation(program, positionAttribName.c_str());

    std::array<Vector3, 6> quadVertices = GetQuadVertices();

    if (useVertexBuffer)
    {
        setupQuadVertexBuffer(positionAttribZ, positionAttribXYScale);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
    }
    else
    {
        for (Vector3 &vertex : quadVertices)
        {
            vertex.x() *= positionAttribXYScale;
            vertex.y() *= positionAttribXYScale;
            vertex.z() = positionAttribZ;
        }

        if (previousBuffer != 0)
        {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
        if (previousBuffer != 0)
        {
            glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
        }
    }
    glEnableVertexAttribArray(positionLocation);

    if (useInstancedDrawCalls)
    {
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, numInstances);
    }
    else
    {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    if (previousProgram != static_cast<GLint>(program))
    {
        glUseProgram(previousProgram);
    }
}

void ANGLETestBase::drawIndexedQuad(GLuint program,
                                    const std::string &positionAttribName,
                                    GLfloat positionAttribZ)
{
    drawIndexedQuad(program, positionAttribName, positionAttribZ, 1.0f);
}

void ANGLETestBase::drawIndexedQuad(GLuint program,
                                    const std::string &positionAttribName,
                                    GLfloat positionAttribZ,
                                    GLfloat positionAttribXYScale)
{
    drawIndexedQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale, false);
}

void ANGLETestBase::drawIndexedQuad(GLuint program,
                                    const std::string &positionAttribName,
                                    GLfloat positionAttribZ,
                                    GLfloat positionAttribXYScale,
                                    bool useIndexBuffer)
{
    drawIndexedQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale,
                    useIndexBuffer, false);
}

void ANGLETestBase::drawIndexedQuad(GLuint program,
                                    const std::string &positionAttribName,
                                    GLfloat positionAttribZ,
                                    GLfloat positionAttribXYScale,
                                    bool useIndexBuffer,
                                    bool restrictedRange)
{
    GLint positionLocation = glGetAttribLocation(program, positionAttribName.c_str());

    GLint activeProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(program);
    }

    GLuint prevCoordBinding = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint *>(&prevCoordBinding));

    setupIndexedQuadVertexBuffer(positionAttribZ, positionAttribXYScale);

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);
    glBindBuffer(GL_ARRAY_BUFFER, prevCoordBinding);

    GLuint prevIndexBinding = 0;
    const GLvoid *indices;
    if (useIndexBuffer)
    {
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                      reinterpret_cast<GLint *>(&prevIndexBinding));

        setupIndexedQuadIndexBuffer();
        indices = 0;
    }
    else
    {
        indices = kIndexedQuadIndices.data();
    }

    if (!restrictedRange)
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    }
    else
    {
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, indices);
    }

    if (useIndexBuffer)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevIndexBinding);
    }

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(static_cast<GLuint>(activeProgram));
    }
}

GLuint ANGLETestBase::get2DTexturedQuadProgram()
{
    if (m2DTexturedQuadProgram)
    {
        return m2DTexturedQuadProgram;
    }

    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "varying mediump vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}\n";

    constexpr char kFS[] =
        "varying mediump vec2 texCoord;\n"
        "uniform sampler2D tex;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = texture2D(tex, texCoord);\n"
        "}\n";

    m2DTexturedQuadProgram = CompileProgram(kVS, kFS);
    return m2DTexturedQuadProgram;
}

GLuint ANGLETestBase::get3DTexturedQuadProgram()
{
    if (m3DTexturedQuadProgram)
    {
        return m3DTexturedQuadProgram;
    }

    constexpr char kVS[] = R"(#version 300 es
in vec2 position;
out vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = position * 0.5 + vec2(0.5);
})";

    constexpr char kFS[] = R"(#version 300 es
precision highp float;

in vec2 texCoord;
out vec4 my_FragColor;

uniform highp sampler3D tex;
uniform float u_layer;

void main()
{
    my_FragColor = texture(tex, vec3(texCoord, u_layer));
})";

    m3DTexturedQuadProgram = CompileProgram(kVS, kFS);
    return m3DTexturedQuadProgram;
}

void ANGLETestBase::draw2DTexturedQuad(GLfloat positionAttribZ,
                                       GLfloat positionAttribXYScale,
                                       bool useVertexBuffer)
{
    ASSERT_NE(0u, get2DTexturedQuadProgram());
    drawQuad(get2DTexturedQuadProgram(), "position", positionAttribZ, positionAttribXYScale,
             useVertexBuffer);
}

void ANGLETestBase::draw3DTexturedQuad(GLfloat positionAttribZ,
                                       GLfloat positionAttribXYScale,
                                       bool useVertexBuffer,
                                       float layer)
{
    GLuint program = get3DTexturedQuadProgram();
    ASSERT_NE(0u, program);
    GLint activeProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(program);
    }
    glUniform1f(glGetUniformLocation(program, "u_layer"), layer);

    drawQuad(program, "position", positionAttribZ, positionAttribXYScale, useVertexBuffer);

    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(static_cast<GLuint>(activeProgram));
    }
}

bool ANGLETestBase::platformSupportsMultithreading() const
{
    return (IsOpenGLES() && IsAndroid()) || IsVulkan();
}

void ANGLETestBase::checkD3D11SDKLayersMessages()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    // On Windows D3D11, check ID3D11InfoQueue to see if any D3D11 SDK Layers messages
    // were outputted by the test. We enable the Debug layers in Release tests as well.
    if (mIgnoreD3D11SDKLayersWarnings ||
        mFixture->eglWindow->getPlatform().renderer != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
        mFixture->eglWindow->getDisplay() == EGL_NO_DISPLAY)
    {
        return;
    }

    const char *extensionString = static_cast<const char *>(
        eglQueryString(mFixture->eglWindow->getDisplay(), EGL_EXTENSIONS));
    if (!extensionString)
    {
        std::cout << "Error getting extension string from EGL Window." << std::endl;
        return;
    }

    if (!strstr(extensionString, "EGL_EXT_device_query"))
    {
        return;
    }

    EGLAttrib device      = 0;
    EGLAttrib angleDevice = 0;

    ASSERT_EGL_TRUE(
        eglQueryDisplayAttribEXT(mFixture->eglWindow->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
    ASSERT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                            EGL_D3D11_DEVICE_ANGLE, &device));
    ID3D11Device *d3d11Device = reinterpret_cast<ID3D11Device *>(device);

    ID3D11InfoQueue *infoQueue = nullptr;
    HRESULT hr =
        d3d11Device->QueryInterface(__uuidof(infoQueue), reinterpret_cast<void **>(&infoQueue));
    if (SUCCEEDED(hr))
    {
        UINT64 numStoredD3DDebugMessages =
            infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

        if (numStoredD3DDebugMessages > 0)
        {
            for (UINT64 i = 0; i < numStoredD3DDebugMessages; i++)
            {
                SIZE_T messageLength = 0;
                hr                   = infoQueue->GetMessage(i, nullptr, &messageLength);

                if (SUCCEEDED(hr))
                {
                    D3D11_MESSAGE *pMessage =
                        reinterpret_cast<D3D11_MESSAGE *>(malloc(messageLength));
                    infoQueue->GetMessage(i, pMessage, &messageLength);

                    std::cout << "Message " << i << ":"
                              << " " << pMessage->pDescription << "\n";
                    free(pMessage);
                }
            }
            // Clear the queue, so that previous failures are not reported
            // for subsequent, otherwise passing, tests
            infoQueue->ClearStoredMessages();

            FAIL() << numStoredD3DDebugMessages
                   << " D3D11 SDK Layers message(s) detected! Test Failed.\n";
        }
    }

    SafeRelease(infoQueue);
#endif  // defined(ANGLE_PLATFORM_WINDOWS)
}

void ANGLETestBase::setWindowWidth(int width)
{
    mWidth = width;
}

void ANGLETestBase::setWindowHeight(int height)
{
    mHeight = height;
}

GLWindowBase *ANGLETestBase::getGLWindow() const
{
    // WGL tests are currently disabled.
    assert(!mFixture->wglWindow);
    return mFixture->eglWindow;
}

void ANGLETestBase::setConfigRedBits(int bits)
{
    mFixture->configParams.redBits = bits;
}

void ANGLETestBase::setConfigGreenBits(int bits)
{
    mFixture->configParams.greenBits = bits;
}

void ANGLETestBase::setConfigBlueBits(int bits)
{
    mFixture->configParams.blueBits = bits;
}

void ANGLETestBase::setConfigAlphaBits(int bits)
{
    mFixture->configParams.alphaBits = bits;
}

void ANGLETestBase::setConfigDepthBits(int bits)
{
    mFixture->configParams.depthBits = bits;
}

void ANGLETestBase::setConfigStencilBits(int bits)
{
    mFixture->configParams.stencilBits = bits;
}

void ANGLETestBase::setConfigComponentType(EGLenum componentType)
{
    mFixture->configParams.componentType = componentType;
}

void ANGLETestBase::setMultisampleEnabled(bool enabled)
{
    mFixture->configParams.multisample = enabled;
}

void ANGLETestBase::setSamples(EGLint samples)
{
    mFixture->configParams.samples = samples;
}

void ANGLETestBase::setDebugEnabled(bool enabled)
{
    mFixture->configParams.debug = enabled;
}

void ANGLETestBase::setNoErrorEnabled(bool enabled)
{
    mFixture->configParams.noError = enabled;
}

void ANGLETestBase::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mFixture->configParams.webGLCompatibility = webglCompatibility;
}

void ANGLETestBase::setExtensionsEnabled(bool extensionsEnabled)
{
    mFixture->configParams.extensionsEnabled = extensionsEnabled;
}

void ANGLETestBase::setRobustAccess(bool enabled)
{
    mFixture->configParams.robustAccess = enabled;
}

void ANGLETestBase::setBindGeneratesResource(bool bindGeneratesResource)
{
    mFixture->configParams.bindGeneratesResource = bindGeneratesResource;
}

void ANGLETestBase::setClientArraysEnabled(bool enabled)
{
    mFixture->configParams.clientArraysEnabled = enabled;
}

void ANGLETestBase::setRobustResourceInit(bool enabled)
{
    mFixture->configParams.robustResourceInit = enabled;
}

void ANGLETestBase::setContextProgramCacheEnabled(bool enabled)
{
    mFixture->configParams.contextProgramCacheEnabled = enabled;
}

void ANGLETestBase::setContextResetStrategy(EGLenum resetStrategy)
{
    mFixture->configParams.resetStrategy = resetStrategy;
}

void ANGLETestBase::forceNewDisplay()
{
    mForceNewDisplay = true;
}

void ANGLETestBase::setDeferContextInit(bool enabled)
{
    mDeferContextInit = enabled;
}

int ANGLETestBase::getClientMajorVersion() const
{
    return getGLWindow()->getClientMajorVersion();
}

int ANGLETestBase::getClientMinorVersion() const
{
    return getGLWindow()->getClientMinorVersion();
}

EGLWindow *ANGLETestBase::getEGLWindow() const
{
    return mFixture->eglWindow;
}

int ANGLETestBase::getWindowWidth() const
{
    return mWidth;
}

int ANGLETestBase::getWindowHeight() const
{
    return mHeight;
}

bool ANGLETestBase::isMultisampleEnabled() const
{
    return mFixture->eglWindow->isMultisample();
}

void ANGLETestBase::setWindowVisible(bool isVisible)
{
    mFixture->osWindow->setVisible(isVisible);
}

ANGLETestBase::TestFixture::TestFixture()  = default;
ANGLETestBase::TestFixture::~TestFixture() = default;

EGLint ANGLETestBase::getPlatformRenderer() const
{
    assert(mFixture->eglWindow);
    return mFixture->eglWindow->getPlatform().renderer;
}

void ANGLETestBase::ignoreD3D11SDKLayersWarnings()
{
    // Some tests may need to disable the D3D11 SDK Layers Warnings checks
    mIgnoreD3D11SDKLayersWarnings = true;
}

void ANGLETestBase::treatPlatformWarningsAsErrors()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    // Only do warnings-as-errors on 8 and above. We may fall back to the old
    // compiler DLL on Windows 7.
    gPlatformContext.warningsAsErrors = IsWindows8OrGreater();
#endif  // defined(ANGLE_PLATFORM_WINDOWS)
}

ANGLETestBase::ScopedIgnorePlatformMessages::ScopedIgnorePlatformMessages()
{
    gPlatformContext.ignoreMessages = true;
}

ANGLETestBase::ScopedIgnorePlatformMessages::~ScopedIgnorePlatformMessages()
{
    gPlatformContext.ignoreMessages = false;
}

OSWindow *ANGLETestBase::mOSWindowSingleton = nullptr;
std::map<angle::PlatformParameters, ANGLETestBase::TestFixture> ANGLETestBase::gFixtures;
Optional<EGLint> ANGLETestBase::mLastRendererType;

std::unique_ptr<Library> ANGLETestEnvironment::gEGLLibrary;
std::unique_ptr<Library> ANGLETestEnvironment::gWGLLibrary;

void ANGLETestEnvironment::SetUp() {}

void ANGLETestEnvironment::TearDown()
{
    ANGLETestBase::ReleaseFixtures();
}

Library *ANGLETestEnvironment::GetEGLLibrary()
{
#if defined(ANGLE_USE_UTIL_LOADER)
    if (!gEGLLibrary)
    {
        gEGLLibrary.reset(OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, SearchType::ApplicationDir));
    }
#endif  // defined(ANGLE_USE_UTIL_LOADER)
    return gEGLLibrary.get();
}

Library *ANGLETestEnvironment::GetWGLLibrary()
{
#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
    if (!gWGLLibrary)
    {
        gWGLLibrary.reset(OpenSharedLibrary("opengl32", SearchType::SystemDir));
    }
#endif  // defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
    return gWGLLibrary.get();
}

void ANGLEProcessTestArgs(int *argc, char *argv[])
{
    testing::AddGlobalTestEnvironment(new ANGLETestEnvironment());

    for (int argIndex = 1; argIndex < *argc; argIndex++)
    {
        if (strncmp(argv[argIndex], kUseConfig, strlen(kUseConfig)) == 0)
        {
            gSelectedConfig = std::string(argv[argIndex] + strlen(kUseConfig));
        }
        if (strncmp(argv[argIndex], kSeparateProcessPerConfig, strlen(kSeparateProcessPerConfig)) ==
            0)
        {
            gSeparateProcessPerConfig = true;
        }
    }

    if (gSeparateProcessPerConfig)
    {
        if (!gSelectedConfig.empty())
        {
            std::cout << "Cannot use both a single test config and separate processes.\n";
            exit(1);
        }

        if (RunSeparateProcessesForEachConfig(argc, argv))
        {
            exit(0);
        }
        else
        {
            std::cout << "Some subprocesses failed.\n";
            exit(1);
        }
    }
}

//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETest:
//   Implementation of common ANGLE testing fixture.
//

#include "ANGLETest.h"
#include "EGLWindow.h"
#include "OSWindow.h"
#include "platform/Platform.h"

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

// Use a custom ANGLE platform class to capture and report internal errors.
class TestPlatform : public angle::Platform
{
  public:
    TestPlatform() : mIgnoreMessages(false) {}

    void logError(const char *errorMessage) override;
    void logWarning(const char *warningMessage) override;
    void logInfo(const char *infoMessage) override;

    void ignoreMessages();
    void enableMessages();

  private:
    bool mIgnoreMessages;
};

void TestPlatform::logError(const char *errorMessage)
{
    if (mIgnoreMessages)
        return;

    FAIL() << errorMessage;
}

void TestPlatform::logWarning(const char *warningMessage)
{
    if (mIgnoreMessages)
        return;

    std::cerr << "Warning: " << warningMessage << std::endl;
}

void TestPlatform::logInfo(const char *infoMessage)
{
    if (mIgnoreMessages)
        return;

    angle::WriteDebugMessage("%s\n", infoMessage);
}

void TestPlatform::ignoreMessages()
{
    mIgnoreMessages = true;
}

void TestPlatform::enableMessages()
{
    mIgnoreMessages = false;
}

TestPlatform g_testPlatformInstance;

std::array<Vector3, 4> GetIndexedQuadVertices()
{
    std::array<Vector3, 4> vertices;
    vertices[0] = Vector3(-1.0f, 1.0f, 0.5f);
    vertices[1] = Vector3(-1.0f, -1.0f, 0.5f);
    vertices[2] = Vector3(1.0f, -1.0f, 0.5f);
    vertices[3] = Vector3(1.0f, 1.0f, 0.5f);
    return vertices;
}

}  // anonymous namespace

GLColorRGB::GLColorRGB() : R(0), G(0), B(0)
{
}

GLColorRGB::GLColorRGB(GLubyte r, GLubyte g, GLubyte b) : R(r), G(g), B(b)
{
}

GLColorRGB::GLColorRGB(const Vector3 &floatColor)
    : R(ColorDenorm(floatColor.x)), G(ColorDenorm(floatColor.y)), B(ColorDenorm(floatColor.z))
{
}

GLColor::GLColor() : R(0), G(0), B(0), A(0)
{
}

GLColor::GLColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a) : R(r), G(g), B(b), A(a)
{
}

GLColor::GLColor(const Vector4 &floatColor)
    : R(ColorDenorm(floatColor.x)),
      G(ColorDenorm(floatColor.y)),
      B(ColorDenorm(floatColor.z)),
      A(ColorDenorm(floatColor.w))
{
}

GLColor::GLColor(GLuint colorValue) : R(0), G(0), B(0), A(0)
{
    memcpy(&R, &colorValue, sizeof(GLuint));
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

std::ostream &operator<<(std::ostream &ostream, const GLColor &color)
{
    ostream << "(" << static_cast<unsigned int>(color.R) << ", "
            << static_cast<unsigned int>(color.G) << ", " << static_cast<unsigned int>(color.B)
            << ", " << static_cast<unsigned int>(color.A) << ")";
    return ostream;
}

}  // namespace angle

// static
std::array<Vector3, 6> ANGLETest::GetQuadVertices()
{
    std::array<Vector3, 6> vertices;
    vertices[0] = Vector3(-1.0f, 1.0f, 0.5f);
    vertices[1] = Vector3(-1.0f, -1.0f, 0.5f);
    vertices[2] = Vector3(1.0f, -1.0f, 0.5f);
    vertices[3] = Vector3(-1.0f, 1.0f, 0.5f);
    vertices[4] = Vector3(1.0f, -1.0f, 0.5f);
    vertices[5] = Vector3(1.0f, 1.0f, 0.5f);
    return vertices;
}

ANGLETest::ANGLETest()
    : mEGLWindow(nullptr),
      mWidth(16),
      mHeight(16),
      mIgnoreD3D11SDKLayersWarnings(false),
      mQuadVertexBuffer(0)
{
    mEGLWindow =
        new EGLWindow(GetParam().majorVersion, GetParam().minorVersion, GetParam().eglParameters);
}

ANGLETest::~ANGLETest()
{
    if (mQuadVertexBuffer)
    {
        glDeleteBuffers(1, &mQuadVertexBuffer);
    }
    SafeDelete(mEGLWindow);
}

void ANGLETest::SetUp()
{
    angle::g_testPlatformInstance.enableMessages();

    // Resize the window before creating the context so that the first make current
    // sets the viewport and scissor box to the right size.
    bool needSwap = false;
    if (mOSWindow->getWidth() != mWidth || mOSWindow->getHeight() != mHeight)
    {
        if (!mOSWindow->resize(mWidth, mHeight))
        {
            FAIL() << "Failed to resize ANGLE test window.";
        }
        needSwap = true;
    }

    if (!createEGLContext())
    {
        FAIL() << "egl context creation failed.";
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

    const auto &info = testing::UnitTest::GetInstance()->current_test_info();
    angle::WriteDebugMessage("Entering %s.%s\n", info->test_case_name(), info->name());
}

void ANGLETest::TearDown()
{
    checkD3D11SDKLayersMessages();

    const auto &info = testing::UnitTest::GetInstance()->current_test_info();
    angle::WriteDebugMessage("Exiting %s.%s\n", info->test_case_name(), info->name());

    swapBuffers();
    mOSWindow->messageLoop();

    if (!destroyEGLContext())
    {
        FAIL() << "egl context destruction failed.";
    }

    // Check for quit message
    Event myEvent;
    while (mOSWindow->popEvent(&myEvent))
    {
        if (myEvent.Type == Event::EVENT_CLOSED)
        {
            exit(0);
        }
    }
}

void ANGLETest::swapBuffers()
{
    if (mEGLWindow->isGLInitialized())
    {
        mEGLWindow->swap();
    }
}

void ANGLETest::setupQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale)
{
    if (mQuadVertexBuffer == 0)
    {
        glGenBuffers(1, &mQuadVertexBuffer);
    }

    auto quadVertices = GetQuadVertices();
    for (Vector3 &vertex : quadVertices)
    {
        vertex.x *= positionAttribXYScale;
        vertex.y *= positionAttribXYScale;
        vertex.z = positionAttribZ;
    }

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 6, quadVertices.data(), GL_STATIC_DRAW);
}

void ANGLETest::setupIndexedQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale)
{
    if (mQuadVertexBuffer == 0)
    {
        glGenBuffers(1, &mQuadVertexBuffer);
    }

    auto quadVertices = angle::GetIndexedQuadVertices();
    for (Vector3 &vertex : quadVertices)
    {
        vertex.x *= positionAttribXYScale;
        vertex.y *= positionAttribXYScale;
        vertex.z = positionAttribZ;
    }

    glBindBuffer(GL_ARRAY_BUFFER, mQuadVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 4, quadVertices.data(), GL_STATIC_DRAW);
}

// static
void ANGLETest::drawQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ)
{
    drawQuad(program, positionAttribName, positionAttribZ, 1.0f);
}

// static
void ANGLETest::drawQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale)
{
    drawQuad(program, positionAttribName, positionAttribZ, positionAttribXYScale, false);
}

void ANGLETest::drawQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale,
                         bool useVertexBuffer)
{
    GLint previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    if (previousProgram != static_cast<GLint>(program))
    {
        glUseProgram(program);
    }

    GLint positionLocation = glGetAttribLocation(program, positionAttribName.c_str());

    if (useVertexBuffer)
    {
        setupQuadVertexBuffer(positionAttribZ, positionAttribXYScale);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        auto quadVertices = GetQuadVertices();
        for (Vector3 &vertex : quadVertices)
        {
            vertex.x *= positionAttribXYScale;
            vertex.y *= positionAttribXYScale;
            vertex.z = positionAttribZ;
        }

        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
    }
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    if (previousProgram != static_cast<GLint>(program))
    {
        glUseProgram(previousProgram);
    }
}

void ANGLETest::drawIndexedQuad(GLuint program,
                                const std::string &positionAttribName,
                                GLfloat positionAttribZ)
{
    drawIndexedQuad(program, positionAttribName, positionAttribZ, 1.0f);
}

void ANGLETest::drawIndexedQuad(GLuint program,
                                const std::string &positionAttribName,
                                GLfloat positionAttribZ,
                                GLfloat positionAttribXYScale)
{
    GLint positionLocation = glGetAttribLocation(program, positionAttribName.c_str());

    GLint activeProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(program);
    }

    GLuint prevBinding = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint *>(&prevBinding));

    setupIndexedQuadVertexBuffer(positionAttribZ, positionAttribXYScale);

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);
    glBindBuffer(GL_ARRAY_BUFFER, prevBinding);

    const GLushort indices[] = {
        0, 1, 2, 0, 2, 3,
    };

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    if (static_cast<GLuint>(activeProgram) != program)
    {
        glUseProgram(static_cast<GLuint>(activeProgram));
    }
}

GLuint ANGLETest::compileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);

    const char *sourceArray[1] = { source.c_str() };
    glShaderSource(shader, 1, sourceArray, NULL);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    if (compileResult == 0)
    {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength == 0)
        {
            std::cerr << "shader compilation failed with empty log." << std::endl;
        }
        else
        {
            std::vector<GLchar> infoLog(infoLogLength);
            glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), NULL, &infoLog[0]);

            std::cerr << "shader compilation failed: " << &infoLog[0];
        }

        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

void ANGLETest::checkD3D11SDKLayersMessages()
{
#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(NDEBUG)
    // In debug D3D11 mode, check ID3D11InfoQueue to see if any D3D11 SDK Layers messages
    // were outputted by the test
    if (mIgnoreD3D11SDKLayersWarnings ||
        mEGLWindow->getPlatform().renderer != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
        mEGLWindow->getDisplay() == EGL_NO_DISPLAY)
    {
        return;
    }

    const char *extensionString =
        static_cast<const char *>(eglQueryString(mEGLWindow->getDisplay(), EGL_EXTENSIONS));
    if (!strstr(extensionString, "EGL_EXT_device_query"))
    {
        return;
    }

    EGLAttrib device      = 0;
    EGLAttrib angleDevice = 0;

    PFNEGLQUERYDISPLAYATTRIBEXTPROC queryDisplayAttribEXT;
    PFNEGLQUERYDEVICEATTRIBEXTPROC queryDeviceAttribEXT;

    queryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDisplayAttribEXT"));
    queryDeviceAttribEXT = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDeviceAttribEXT"));
    ASSERT_NE(nullptr, queryDisplayAttribEXT);
    ASSERT_NE(nullptr, queryDeviceAttribEXT);

    ASSERT_EGL_TRUE(queryDisplayAttribEXT(mEGLWindow->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
    ASSERT_EGL_TRUE(queryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
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

            FAIL() << numStoredD3DDebugMessages
                   << " D3D11 SDK Layers message(s) detected! Test Failed.\n";
        }
    }

    SafeRelease(infoQueue);
#endif
}

static bool checkExtensionExists(const char *allExtensions, const std::string &extName)
{
    return strstr(allExtensions, extName.c_str()) != nullptr;
}

bool ANGLETest::extensionEnabled(const std::string &extName)
{
    return checkExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                extName);
}

bool ANGLETest::eglDisplayExtensionEnabled(EGLDisplay display, const std::string &extName)
{
    return checkExtensionExists(eglQueryString(display, EGL_EXTENSIONS), extName);
}

bool ANGLETest::eglClientExtensionEnabled(const std::string &extName)
{
    return checkExtensionExists(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS), extName);
}

void ANGLETest::setWindowWidth(int width)
{
    mWidth = width;
}

void ANGLETest::setWindowHeight(int height)
{
    mHeight = height;
}

void ANGLETest::setConfigRedBits(int bits)
{
    mEGLWindow->setConfigRedBits(bits);
}

void ANGLETest::setConfigGreenBits(int bits)
{
    mEGLWindow->setConfigGreenBits(bits);
}

void ANGLETest::setConfigBlueBits(int bits)
{
    mEGLWindow->setConfigBlueBits(bits);
}

void ANGLETest::setConfigAlphaBits(int bits)
{
    mEGLWindow->setConfigAlphaBits(bits);
}

void ANGLETest::setConfigDepthBits(int bits)
{
    mEGLWindow->setConfigDepthBits(bits);
}

void ANGLETest::setConfigStencilBits(int bits)
{
    mEGLWindow->setConfigStencilBits(bits);
}

void ANGLETest::setMultisampleEnabled(bool enabled)
{
    mEGLWindow->setMultisample(enabled);
}

void ANGLETest::setDebugEnabled(bool enabled)
{
    mEGLWindow->setDebugEnabled(enabled);
}

void ANGLETest::setNoErrorEnabled(bool enabled)
{
    mEGLWindow->setNoErrorEnabled(enabled);
}

void ANGLETest::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mEGLWindow->setWebGLCompatibilityEnabled(webglCompatibility);
}

void ANGLETest::setBindGeneratesResource(bool bindGeneratesResource)
{
    mEGLWindow->setBindGeneratesResource(bindGeneratesResource);
}

int ANGLETest::getClientMajorVersion() const
{
    return mEGLWindow->getClientMajorVersion();
}

int ANGLETest::getClientMinorVersion() const
{
    return mEGLWindow->getClientMinorVersion();
}

EGLWindow *ANGLETest::getEGLWindow() const
{
    return mEGLWindow;
}

int ANGLETest::getWindowWidth() const
{
    return mWidth;
}

int ANGLETest::getWindowHeight() const
{
    return mHeight;
}

bool ANGLETest::isMultisampleEnabled() const
{
    return mEGLWindow->isMultisample();
}

bool ANGLETest::createEGLContext()
{
    return mEGLWindow->initializeGL(mOSWindow);
}

bool ANGLETest::destroyEGLContext()
{
    mEGLWindow->destroyGL();
    return true;
}

bool ANGLETest::InitTestWindow()
{
    mOSWindow = CreateOSWindow();
    if (!mOSWindow->initialize("ANGLE_TEST", 128, 128))
    {
        return false;
    }

    mOSWindow->setVisible(true);

    return true;
}

bool ANGLETest::DestroyTestWindow()
{
    if (mOSWindow)
    {
        mOSWindow->destroy();
        delete mOSWindow;
        mOSWindow = NULL;
    }

    return true;
}

void ANGLETest::SetWindowVisible(bool isVisible)
{
    mOSWindow->setVisible(isVisible);
}

bool IsIntel()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Intel") != std::string::npos);
}

bool IsAdreno()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Adreno") != std::string::npos);
}

bool IsAMD()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("AMD") != std::string::npos) ||
           (rendererString.find("ATI") != std::string::npos);
}

bool IsNVIDIA()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("NVIDIA") != std::string::npos);
}

bool IsD3D11()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D11 vs_5_0") != std::string::npos);
}

bool IsD3D11_FL93()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D11 vs_4_0_") != std::string::npos);
}

bool IsD3D9()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D9") != std::string::npos);
}

bool IsD3DSM3()
{
    return IsD3D9() || IsD3D11_FL93();
}

bool IsDesktopOpenGL()
{
    return IsOpenGL() && !IsOpenGLES();
}

bool IsOpenGLES()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("OpenGL ES") != std::string::npos);
}

bool IsOpenGL()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("OpenGL") != std::string::npos);
}

bool IsAndroid()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return true;
#else
    return false;
#endif
}

bool IsLinux()
{
#if defined(ANGLE_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

bool IsOSX()
{
#if defined(ANGLE_PLATFORM_APPLE)
    return true;
#else
    return false;
#endif
}

bool IsWindows()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

bool IsDebug()
{
#if !defined(NDEBUG)
    return true;
#else
    return false;
#endif
}

bool IsRelease()
{
    return !IsDebug();
}

EGLint ANGLETest::getPlatformRenderer() const
{
    assert(mEGLWindow);
    return mEGLWindow->getPlatform().renderer;
}

void ANGLETest::ignoreD3D11SDKLayersWarnings()
{
    // Some tests may need to disable the D3D11 SDK Layers Warnings checks
    mIgnoreD3D11SDKLayersWarnings = true;
}

OSWindow *ANGLETest::mOSWindow = NULL;

void ANGLETestEnvironment::SetUp()
{
    mGLESLibrary.reset(angle::loadLibrary("libGLESv2"));
    if (mGLESLibrary)
    {
        auto initFunc = reinterpret_cast<ANGLEPlatformInitializeFunc>(
            mGLESLibrary->getSymbol("ANGLEPlatformInitialize"));
        if (initFunc)
        {
            initFunc(&angle::g_testPlatformInstance);
        }
    }

    if (!ANGLETest::InitTestWindow())
    {
        FAIL() << "Failed to create ANGLE test window.";
    }
}

void ANGLETestEnvironment::TearDown()
{
    ANGLETest::DestroyTestWindow();

    if (mGLESLibrary)
    {
        auto shutdownFunc = reinterpret_cast<ANGLEPlatformShutdownFunc>(
            mGLESLibrary->getSymbol("ANGLEPlatformShutdown"));
        if (shutdownFunc)
        {
            shutdownFunc();
        }
    }
}

void IgnoreANGLEPlatformMessages()
{
    // Negative tests may trigger expected errors/warnings in the ANGLE Platform.
    angle::g_testPlatformInstance.ignoreMessages();
}

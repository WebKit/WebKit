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

void TestPlatform_logError(angle::PlatformMethods *platform, const char *errorMessage)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->ignoreMessages)
        return;

    FAIL() << errorMessage;
}

void TestPlatform_logWarning(angle::PlatformMethods *platform, const char *warningMessage)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->ignoreMessages)
        return;

    std::cerr << "Warning: " << warningMessage << std::endl;
}

void TestPlatform_logInfo(angle::PlatformMethods *platform, const char *infoMessage)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->ignoreMessages)
        return;

    angle::WriteDebugMessage("%s\n", infoMessage);
}

void TestPlatform_overrideWorkaroundsD3D(angle::PlatformMethods *platform,
                                         WorkaroundsD3D *workaroundsD3D)
{
    auto *testPlatformContext = static_cast<TestPlatformContext *>(platform->context);
    if (testPlatformContext->currentTest)
    {
        testPlatformContext->currentTest->overrideWorkaroundsD3D(workaroundsD3D);
    }
}

std::array<angle::Vector3, 4> GetIndexedQuadVertices()
{
    std::array<angle::Vector3, 4> vertices;
    vertices[0] = angle::Vector3(-1.0f, 1.0f, 0.5f);
    vertices[1] = angle::Vector3(-1.0f, -1.0f, 0.5f);
    vertices[2] = angle::Vector3(1.0f, -1.0f, 0.5f);
    vertices[3] = angle::Vector3(1.0f, 1.0f, 0.5f);
    return vertices;
}

static constexpr std::array<GLushort, 6> IndexedQuadIndices = {{0, 1, 2, 0, 2, 3}};

}  // anonymous namespace

GLColorRGB::GLColorRGB() : R(0), G(0), B(0)
{
}

GLColorRGB::GLColorRGB(GLubyte r, GLubyte g, GLubyte b) : R(r), G(g), B(b)
{
}

GLColorRGB::GLColorRGB(const angle::Vector3 &floatColor)
    : R(ColorDenorm(floatColor.x())), G(ColorDenorm(floatColor.y())), B(ColorDenorm(floatColor.z()))
{
}

GLColor::GLColor() : R(0), G(0), B(0), A(0)
{
}

GLColor::GLColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a) : R(r), G(g), B(b), A(a)
{
}

GLColor::GLColor(const angle::Vector4 &floatColor)
    : R(ColorDenorm(floatColor.x())),
      G(ColorDenorm(floatColor.y())),
      B(ColorDenorm(floatColor.z())),
      A(ColorDenorm(floatColor.w()))
{
}

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

angle::Vector4 GLColor::toNormalizedVector() const
{
    return angle::Vector4(ColorNorm(R), ColorNorm(G), ColorNorm(B), ColorNorm(A));
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

// static
std::array<angle::Vector3, 6> ANGLETestBase::GetQuadVertices()
{
    std::array<angle::Vector3, 6> vertices;
    vertices[0] = angle::Vector3(-1.0f, 1.0f, 0.5f);
    vertices[1] = angle::Vector3(-1.0f, -1.0f, 0.5f);
    vertices[2] = angle::Vector3(1.0f, -1.0f, 0.5f);
    vertices[3] = angle::Vector3(-1.0f, 1.0f, 0.5f);
    vertices[4] = angle::Vector3(1.0f, -1.0f, 0.5f);
    vertices[5] = angle::Vector3(1.0f, 1.0f, 0.5f);
    return vertices;
}

// static
std::array<GLushort, 6> ANGLETestBase::GetQuadIndices()
{
    return angle::IndexedQuadIndices;
}

ANGLETestBase::ANGLETestBase(const angle::PlatformParameters &params)
    : mEGLWindow(nullptr),
      mWidth(16),
      mHeight(16),
      mIgnoreD3D11SDKLayersWarnings(false),
      mQuadVertexBuffer(0),
      mQuadIndexBuffer(0),
      m2DTexturedQuadProgram(0),
      mDeferContextInit(false)
{
    mEGLWindow = new EGLWindow(params.majorVersion, params.minorVersion, params.eglParameters);

    // Default debug layers to enabled in tests.
    mEGLWindow->setDebugLayersEnabled(true);

    // Workaround for NVIDIA not being able to share OpenGL and Vulkan contexts.
    EGLint renderer      = params.getRenderer();
    bool needsWindowSwap = mLastRendererType.valid() &&
                           ((renderer != EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE) !=
                            (mLastRendererType.value() != EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE));

    if (needsWindowSwap)
    {
        DestroyTestWindow();
        if (!InitTestWindow())
        {
            std::cerr << "Failed to create ANGLE test window.";
        }
    }
    mLastRendererType = renderer;
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
    SafeDelete(mEGLWindow);
}

void ANGLETestBase::ANGLETestSetUp()
{
    mPlatformContext.ignoreMessages = false;
    mPlatformContext.currentTest    = this;

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

    mPlatformMethods.overrideWorkaroundsD3D = angle::TestPlatform_overrideWorkaroundsD3D;
    mPlatformMethods.logError               = angle::TestPlatform_logError;
    mPlatformMethods.logWarning             = angle::TestPlatform_logWarning;
    mPlatformMethods.logInfo                = angle::TestPlatform_logInfo;
    mPlatformMethods.context                = &mPlatformContext;
    mEGLWindow->setPlatformMethods(&mPlatformMethods);

    if (!mEGLWindow->initializeDisplayAndSurface(mOSWindow))
    {
        FAIL() << "egl display or surface init failed.";
    }

    if (!mDeferContextInit && !mEGLWindow->initializeContext())
    {
        FAIL() << "GL Context init failed.";
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

void ANGLETestBase::ANGLETestTearDown()
{
    mEGLWindow->setPlatformMethods(nullptr);

    mPlatformContext.currentTest = nullptr;
    checkD3D11SDKLayersMessages();

    const auto &info = testing::UnitTest::GetInstance()->current_test_info();
    angle::WriteDebugMessage("Exiting %s.%s\n", info->test_case_name(), info->name());

    swapBuffers();

    if (eglGetError() != EGL_SUCCESS)
    {
        FAIL() << "egl error during swap.";
    }

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

void ANGLETestBase::swapBuffers()
{
    if (mEGLWindow->isGLInitialized())
    {
        mEGLWindow->swap();
    }
}

void ANGLETestBase::setupQuadVertexBuffer(GLfloat positionAttribZ, GLfloat positionAttribXYScale)
{
    if (mQuadVertexBuffer == 0)
    {
        glGenBuffers(1, &mQuadVertexBuffer);
    }

    auto quadVertices = GetQuadVertices();
    for (angle::Vector3 &vertex : quadVertices)
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

    auto quadVertices = angle::GetIndexedQuadVertices();
    for (angle::Vector3 &vertex : quadVertices)
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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(angle::IndexedQuadIndices),
                 angle::IndexedQuadIndices.data(), GL_STATIC_DRAW);
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

    if (useVertexBuffer)
    {
        setupQuadVertexBuffer(positionAttribZ, positionAttribXYScale);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
    }
    else
    {
        auto quadVertices = GetQuadVertices();
        for (angle::Vector3 &vertex : quadVertices)
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
        indices = angle::IndexedQuadIndices.data();
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

    const std::string &vs =
        "attribute vec2 position;\n"
        "varying mediump vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    texCoord = position * 0.5 + vec2(0.5);\n"
        "}\n";

    const std::string &fs =
        "varying mediump vec2 texCoord;\n"
        "uniform sampler2D tex;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = texture2D(tex, texCoord);\n"
        "}\n";

    m2DTexturedQuadProgram = CompileProgram(vs, fs);
    return m2DTexturedQuadProgram;
}

void ANGLETestBase::draw2DTexturedQuad(GLfloat positionAttribZ,
                                       GLfloat positionAttribXYScale,
                                       bool useVertexBuffer)
{
    ASSERT_NE(0u, get2DTexturedQuadProgram());
    return drawQuad(get2DTexturedQuadProgram(), "position", positionAttribZ, positionAttribXYScale,
                    useVertexBuffer);
}

GLuint ANGLETestBase::compileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);

    const char *sourceArray[1] = { source.c_str() };
    glShaderSource(shader, 1, sourceArray, nullptr);
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
            glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, &infoLog[0]);

            std::cerr << "shader compilation failed: " << &infoLog[0] << std::endl;
        }

        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

void ANGLETestBase::checkD3D11SDKLayersMessages()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    // On Windows D3D11, check ID3D11InfoQueue to see if any D3D11 SDK Layers messages
    // were outputted by the test. We enable the Debug layers in Release tests as well.
    if (mIgnoreD3D11SDKLayersWarnings ||
        mEGLWindow->getPlatform().renderer != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
        mEGLWindow->getDisplay() == EGL_NO_DISPLAY)
    {
        return;
    }

    const char *extensionString =
        static_cast<const char *>(eglQueryString(mEGLWindow->getDisplay(), EGL_EXTENSIONS));
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
#endif  // defined(ANGLE_PLATFORM_WINDOWS)
}

bool ANGLETestBase::extensionEnabled(const std::string &extName)
{
    return CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                extName);
}

bool ANGLETestBase::extensionRequestable(const std::string &extName)
{
    return CheckExtensionExists(
        reinterpret_cast<const char *>(glGetString(GL_REQUESTABLE_EXTENSIONS_ANGLE)), extName);
}

bool ANGLETestBase::eglDisplayExtensionEnabled(EGLDisplay display, const std::string &extName)
{
    return CheckExtensionExists(eglQueryString(display, EGL_EXTENSIONS), extName);
}

bool ANGLETestBase::eglClientExtensionEnabled(const std::string &extName)
{
    return EGLWindow::ClientExtensionEnabled(extName);
}

bool ANGLETestBase::eglDeviceExtensionEnabled(EGLDeviceEXT device, const std::string &extName)
{
    PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT =
        reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(
            eglGetProcAddress("eglQueryDeviceStringEXT"));
    return CheckExtensionExists(eglQueryDeviceStringEXT(device, EGL_EXTENSIONS), extName);
}

void ANGLETestBase::setWindowWidth(int width)
{
    mWidth = width;
}

void ANGLETestBase::setWindowHeight(int height)
{
    mHeight = height;
}

void ANGLETestBase::setConfigRedBits(int bits)
{
    mEGLWindow->setConfigRedBits(bits);
}

void ANGLETestBase::setConfigGreenBits(int bits)
{
    mEGLWindow->setConfigGreenBits(bits);
}

void ANGLETestBase::setConfigBlueBits(int bits)
{
    mEGLWindow->setConfigBlueBits(bits);
}

void ANGLETestBase::setConfigAlphaBits(int bits)
{
    mEGLWindow->setConfigAlphaBits(bits);
}

void ANGLETestBase::setConfigDepthBits(int bits)
{
    mEGLWindow->setConfigDepthBits(bits);
}

void ANGLETestBase::setConfigStencilBits(int bits)
{
    mEGLWindow->setConfigStencilBits(bits);
}

void ANGLETestBase::setConfigComponentType(EGLenum componentType)
{
    mEGLWindow->setConfigComponentType(componentType);
}

void ANGLETestBase::setMultisampleEnabled(bool enabled)
{
    mEGLWindow->setMultisample(enabled);
}

void ANGLETestBase::setSamples(EGLint samples)
{
    mEGLWindow->setSamples(samples);
}

void ANGLETestBase::setDebugEnabled(bool enabled)
{
    mEGLWindow->setDebugEnabled(enabled);
}

void ANGLETestBase::setNoErrorEnabled(bool enabled)
{
    mEGLWindow->setNoErrorEnabled(enabled);
}

void ANGLETestBase::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mEGLWindow->setWebGLCompatibilityEnabled(webglCompatibility);
}

void ANGLETestBase::setRobustAccess(bool enabled)
{
    mEGLWindow->setRobustAccess(enabled);
}

void ANGLETestBase::setBindGeneratesResource(bool bindGeneratesResource)
{
    mEGLWindow->setBindGeneratesResource(bindGeneratesResource);
}

void ANGLETestBase::setDebugLayersEnabled(bool enabled)
{
    mEGLWindow->setDebugLayersEnabled(enabled);
}

void ANGLETestBase::setClientArraysEnabled(bool enabled)
{
    mEGLWindow->setClientArraysEnabled(enabled);
}

void ANGLETestBase::setRobustResourceInit(bool enabled)
{
    mEGLWindow->setRobustResourceInit(enabled);
}

void ANGLETestBase::setContextProgramCacheEnabled(bool enabled)
{
    mEGLWindow->setContextProgramCacheEnabled(enabled);
}

void ANGLETestBase::setDeferContextInit(bool enabled)
{
    mDeferContextInit = enabled;
}

int ANGLETestBase::getClientMajorVersion() const
{
    return mEGLWindow->getClientMajorVersion();
}

int ANGLETestBase::getClientMinorVersion() const
{
    return mEGLWindow->getClientMinorVersion();
}

EGLWindow *ANGLETestBase::getEGLWindow() const
{
    return mEGLWindow;
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
    return mEGLWindow->isMultisample();
}

bool ANGLETestBase::destroyEGLContext()
{
    mEGLWindow->destroyGL();
    return true;
}

// static
bool ANGLETestBase::InitTestWindow()
{
    mOSWindow = CreateOSWindow();
    if (!mOSWindow->initialize("ANGLE_TEST", 128, 128))
    {
        return false;
    }

    mOSWindow->setVisible(true);

    mGLESLibrary.reset(angle::loadLibrary("libGLESv2"));

    return true;
}

// static
bool ANGLETestBase::DestroyTestWindow()
{
    if (mOSWindow)
    {
        mOSWindow->destroy();
        delete mOSWindow;
        mOSWindow = nullptr;
    }

    mGLESLibrary.reset(nullptr);

    return true;
}

void ANGLETestBase::SetWindowVisible(bool isVisible)
{
    mOSWindow->setVisible(isVisible);
}

ANGLETest::ANGLETest() : ANGLETestBase(GetParam())
{
}

void ANGLETest::SetUp()
{
    ANGLETestBase::ANGLETestSetUp();
}

void ANGLETest::TearDown()
{
    ANGLETestBase::ANGLETestTearDown();
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

bool IsNULL()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("NULL") != std::string::npos);
}

bool IsAndroid()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return true;
#else
    return false;
#endif
}

bool IsVulkan()
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Vulkan") != std::string::npos);
}

bool IsOzone()
{
#if defined(USE_OZONE)
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

EGLint ANGLETestBase::getPlatformRenderer() const
{
    assert(mEGLWindow);
    return mEGLWindow->getPlatform().renderer;
}

void ANGLETestBase::ignoreD3D11SDKLayersWarnings()
{
    // Some tests may need to disable the D3D11 SDK Layers Warnings checks
    mIgnoreD3D11SDKLayersWarnings = true;
}

ANGLETestBase::ScopedIgnorePlatformMessages::ScopedIgnorePlatformMessages(ANGLETestBase *test)
    : mTest(test)
{
    mTest->mPlatformContext.ignoreMessages = true;
}

ANGLETestBase::ScopedIgnorePlatformMessages::~ScopedIgnorePlatformMessages()
{
    mTest->mPlatformContext.ignoreMessages = false;
}

OSWindow *ANGLETestBase::mOSWindow = nullptr;
Optional<EGLint> ANGLETestBase::mLastRendererType;
std::unique_ptr<angle::Library> ANGLETestBase::mGLESLibrary;

void ANGLETestEnvironment::SetUp()
{
    if (!ANGLETestBase::InitTestWindow())
    {
        FAIL() << "Failed to create ANGLE test window.";
    }
}

void ANGLETestEnvironment::TearDown()
{
    ANGLETestBase::DestroyTestWindow();
}

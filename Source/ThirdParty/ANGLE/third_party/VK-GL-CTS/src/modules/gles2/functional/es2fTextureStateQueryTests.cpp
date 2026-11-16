/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Texture State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es2fTextureStateQueryTests.hpp"
#include "es2fApiCase.hpp"
#include "glsStateQueryUtil.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"
#include "deMath.h"

using namespace glw; // GLint and other GL types
using namespace deqp::gls;
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace TextureParamVerifiers
{

// TexParamVerifier

class TexParamVerifier : protected glu::CallLogWrapper
{
public:
    TexParamVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix);
    virtual ~TexParamVerifier(); // make GCC happy

    const char *getTestNamePostfix(void) const;

    virtual void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference) = 0;
    virtual void verifyFloat(tcu::TestContext &testCtx, GLenum target, GLenum name, GLfloat reference) = 0;

private:
    const char *const m_testNamePostfix;
};

TexParamVerifier::TexParamVerifier(const glw::Functions &gl, tcu::TestLog &log, const char *testNamePostfix)
    : glu::CallLogWrapper(gl, log)
    , m_testNamePostfix(testNamePostfix)
{
    enableLogging(true);
}
TexParamVerifier::~TexParamVerifier()
{
}

const char *TexParamVerifier::getTestNamePostfix(void) const
{
    return m_testNamePostfix;
}

class GetTexParameterIVerifier : public TexParamVerifier
{
public:
    GetTexParameterIVerifier(const glw::Functions &gl, tcu::TestLog &log);

    void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference);
    void verifyFloat(tcu::TestContext &testCtx, GLenum target, GLenum name, GLfloat reference);
};

GetTexParameterIVerifier::GetTexParameterIVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : TexParamVerifier(gl, log, "_gettexparameteri")
{
}

void GetTexParameterIVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLint> state;
    glGetTexParameteriv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid texture param value");
    }
}

void GetTexParameterIVerifier::verifyFloat(tcu::TestContext &testCtx, GLenum target, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    const GLint expectedGLStateMax = StateQueryUtil::roundGLfloatToNearestIntegerHalfUp<GLint>(reference);
    const GLint expectedGLStateMin = StateQueryUtil::roundGLfloatToNearestIntegerHalfDown<GLint>(reference);

    StateQueryMemoryWriteGuard<GLint> state;
    glGetTexParameteriv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state < expectedGLStateMin || state > expectedGLStateMax)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected in range [" << expectedGLStateMin << ", "
                         << expectedGLStateMax << "]; got " << state << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid texture param value");
    }
}

class GetTexParameterFVerifier : public TexParamVerifier
{
public:
    GetTexParameterFVerifier(const glw::Functions &gl, tcu::TestLog &log);

    void verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference);
    void verifyFloat(tcu::TestContext &testCtx, GLenum target, GLenum name, GLfloat reference);
};

GetTexParameterFVerifier::GetTexParameterFVerifier(const glw::Functions &gl, tcu::TestLog &log)
    : TexParamVerifier(gl, log, "_gettexparameterf")
{
}

void GetTexParameterFVerifier::verifyInteger(tcu::TestContext &testCtx, GLenum target, GLenum name, GLint reference)
{
    DE_ASSERT(
        reference ==
        GLint(GLfloat(
            reference))); // reference integer must have 1:1 mapping to float for this to work. Reference value is always such value in these tests

    using tcu::TestLog;

    const GLfloat referenceAsFloat = GLfloat(reference);

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetTexParameterfv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != referenceAsFloat)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << referenceAsFloat << "; got " << state
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

void GetTexParameterFVerifier::verifyFloat(tcu::TestContext &testCtx, GLenum target, GLenum name, GLfloat reference)
{
    using tcu::TestLog;

    StateQueryMemoryWriteGuard<GLfloat> state;
    glGetTexParameterfv(target, name, &state);

    if (!state.verifyValidity(testCtx))
        return;

    if (state != reference)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: expected " << reference << "; got " << state
                         << TestLog::EndMessage;

        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid float value");
    }
}

} // namespace TextureParamVerifiers

namespace
{

using namespace TextureParamVerifiers;

// Tests

class TextureCase : public ApiCase
{
public:
    TextureCase(Context &context, TexParamVerifier *verifier, const char *name, const char *description,
                GLenum textureTarget)
        : ApiCase(context, name, description)
        , m_textureTarget(textureTarget)
        , m_verifier(verifier)
    {
    }

    virtual void testTexture(void) = 0;

    void test(void)
    {
        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(m_textureTarget, textureId);
        expectError(GL_NO_ERROR);

        testTexture();

        glDeleteTextures(1, &textureId);
        expectError(GL_NO_ERROR);
    }

protected:
    GLenum m_textureTarget;
    TexParamVerifier *m_verifier;
};

class TextureWrapCase : public TextureCase
{
public:
    TextureWrapCase(Context &context, TexParamVerifier *verifier, const char *name, const char *description,
                    GLenum textureTarget, GLenum valueName)
        : TextureCase(context, verifier, name, description, textureTarget)
        , m_valueName(valueName)
    {
    }

    void testTexture(void)
    {
        const GLenum wrapValues[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};

        m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, GL_REPEAT);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
        {
            glTexParameteri(m_textureTarget, m_valueName, wrapValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, wrapValues[ndx]);
            expectError(GL_NO_ERROR);
        }

        //check unit conversions with float

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(wrapValues); ++ndx)
        {
            glTexParameterf(m_textureTarget, m_valueName, (GLfloat)wrapValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, m_valueName, wrapValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }

private:
    GLenum m_valueName;
};

class TextureMagFilterCase : public TextureCase
{
public:
    TextureMagFilterCase(Context &context, TexParamVerifier *verifier, const char *name, const char *description,
                         GLenum textureTarget)
        : TextureCase(context, verifier, name, description, textureTarget)
    {
    }

    void testTexture(void)
    {
        const GLenum magValues[] = {GL_NEAREST, GL_LINEAR};

        m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
        {
            glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
            expectError(GL_NO_ERROR);
        }

        //check unit conversions with float

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(magValues); ++ndx)
        {
            glTexParameterf(m_textureTarget, GL_TEXTURE_MAG_FILTER, (GLfloat)magValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MAG_FILTER, magValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }
};

class TextureMinFilterCase : public TextureCase
{
public:
    TextureMinFilterCase(Context &context, TexParamVerifier *verifier, const char *name, const char *description,
                         GLenum textureTarget)
        : TextureCase(context, verifier, name, description, textureTarget)
    {
    }

    void testTexture(void)
    {
        const GLenum minValues[] = {GL_NEAREST,
                                    GL_LINEAR,
                                    GL_NEAREST_MIPMAP_NEAREST,
                                    GL_NEAREST_MIPMAP_LINEAR,
                                    GL_LINEAR_MIPMAP_NEAREST,
                                    GL_LINEAR_MIPMAP_LINEAR};

        m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        expectError(GL_NO_ERROR);

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
        {
            glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
            expectError(GL_NO_ERROR);
        }

        //check unit conversions with float

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(minValues); ++ndx)
        {
            glTexParameterf(m_textureTarget, GL_TEXTURE_MIN_FILTER, (GLfloat)minValues[ndx]);
            expectError(GL_NO_ERROR);

            m_verifier->verifyInteger(m_testCtx, m_textureTarget, GL_TEXTURE_MIN_FILTER, minValues[ndx]);
            expectError(GL_NO_ERROR);
        }
    }
};

} // namespace

#define FOR_EACH_VERIFIER(VERIFIERS, CODE_BLOCK)                                                 \
    do                                                                                           \
    {                                                                                            \
        for (int _verifierNdx = 0; _verifierNdx < DE_LENGTH_OF_ARRAY(VERIFIERS); _verifierNdx++) \
        {                                                                                        \
            TexParamVerifier *verifier = (VERIFIERS)[_verifierNdx];                              \
            CODE_BLOCK;                                                                          \
        }                                                                                        \
    } while (0)

TextureStateQueryTests::TextureStateQueryTests(Context &context)
    : TestCaseGroup(context, "texture", "Texture State Query tests")
    , m_verifierInt(nullptr)
    , m_verifierFloat(nullptr)
{
}

TextureStateQueryTests::~TextureStateQueryTests(void)
{
    deinit();
}

void TextureStateQueryTests::init(void)
{
    using namespace TextureParamVerifiers;

    DE_ASSERT(m_verifierInt == nullptr);
    DE_ASSERT(m_verifierFloat == nullptr);

    m_verifierInt =
        new GetTexParameterIVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());
    m_verifierFloat =
        new GetTexParameterFVerifier(m_context.getRenderContext().getFunctions(), m_context.getTestContext().getLog());

    TexParamVerifier *verifiers[] = {m_verifierInt, m_verifierFloat};

    const struct
    {
        const char *name;
        GLenum textureTarget;
    } textureTargets[] = {{"texture_2d", GL_TEXTURE_2D}, {"texture_cube_map", GL_TEXTURE_CUBE_MAP}};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(textureTargets); ++ndx)
    {
        FOR_EACH_VERIFIER(
            verifiers,
            addChild(new TextureWrapCase(
                m_context, verifier,
                (std::string(textureTargets[ndx].name) + "_texture_wrap_s" + verifier->getTestNamePostfix()).c_str(),
                "TEXTURE_WRAP_S", textureTargets[ndx].textureTarget, GL_TEXTURE_WRAP_S)));
        FOR_EACH_VERIFIER(
            verifiers,
            addChild(new TextureWrapCase(
                m_context, verifier,
                (std::string(textureTargets[ndx].name) + "_texture_wrap_t" + verifier->getTestNamePostfix()).c_str(),
                "TEXTURE_WRAP_T", textureTargets[ndx].textureTarget, GL_TEXTURE_WRAP_T)));

        FOR_EACH_VERIFIER(verifiers,
                          addChild(new TextureMagFilterCase(m_context, verifier,
                                                            (std::string(textureTargets[ndx].name) +
                                                             "_texture_mag_filter" + verifier->getTestNamePostfix())
                                                                .c_str(),
                                                            "TEXTURE_MAG_FILTER", textureTargets[ndx].textureTarget)));
        FOR_EACH_VERIFIER(verifiers,
                          addChild(new TextureMinFilterCase(m_context, verifier,
                                                            (std::string(textureTargets[ndx].name) +
                                                             "_texture_min_filter" + verifier->getTestNamePostfix())
                                                                .c_str(),
                                                            "TEXTURE_MIN_FILTER", textureTargets[ndx].textureTarget)));
    }
}

void TextureStateQueryTests::deinit(void)
{
    if (m_verifierInt)
    {
        delete m_verifierInt;
        m_verifierInt = NULL;
    }
    if (m_verifierFloat)
    {
        delete m_verifierFloat;
        m_verifierFloat = NULL;
    }

    this->TestCaseGroup::deinit();
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

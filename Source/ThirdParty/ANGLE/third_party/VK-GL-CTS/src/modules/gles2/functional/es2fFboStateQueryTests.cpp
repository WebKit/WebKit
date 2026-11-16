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
 * \brief Fbo state query tests.
 *//*--------------------------------------------------------------------*/

#include "es2fFboStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "es2fApiCase.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "deMath.h"

using namespace glw; // GLint and other GL types
using deqp::gls::StateQueryUtil::StateQueryMemoryWriteGuard;

namespace deqp
{
namespace gles2
{
namespace Functional
{
namespace
{

void checkIntEquals(tcu::TestContext &testCtx, GLint got, GLint expected)
{
    using tcu::TestLog;

    if (got != expected)
    {
        testCtx.getLog() << TestLog::Message << "// ERROR: Expected " << expected << "; got " << got
                         << TestLog::EndMessage;
        if (testCtx.getTestResult() == QP_TEST_RESULT_PASS)
            testCtx.setTestResult(QP_TEST_RESULT_FAIL, "got invalid value");
    }
}

void checkAttachmentParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target, GLenum attachment,
                          GLenum pname, GLenum reference)
{
    StateQueryMemoryWriteGuard<GLint> state;
    gl.glGetFramebufferAttachmentParameteriv(target, attachment, pname, &state);

    if (state.verifyValidity(testCtx))
        checkIntEquals(testCtx, state, reference);
}

void checkColorAttachmentParam(tcu::TestContext &testCtx, glu::CallLogWrapper &gl, GLenum target, GLenum pname,
                               GLenum reference)
{
    checkAttachmentParam(testCtx, gl, target, GL_COLOR_ATTACHMENT0, pname, reference);
}

class AttachmentObjectCase : public ApiCase
{
public:
    AttachmentObjectCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // texture
        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                      GL_TEXTURE);
            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                      textureID);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        // rb
        {
            GLuint renderbufferID = 0;
            glGenRenderbuffers(1, &renderbufferID);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 128, 128);
            expectError(GL_NO_ERROR);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbufferID);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                      GL_RENDERBUFFER);
            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                      renderbufferID);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &renderbufferID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureLevelCase : public ApiCase
{
public:
    AttachmentTextureLevelCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        // GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL can only be 0
        {
            GLuint textureID = 0;

            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            expectError(GL_NO_ERROR);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);
            expectError(GL_NO_ERROR);

            checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, 0);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

class AttachmentTextureCubeMapFaceCase : public ApiCase
{
public:
    AttachmentTextureCubeMapFaceCase(Context &context, const char *name, const char *description)
        : ApiCase(context, name, description)
    {
    }

    void test(void)
    {
        GLuint framebufferID = 0;
        glGenFramebuffers(1, &framebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
        expectError(GL_NO_ERROR);

        {
            GLuint textureID = 0;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
            expectError(GL_NO_ERROR);

            const GLenum faces[] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(faces); ++ndx)
                glTexImage2D(faces[ndx], 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            expectError(GL_NO_ERROR);

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(faces); ++ndx)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[ndx], textureID, 0);
                checkColorAttachmentParam(m_testCtx, *this, GL_FRAMEBUFFER,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, faces[ndx]);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glDeleteTextures(1, &textureID);
        }

        glDeleteFramebuffers(1, &framebufferID);
        expectError(GL_NO_ERROR);
    }
};

} // namespace

FboStateQueryTests::FboStateQueryTests(Context &context) : TestCaseGroup(context, "fbo", "Fbo State Query tests")
{
}

void FboStateQueryTests::init(void)
{
    addChild(new AttachmentObjectCase(m_context, "framebuffer_attachment_object",
                                      "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE and FRAMEBUFFER_ATTACHMENT_OBJECT_NAME"));
    addChild(new AttachmentTextureLevelCase(m_context, "framebuffer_attachment_texture_level",
                                            "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL"));
    addChild(new AttachmentTextureCubeMapFaceCase(m_context, "framebuffer_attachment_texture_cube_map_face",
                                                  "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE"));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

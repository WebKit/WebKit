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
 * \brief Framebuffer Object API Tests.
 *
 * Notes:
 *   All gl calls are passed thru sgl2::Context class. Reasons:
 *    + Name, object allocation is tracked and live resources are freed
 *      when Context is destroyed.
 *    + Makes it possible to easily log all relevant calls into test log.
 *      \todo [pyry] This is not implemented yet
 *//*--------------------------------------------------------------------*/

#include "es2fFboApiTest.hpp"
#include "sglrGLContext.hpp"
#include "gluDefs.hpp"
#include "gluContextInfo.hpp"
#include "gluStrUtil.hpp"
#include "tcuRenderTarget.hpp"
#include "deString.h"
#include "glwFunctions.hpp"
#include "glsFboUtil.hpp"
#include "glwEnums.hpp"

#include <iterator>
#include <algorithm>

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;

using glw::GLenum;
using glw::GLint;

static void logComment(tcu::TestContext &testCtx, const char *comment)
{
    testCtx.getLog() << TestLog::Message << "// " << comment << TestLog::EndMessage;
}

static void checkError(tcu::TestContext &testCtx, sglr::Context &ctx, GLenum expect)
{
    GLenum result = ctx.getError();
    testCtx.getLog() << TestLog::Message << "// " << (result == expect ? "Pass" : "Fail") << ", expected "
                     << glu::getErrorStr(expect) << TestLog::EndMessage;

    if (result != expect)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Error code mismatch");
}

static void checkEitherError(tcu::TestContext &testCtx, sglr::Context &ctx, GLenum expectA, GLenum expectB)
{
    GLenum result = ctx.getError();
    bool isOk     = (result == expectA || result == expectB);

    testCtx.getLog() << TestLog::Message << "// " << (isOk ? "Pass" : "Fail") << ", expected "
                     << glu::getErrorStr(expectA) << " or " << glu::getErrorStr(expectB) << TestLog::EndMessage;

    if (!isOk)
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Error code mismatch");
}

static const char *getAttachmentName(GLenum attachment)
{
    switch (attachment)
    {
    case GL_COLOR_ATTACHMENT0:
        return "GL_COLOR_ATTACHMENT0";
    case GL_DEPTH_ATTACHMENT:
        return "GL_DEPTH_ATTACHMENT";
    case GL_STENCIL_ATTACHMENT:
        return "GL_STENCIL_ATTACHMENT";
    default:
        throw tcu::InternalError("Unknown attachment", "", __FILE__, __LINE__);
    }
}

static const char *getAttachmentParameterName(GLenum pname)
{
    switch (pname)
    {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE";
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME";
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL";
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE";
    default:
        throw tcu::InternalError("Unknown parameter", "", __FILE__, __LINE__);
    }
}

static string getAttachmentParameterValueName(GLint value)
{
    switch (value)
    {
    case 0:
        return "GL_NONE(0)";
    case GL_TEXTURE:
        return "GL_TEXTURE";
    case GL_RENDERBUFFER:
        return "GL_RENDERBUFFER";
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
    default:
    {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "0x%x", value);
        return string(tmp);
    }
    }
}

static void checkFboAttachmentParam(tcu::TestContext &testCtx, sglr::Context &ctx, GLenum attachment, GLenum pname,
                                    GLint expectedValue)
{
    TestLog &log = testCtx.getLog();
    log << TestLog::Message << "// Querying " << getAttachmentName(attachment) << " "
        << getAttachmentParameterName(pname) << TestLog::EndMessage;

    GLint value = 0xcdcdcdcd;
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, pname, &value);

    GLenum err = ctx.getError();

    if (value == expectedValue && err == GL_NO_ERROR)
        log << TestLog::Message << "// Pass" << TestLog::EndMessage;
    else
    {
        log << TestLog::Message << "// Fail, expected " << getAttachmentParameterValueName(expectedValue)
            << " without error" << TestLog::EndMessage;
        testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid result for attachment param query");
    }
}

static void notSupportedTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    DE_UNREF(testCtx);
    DE_UNREF(context);
    throw tcu::NotSupportedError("Not supported", "", __FILE__, __LINE__);
}

static void textureLevelsTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    uint32_t tex = 1;
    uint32_t fbo = 1;

    context.bindTexture(GL_TEXTURE_2D, tex);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256);
    context.texImage2D(GL_TEXTURE_2D, 1, GL_RGB, 128, 128);

    context.bindFramebuffer(GL_FRAMEBUFFER, fbo);

    static int levels[] = {2, 1, 0, -1, 0x7fffffff, 0, 1};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(levels); ndx++)
    {
        context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, levels[ndx]);
        checkError(testCtx, context, levels[ndx] == 0 ? GL_NO_ERROR : GL_INVALID_VALUE);
    }
}

static void textureLevelsWithRenderToMipmapTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    uint32_t tex = 1;
    uint32_t fbo = 1;

    context.bindTexture(GL_TEXTURE_2D, tex);
    context.texImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256);
    context.texImage2D(GL_TEXTURE_2D, 1, GL_RGB, 128, 128);

    context.bindFramebuffer(GL_FRAMEBUFFER, fbo);

    static int levels[] = {2, 1, 0, -1, 0x7fffffff, 0, 1};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(levels); ndx++)
    {
        context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, levels[ndx]);
        checkError(testCtx, context, de::inBounds(levels[ndx], 0, 16) ? GL_NO_ERROR : GL_INVALID_VALUE);
    }
}

static void validTex2DAttachmentsTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    context.bindFramebuffer(GL_FRAMEBUFFER, 1);
    static const GLenum attachmentPoints[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

    // Texture2D
    uint32_t tex2D = 1;
    context.bindTexture(GL_TEXTURE_2D, tex2D);
    for (int pointNdx = 0; pointNdx < DE_LENGTH_OF_ARRAY(attachmentPoints); pointNdx++)
    {
        context.framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoints[pointNdx], GL_TEXTURE_2D, tex2D, 0);
        checkError(testCtx, context, GL_NO_ERROR);
    }
}

static void validTexCubeAttachmentsTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    static const GLenum attachmentPoints[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    static const GLenum cubeTargets[]      = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                                              GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                                              GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    // TextureCube
    uint32_t texCube = 2;
    context.bindTexture(GL_TEXTURE_CUBE_MAP, texCube);
    for (int pointNdx = 0; pointNdx < DE_LENGTH_OF_ARRAY(attachmentPoints); pointNdx++)
    {
        for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(cubeTargets); targetNdx++)
        {
            context.framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoints[pointNdx], cubeTargets[targetNdx], texCube,
                                         0);
            checkError(testCtx, context, GL_NO_ERROR);
        }
    }
}

static void validRboAttachmentsTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    static const GLenum attachmentPoints[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    // Renderbuffer
    uint32_t rbo = 3;
    context.bindRenderbuffer(GL_RENDERBUFFER, rbo);
    for (int pointNdx = 0; pointNdx < DE_LENGTH_OF_ARRAY(attachmentPoints); pointNdx++)
    {
        context.framebufferRenderbuffer(GL_FRAMEBUFFER, attachmentPoints[pointNdx], GL_RENDERBUFFER, rbo);
        checkError(testCtx, context, GL_NO_ERROR);
    }
}

static void attachToDefaultFramebufferTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    logComment(testCtx, "Attaching 2D texture to default framebuffer");

    uint32_t tex2D = 1;
    context.bindTexture(GL_TEXTURE_2D, tex2D);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);

    logComment(testCtx, "Attaching renderbuffer to default framebuffer");

    uint32_t rbo = 1;
    context.bindRenderbuffer(GL_RENDERBUFFER, rbo);
    context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    checkError(testCtx, context, GL_INVALID_OPERATION);
}

static void invalidTex2DAttachmentTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    logComment(testCtx, "Attaching 2D texture using GL_TEXTURE_CUBE_MAP_NEGATIVE_X texture target");

    uint32_t tex2D = 1;
    context.bindTexture(GL_TEXTURE_2D, tex2D);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, tex2D, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);

    logComment(testCtx, "Attaching deleted 2D texture object");
    context.deleteTextures(1, &tex2D);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);
}

static void invalidTexCubeAttachmentTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    logComment(testCtx, "Attaching cube texture using GL_TEXTURE_2D texture target");
    uint32_t texCube = 2;
    context.bindTexture(GL_TEXTURE_CUBE_MAP, texCube);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texCube, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);

    logComment(testCtx, "Attaching deleted cube texture object");
    context.deleteTextures(1, &texCube);
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texCube, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);
}

static void invalidRboAttachmentTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    logComment(testCtx, "Attaching renderbuffer using GL_FRAMEBUFFER renderbuffer target");
    uint32_t rbo = 3;
    context.bindRenderbuffer(GL_RENDERBUFFER, rbo);
    context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER, rbo);
    checkError(testCtx, context, GL_INVALID_ENUM);

    logComment(testCtx, "Attaching deleted renderbuffer object");
    context.deleteRenderbuffers(1, &rbo);
    context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    checkError(testCtx, context, GL_INVALID_OPERATION);
}

static void attachNamesTest(tcu::TestContext &testCtx, sglr::Context &context)
{
    context.bindFramebuffer(GL_FRAMEBUFFER, 1);

    // Just allocate some names, don't bind for storage
    uint32_t reservedTexName;
    context.genTextures(1, &reservedTexName);

    logComment(testCtx, "Attaching allocated texture name to 2D target");
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reservedTexName, 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);

    logComment(testCtx, "Attaching allocated texture name to cube target");
    context.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, reservedTexName,
                                 0);
    checkError(testCtx, context, GL_INVALID_OPERATION);

    uint32_t reservedRboName;
    context.genRenderbuffers(1, &reservedRboName);

    logComment(testCtx, "Attaching allocated renderbuffer name");
    context.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, reservedRboName);
    checkError(testCtx, context, GL_INVALID_OPERATION);
}

static void attachmentQueryDefaultFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    // Check that proper error codes are returned
    GLint unused = 1;
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                            &unused);
    checkEitherError(testCtx, ctx, GL_INVALID_ENUM, GL_INVALID_OPERATION);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                            &unused);
    checkEitherError(testCtx, ctx, GL_INVALID_ENUM, GL_INVALID_OPERATION);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &unused);
    checkEitherError(testCtx, ctx, GL_INVALID_ENUM, GL_INVALID_OPERATION);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &unused);
    checkEitherError(testCtx, ctx, GL_INVALID_ENUM, GL_INVALID_OPERATION);
}

static void attachmentQueryEmptyFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    static const GLenum attachmentPoints[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(attachmentPoints); ndx++)
        checkFboAttachmentParam(testCtx, ctx, attachmentPoints[ndx], GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);

    // Check that proper error codes are returned
    GLint unused = -1;
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                            &unused);
    checkError(testCtx, ctx, GL_INVALID_ENUM);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &unused);
    checkError(testCtx, ctx, GL_INVALID_ENUM);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &unused);
    checkError(testCtx, ctx, GL_INVALID_ENUM);
}

static void es3AttachmentQueryEmptyFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    // Error codes changed for ES3 so this version of test should be
    // used when ES2 context is created on ES3 capable hardwere

    static const GLenum attachmentPoints[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(attachmentPoints); ndx++)
        checkFboAttachmentParam(testCtx, ctx, attachmentPoints[ndx], GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 0);

    // Check that proper error codes are returned
    GLint unused = -1;
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &unused);
    checkError(testCtx, ctx, GL_INVALID_OPERATION);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &unused);
    checkError(testCtx, ctx, GL_INVALID_OPERATION);
}

static void attachmentQueryTex2DTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    ctx.bindTexture(GL_TEXTURE_2D, 1);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 1, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 1);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, 0);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, 0);
}

static void attachmentQueryTexCubeTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    ctx.bindTexture(GL_TEXTURE_CUBE_MAP, 2);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 2, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 2);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, 0);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
                            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
}

static void attachmentQueryRboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    ctx.bindRenderbuffer(GL_RENDERBUFFER, 3);
    ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 3);

    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                            GL_RENDERBUFFER);
    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 3);

    GLint unused = 0;
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &unused);
    checkError(testCtx, ctx, GL_INVALID_ENUM);
    ctx.getFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                            GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &unused);
    checkError(testCtx, ctx, GL_INVALID_ENUM);
}

static void deleteTex2DAttachedToBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t tex2D = 1;
    ctx.bindTexture(GL_TEXTURE_2D, tex2D);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, tex2D);

    ctx.deleteTextures(1, &tex2D);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);
}

static void deleteTexCubeAttachedToBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t texCube = 1;
    ctx.bindTexture(GL_TEXTURE_CUBE_MAP, texCube);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texCube, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, texCube);

    ctx.deleteTextures(1, &texCube);

    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);
}

static void deleteRboAttachedToBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t rbo = 1;
    ctx.bindRenderbuffer(GL_RENDERBUFFER, rbo);
    ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                            GL_RENDERBUFFER);
    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, rbo);

    ctx.deleteRenderbuffers(1, &rbo);

    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_NONE);
}

static void deleteTex2DAttachedToNotBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t tex2D = 1;
    ctx.bindTexture(GL_TEXTURE_2D, tex2D);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, tex2D);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

    ctx.deleteTextures(1, &tex2D);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, tex2D);
}

static void deleteTexCubeAttachedToNotBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t texCube = 1;
    ctx.bindTexture(GL_TEXTURE_CUBE_MAP, texCube);
    ctx.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texCube, 0);

    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, texCube);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

    ctx.deleteTextures(1, &texCube);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, GL_TEXTURE);
    checkFboAttachmentParam(testCtx, ctx, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, texCube);
}

static void deleteRboAttachedToNotBoundFboTest(tcu::TestContext &testCtx, sglr::Context &ctx)
{
    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    uint32_t rbo = 1;
    ctx.bindRenderbuffer(GL_RENDERBUFFER, rbo);
    ctx.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                            GL_RENDERBUFFER);
    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, rbo);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 0);

    ctx.deleteRenderbuffers(1, &rbo);

    ctx.bindFramebuffer(GL_FRAMEBUFFER, 1);

    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                            GL_RENDERBUFFER);
    checkFboAttachmentParam(testCtx, ctx, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, rbo);
}

class FboApiCase : public TestCase
{
public:
    typedef void (*TestFunc)(tcu::TestContext &testCtx, sglr::Context &context);

    FboApiCase(Context &context, const char *name, const char *description, TestFunc test);
    virtual ~FboApiCase(void);

    virtual IterateResult iterate(void);

private:
    FboApiCase(const FboApiCase &other);
    FboApiCase &operator=(const FboApiCase &other);

    TestFunc m_testFunc;
};

FboApiCase::FboApiCase(Context &context, const char *name, const char *description, TestFunc test)
    : TestCase(context, name, description)
    , m_testFunc(test)
{
}

FboApiCase::~FboApiCase(void)
{
}

TestCase::IterateResult FboApiCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    GLU_EXPECT_NO_ERROR(gl.getError(), "Before test case");

    // Initialize result to PASS
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    // Execute test case
    {
        sglr::GLContext context(
            m_context.getRenderContext(), m_testCtx.getLog(), sglr::GLCONTEXT_LOG_CALLS,
            tcu::IVec4(0, 0, m_context.getRenderTarget().getWidth(), m_context.getRenderTarget().getHeight()));
        m_testFunc(m_testCtx, context);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "After test case");

    return STOP;
}

FboApiTestGroup::FboApiTestGroup(Context &context) : TestCaseGroup(context, "api", "API Tests")
{
}

FboApiTestGroup::~FboApiTestGroup(void)
{
}

void FboApiTestGroup::init(void)
{
    std::set<std::string> extensions;
    std::copy(m_context.getContextInfo().getExtensions().begin(), m_context.getContextInfo().getExtensions().end(),
              std::inserter(extensions, extensions.begin()));

    const glu::RenderContext &renderContext = m_context.getRenderContext();
    bool defaultFboIsZero                   = renderContext.getDefaultFramebuffer() == 0;
    bool isES3Compatible   = gls::FboUtil::checkExtensionSupport(renderContext, "DEQP_gles3_core_compatible");
    bool hasRenderToMipmap = isES3Compatible || extensions.find("GL_OES_fbo_render_mipmap") != extensions.end();

    // Valid attachments
    addChild(new FboApiCase(m_context, "valid_tex2d_attachments", "Valid 2D texture attachments",
                            validTex2DAttachmentsTest));
    addChild(new FboApiCase(m_context, "valid_texcube_attachments", "Valid cubemap attachments",
                            validTexCubeAttachmentsTest));
    addChild(
        new FboApiCase(m_context, "valid_rbo_attachments", "Valid renderbuffer attachments", validRboAttachmentsTest));

    // Invalid attachments
    addChild(new FboApiCase(m_context, "attach_to_default_fbo", "Invalid usage: attaching to default FBO",
                            defaultFboIsZero ? attachToDefaultFramebufferTest : notSupportedTest));
    addChild(new FboApiCase(m_context, "invalid_tex2d_attachments", "Invalid 2D texture attachments",
                            invalidTex2DAttachmentTest));
    addChild(new FboApiCase(m_context, "invalid_texcube_attachments", "Invalid cubemap attachments",
                            invalidTexCubeAttachmentTest));
    addChild(new FboApiCase(m_context, "invalid_rbo_attachments", "Invalid renderbuffer attachments",
                            invalidRboAttachmentTest));
    addChild(new FboApiCase(m_context, "attach_names", "Attach allocated names without objects", attachNamesTest));

    addChild(new FboApiCase(m_context, "texture_levels", "Valid and invalid texturel levels",
                            hasRenderToMipmap ? textureLevelsWithRenderToMipmapTest : textureLevelsTest));

    // Attachment queries
    addChild(new FboApiCase(m_context, "attachment_query_default_fbo", "Query attachments from default FBO",
                            defaultFboIsZero ? attachmentQueryDefaultFboTest : notSupportedTest));
    addChild(new FboApiCase(m_context, "attachment_query_empty_fbo", "Query attachments from empty FBO",
                            isES3Compatible ? es3AttachmentQueryEmptyFboTest : attachmentQueryEmptyFboTest));
    addChild(new FboApiCase(m_context, "attachment_query_tex2d", "Query 2d texture attachment properties",
                            attachmentQueryTex2DTest));
    addChild(new FboApiCase(m_context, "attachment_query_texcube", "Query cubemap attachment properties",
                            attachmentQueryTexCubeTest));
    addChild(new FboApiCase(m_context, "attachment_query_rbo", "Query renderbuffer attachment properties",
                            attachmentQueryRboTest));

    // Delete attachments
    addChild(new FboApiCase(m_context, "delete_tex_2d_attached_to_bound_fbo",
                            "Delete 2d texture attached to currently bound FBO", deleteTex2DAttachedToBoundFboTest));
    addChild(new FboApiCase(m_context, "delete_tex_cube_attached_to_bound_fbo",
                            "Delete cubemap attached to currently bound FBO", deleteTexCubeAttachedToBoundFboTest));
    addChild(new FboApiCase(m_context, "delete_rbo_attached_to_bound_fbo",
                            "Delete renderbuffer attached to currently bound FBO", deleteRboAttachedToBoundFboTest));

    addChild(new FboApiCase(m_context, "delete_tex_2d_attached_to_not_bound_fbo", "Delete 2d texture attached to FBO",
                            deleteTex2DAttachedToNotBoundFboTest));
    addChild(new FboApiCase(m_context, "delete_tex_cube_attached_to_not_bound_fbo", "Delete cubemap attached to FBO",
                            deleteTexCubeAttachedToNotBoundFboTest));
    addChild(new FboApiCase(m_context, "delete_rbo_attached_to_not_bound_fbo", "Delete renderbuffer attached to FBO",
                            deleteRboAttachedToNotBoundFboTest));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

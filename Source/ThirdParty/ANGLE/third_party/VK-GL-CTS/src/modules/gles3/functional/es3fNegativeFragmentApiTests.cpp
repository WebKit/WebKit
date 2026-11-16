/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Negative Fragment Pipe API tests.
 *//*--------------------------------------------------------------------*/

#include "es3fNegativeFragmentApiTests.hpp"
#include "es3fApiCase.hpp"

#include "gluContextInfo.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles3
{
namespace Functional
{

using tcu::TestLog;

static bool checkDrawBuffersIndexedSupport(Context &ctx)
{
    return contextSupports(ctx.getRenderContext().getType(), glu::ApiType::es(3, 2)) ||
           contextSupports(ctx.getRenderContext().getType(), glu::ApiType::core(4, 5)) ||
           ctx.getContextInfo().isExtensionSupported("GL_EXT_draw_buffers_indexed");
}

NegativeFragmentApiTests::NegativeFragmentApiTests(Context &context)
    : TestCaseGroup(context, "fragment", "Negative Fragment API Cases")
{
}

NegativeFragmentApiTests::~NegativeFragmentApiTests(void)
{
}

void NegativeFragmentApiTests::init(void)
{
    ES3F_ADD_API_CASE(scissor, "Invalid glScissor() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if either width or height is negative.");
        glScissor(0, 0, -1, 0);
        expectError(GL_INVALID_VALUE);
        glScissor(0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glScissor(0, 0, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(depth_func, "Invalid glDepthFunc() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if func is not an accepted value.");
        glDepthFunc(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(viewport, "Invalid glViewport() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if either width or height is negative.");
        glViewport(0, 0, -1, 1);
        expectError(GL_INVALID_VALUE);
        glViewport(0, 0, 1, -1);
        expectError(GL_INVALID_VALUE);
        glViewport(0, 0, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // Stencil functions

    ES3F_ADD_API_CASE(stencil_func, "Invalid glStencilFunc() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if func is not one of the eight accepted values.");
        glStencilFunc(-1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(stencil_func_separate, "Invalid glStencilFuncSeparate() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if face is not GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.");
        glStencilFuncSeparate(-1, GL_NEVER, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if func is not one of the eight accepted values.");
        glStencilFuncSeparate(GL_FRONT, -1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(stencil_op, "Invalid glStencilOp() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if sfail, dpfail, or dppass is any value other "
                                      "than the defined symbolic constant values.");
        glStencilOp(-1, GL_ZERO, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOp(GL_KEEP, -1, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOp(GL_KEEP, GL_ZERO, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(stencil_op_separate, "Invalid glStencilOpSeparate() usage", {
        m_log << TestLog::Section(
            "",
            "GL_INVALID_ENUM is generated if face is any value other than GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.");
        glStencilOpSeparate(-1, GL_KEEP, GL_ZERO, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if sfail, dpfail, or dppass is any value other "
                                      "than the eight defined symbolic constant values.");
        glStencilOpSeparate(GL_FRONT, -1, GL_ZERO, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, -1, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_ZERO, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(stencil_mask_separate, "Invalid glStencilMaskSeparate() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if face is not GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.");
        glStencilMaskSeparate(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Blend functions

    ES3F_ADD_API_CASE(blend_equation, "Invalid glBlendEquation() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquation(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_equation_separate, "Invalid glBlendEquationSeparate() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if modeRGB is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquationSeparate(-1, GL_FUNC_ADD);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if modeAlpha is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquationSeparate(GL_FUNC_ADD, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_func, "Invalid glBlendFunc() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if either sfactor or dfactor is not an accepted value.");
        glBlendFunc(-1, GL_ONE);
        expectError(GL_INVALID_ENUM);
        glBlendFunc(GL_ONE, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_func_separate, "Invalid glBlendFuncSeparate() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if srcRGB, dstRGB, srcAlpha, or dstAlpha is not an accepted value.");
        glBlendFuncSeparate(-1, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparate(GL_ZERO, -1, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparate(GL_ZERO, GL_ONE, -1, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_SRC_COLOR, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_equationi, "Invalid glBlendEquationi() usage", {
        glw::GLint maxDrawBuffers = -1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (!checkDrawBuffersIndexedSupport(m_context))
            throw tcu::NotSupportedError("GL_EXT_draw_buffers_indexed is not supported", nullptr, __FILE__, __LINE__);
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquationi(0, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if buf is not in the range zero to the value of "
                                      "MAX_DRAW_BUFFERS minus one.");
        glBlendEquationi(-1, GL_FUNC_ADD);
        expectError(GL_INVALID_VALUE);
        glBlendEquationi(maxDrawBuffers, GL_FUNC_ADD);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_equation_separatei, "Invalid glBlendEquationSeparatei() usage", {
        glw::GLint maxDrawBuffers = -1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (!checkDrawBuffersIndexedSupport(m_context))
            throw tcu::NotSupportedError("GL_EXT_draw_buffers_indexed is not supported", nullptr, __FILE__, __LINE__);
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if modeRGB is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquationSeparatei(0, -1, GL_FUNC_ADD);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if modeAlpha is not GL_FUNC_ADD, GL_FUNC_SUBTRACT, "
                                      "GL_FUNC_REVERSE_SUBTRACT, GL_MAX or GL_MIN.");
        glBlendEquationSeparatei(0, GL_FUNC_ADD, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if buf is not in the range zero to the value of "
                                      "MAX_DRAW_BUFFERS minus one.");
        glBlendEquationSeparatei(-1, GL_FUNC_ADD, GL_FUNC_ADD);
        expectError(GL_INVALID_VALUE);
        glBlendEquationSeparatei(maxDrawBuffers, GL_FUNC_ADD, GL_FUNC_ADD);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_funci, "Invalid glBlendFunci() usage", {
        glw::GLint maxDrawBuffers = -1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (!checkDrawBuffersIndexedSupport(m_context))
            throw tcu::NotSupportedError("GL_EXT_draw_buffers_indexed is not supported", nullptr, __FILE__, __LINE__);
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if either sfactor or dfactor is not an accepted value.");
        glBlendFunci(0, -1, GL_ONE);
        expectError(GL_INVALID_ENUM);
        glBlendFunci(0, GL_ONE, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if buf is not in the range zero to the value of "
                                      "MAX_DRAW_BUFFERS minus one.");
        glBlendFunci(-1, GL_ONE, GL_ONE);
        expectError(GL_INVALID_VALUE);
        glBlendFunci(maxDrawBuffers, GL_ONE, GL_ONE);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(blend_func_separatei, "Invalid glBlendFuncSeparatei() usage", {
        glw::GLint maxDrawBuffers = -1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (!checkDrawBuffersIndexedSupport(m_context))
            throw tcu::NotSupportedError("GL_EXT_draw_buffers_indexed is not supported", nullptr, __FILE__, __LINE__);
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if srcRGB, dstRGB, srcAlpha, or dstAlpha is not an accepted value.");
        glBlendFuncSeparatei(0, -1, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparatei(0, GL_ZERO, -1, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparatei(0, GL_ZERO, GL_ONE, -1, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_ENUM);
        glBlendFuncSeparatei(0, GL_ZERO, GL_ONE, GL_SRC_COLOR, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if buf is not in the range zero to the value of "
                                      "MAX_DRAW_BUFFERS minus one.");
        glBlendFuncSeparatei(-1, GL_ONE, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_VALUE);
        glBlendFuncSeparatei(maxDrawBuffers, GL_ONE, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // Rasterization API functions

    ES3F_ADD_API_CASE(cull_face, "Invalid glCullFace() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glCullFace(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(front_face, "Invalid glFrontFace() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glFrontFace(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(line_width, "Invalid glLineWidth() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width is less than or equal to 0.");
        glLineWidth(0);
        expectError(GL_INVALID_VALUE);
        glLineWidth(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // Asynchronous queries

    ES3F_ADD_API_CASE(gen_queries, "Invalid glGenQueries() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        GLuint ids;
        glGenQueries(-1, &ids);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(begin_query, "Invalid glBeginQuery() usage", {
        GLuint ids[3];
        glGenQueries(3, ids);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glBeginQuery(-1, ids[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if glBeginQuery is executed while a query "
                                      "object of the same target is already active.");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ids[0]);
        expectError(GL_NO_ERROR);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ids[1]);
        expectError(GL_INVALID_OPERATION);
        // \note GL_ANY_SAMPLES_PASSED and GL_ANY_SAMPLES_PASSED_CONSERVATIVE alias to the same target for the purposes of this error.
        glBeginQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, ids[1]);
        expectError(GL_INVALID_OPERATION);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, ids[1]);
        expectError(GL_NO_ERROR);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, ids[2]);
        expectError(GL_INVALID_OPERATION);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if id is 0.");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if id not a name returned from a previous call to "
                                  "glGenQueries, or if such a name has since been deleted with glDeleteQueries.");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, -1);
        expectError(GL_INVALID_OPERATION);
        glDeleteQueries(1, &ids[2]);
        expectError(GL_NO_ERROR);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ids[2]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if id is the name of an already active query object.");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ids[0]);
        expectError(GL_NO_ERROR);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, ids[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if id refers to an existing query object "
                                      "whose type does not does not match target.");
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        expectError(GL_NO_ERROR);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, ids[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteQueries(2, &ids[0]);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(end_query, "Invalid glEndQuery() usage", {
        GLuint id;
        glGenQueries(1, &id);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glEndQuery(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if glEndQuery is executed when a query object "
                                      "of the same target is not active.");
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        expectError(GL_INVALID_OPERATION);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, id);
        expectError(GL_NO_ERROR);
        glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        expectError(GL_INVALID_OPERATION);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;

        glDeleteQueries(1, &id);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(delete_queries, "Invalid glDeleteQueries() usage", {
        GLuint id;
        glGenQueries(1, &id);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteQueries(-1, &id);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteQueries(1, &id);
    });

    // Sync objects

    ES3F_ADD_API_CASE(fence_sync, "Invalid glFenceSync() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if condition is not GL_SYNC_GPU_COMMANDS_COMPLETE.");
        glFenceSync(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if flags is not zero.");
        glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0x0010);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(wait_sync, "Invalid glWaitSync() usage", {
        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if sync is not the name of a sync object.");
        glWaitSync(0, 0, GL_TIMEOUT_IGNORED);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if flags is not zero.");
        glWaitSync(sync, 0x0010, GL_TIMEOUT_IGNORED);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if timeout is not GL_TIMEOUT_IGNORED.");
        glWaitSync(sync, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteSync(sync);
    });
    ES3F_ADD_API_CASE(client_wait_sync, "Invalid glClientWaitSync() usage", {
        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if sync is not the name of an existing sync object.");
        glClientWaitSync(0, 0, 10000);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if flags contains any unsupported flag.");
        glClientWaitSync(sync, 0x00000004, 10000);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteSync(sync);
    });
    ES3F_ADD_API_CASE(delete_sync, "Invalid glDeleteSync() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if sync is neither zero or the name of a sync object.");
        glDeleteSync((GLsync)1);
        expectError(GL_INVALID_VALUE);
        glDeleteSync(0);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp

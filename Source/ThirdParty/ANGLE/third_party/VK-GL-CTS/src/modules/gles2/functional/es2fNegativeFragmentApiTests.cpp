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
 * \brief Negative Fragment Pipe API tests.
 *//*--------------------------------------------------------------------*/

#include "es2fNegativeFragmentApiTests.hpp"
#include "es2fApiCase.hpp"

#include "glwEnums.hpp"
#include "glwDefs.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

using tcu::TestLog;

NegativeFragmentApiTests::NegativeFragmentApiTests(Context &context)
    : TestCaseGroup(context, "fragment", "Negative Fragment API Cases")
{
}

NegativeFragmentApiTests::~NegativeFragmentApiTests(void)
{
}

void NegativeFragmentApiTests::init(void)
{
    ES2F_ADD_API_CASE(scissor, "Invalid glScissor() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if either width or height is negative.");
        glScissor(0, 0, -1, 0);
        expectError(GL_INVALID_VALUE);
        glScissor(0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glScissor(0, 0, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(depth_func, "Invalid glDepthFunc() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if func is not an accepted value.");
        glDepthFunc(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(viewport, "Invalid glViewport() usage", {
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

    ES2F_ADD_API_CASE(stencil_func, "Invalid glStencilFunc() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if func is not one of the eight accepted values.");
        glStencilFunc(-1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(stencil_func_separate, "Invalid glStencilFuncSeparate() usage", {
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
    ES2F_ADD_API_CASE(stencil_op, "Invalid glStencilOp() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if sfail, dpfail, or dppass is any value other "
                                      "than the eight defined symbolic constant values.");
        glStencilOp(-1, GL_ZERO, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOp(GL_KEEP, -1, GL_REPLACE);
        expectError(GL_INVALID_ENUM);
        glStencilOp(GL_KEEP, GL_ZERO, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(stencil_op_separate, "Invalid glStencilOpSeparate() usage", {
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
    ES2F_ADD_API_CASE(stencil_mask_separate, "Invalid glStencilMaskSeparate() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if face is not GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.");
        glStencilMaskSeparate(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Blend functions

    ES2F_ADD_API_CASE(blend_equation, "Invalid glBlendEquation() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not one of GL_FUNC_ADD, "
                                      "GL_FUNC_SUBTRACT, or GL_FUNC_REVERSE_SUBTRACT.");
        glBlendEquation(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(blend_equation_separate, "Invalid glBlendEquationSeparate() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if either modeRGB or modeAlpha is not one of "
                                      "GL_FUNC_ADD, GL_FUNC_SUBTRACT, or GL_FUNC_REVERSE_SUBTRACT.");
        glBlendEquationSeparate(-1, GL_FUNC_ADD);
        expectError(GL_INVALID_ENUM);
        glBlendEquationSeparate(GL_FUNC_ADD, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(blend_func_separate, "Invalid glBlendFuncSeparate() usage", {
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
    ES2F_ADD_API_CASE(blend_func, "Invalid glBlendFunc() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if either sfactor or dfactor is not an accepted value.");
        glBlendFunc(-1, GL_ONE);
        expectError(GL_INVALID_ENUM);
        glBlendFunc(GL_ONE, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Rasterization API functions

    ES2F_ADD_API_CASE(cull_face, "Invalid glCullFace() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glCullFace(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    ES2F_ADD_API_CASE(front_face, "Invalid glFrontFace() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glFrontFace(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    ES2F_ADD_API_CASE(line_width, "Invalid glLineWidth() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width is less than or equal to 0.");
        glLineWidth(0);
        expectError(GL_INVALID_VALUE);
        glLineWidth(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

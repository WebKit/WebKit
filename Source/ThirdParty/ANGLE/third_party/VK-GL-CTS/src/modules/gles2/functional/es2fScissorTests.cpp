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
 * \brief GLES2 Scissor tests.
 *//*--------------------------------------------------------------------*/

#include "es2fScissorTests.hpp"

#include "glsScissorTests.hpp"

#include "tcuVector.hpp"

#include "glwEnums.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

ScissorTests::ScissorTests(Context &context) : TestCaseGroup(context, "scissor", "Scissor Tests")
{
}

ScissorTests::~ScissorTests(void)
{
}

void ScissorTests::init(void)
{
    using tcu::Vec4;
    using namespace gls::Functional::ScissorTestInternal;

    tcu::TestContext &tc   = m_context.getTestContext();
    glu::RenderContext &rc = m_context.getRenderContext();

    const struct
    {
        const char *name;
        const char *desc;
        const tcu::Vec4 scissor;
        const tcu::Vec4 render;
        PrimitiveType type;
        const int primitives;
    } cases[] = {{"contained_tris", "Triangles fully inside scissor area (single call)", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 30},
                 {"partial_tris", "Triangles partially inside scissor area (single call)", Vec4(0.3f, 0.3f, 0.4f, 0.4f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 30},
                 {"contained_tri", "Triangle fully inside scissor area", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 1},
                 {"enclosing_tri", "Triangle fully covering scissor area", Vec4(0.4f, 0.4f, 0.2f, 0.2f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 1},
                 {"partial_tri", "Triangle partially inside scissor area", Vec4(0.4f, 0.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 1.0f, 1.0f), TRIANGLE, 1},
                 {"outside_render_tri", "Triangle with scissor area outside render target",
                  Vec4(1.4f, 1.4f, 0.6f, 0.6f), Vec4(0.0f, 0.0f, 0.6f, 0.6f), TRIANGLE, 1},
                 {"partial_lines", "Linse partially inside scissor area", Vec4(0.4f, 0.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 1.0f, 1.0f), LINE, 30},
                 {"contained_line", "Line fully inside scissor area", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), LINE, 1},
                 {"partial_line", "Line partially inside scissor area", Vec4(0.4f, 0.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 1.0f, 1.0f), LINE, 1},
                 {"outside_render_line", "Line with scissor area outside render target", Vec4(1.4f, 1.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 0.6f, 0.6f), LINE, 1},
                 {"contained_point", "Point fully inside scissor area", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                  Vec4(0.5f, 0.5f, 0.0f, 0.0f), POINT, 1},
                 {"partial_points", "Points partially inside scissor area", Vec4(0.4f, 0.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 1.0f, 1.0f), POINT, 30},
                 {"outside_point", "Point fully outside scissor area", Vec4(0.4f, 0.4f, 0.6f, 0.6f),
                  Vec4(0.0f, 0.0f, 0.0f, 0.0f), POINT, 1},
                 {"outside_render_point", "Point with scissor area outside render target", Vec4(1.4f, 1.4f, 0.6f, 0.6f),
                  Vec4(0.5f, 0.5f, 0.0f, 0.0f), POINT, 1}};

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
    {
        addChild(createPrimitiveTest(tc, rc, cases[caseNdx].name, cases[caseNdx].desc, cases[caseNdx].scissor,
                                     cases[caseNdx].render, cases[caseNdx].type, cases[caseNdx].primitives));
    }

    addChild(createClearTest(tc, rc, "clear_depth", "Depth buffer clear", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                             GL_DEPTH_BUFFER_BIT));
    addChild(createClearTest(tc, rc, "clear_stencil", "Stencil buffer clear", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                             GL_STENCIL_BUFFER_BIT));
    addChild(createClearTest(tc, rc, "clear_color", "Color buffer clear", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                             GL_COLOR_BUFFER_BIT));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

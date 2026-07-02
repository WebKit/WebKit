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
 * \brief GLES3 Scissor tests
 *//*--------------------------------------------------------------------*/

#include "es3fScissorTests.hpp"

#include "glsScissorTests.hpp"

#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrContextUtil.hpp"

#include "tcuVector.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"

#include "gluStrUtil.hpp"
#include "gluDrawUtil.hpp"

#include "glwEnums.hpp"
#include "deDefs.hpp"

namespace deqp
{
namespace gles3
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
        const PrimitiveType type;
        const int primitives;
    } cases[] = {{"contained_quads", "Triangles fully inside scissor area (single call)", Vec4(0.1f, 0.1f, 0.8f, 0.8f),
                  Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 30},
                 {"partial_quads", "Triangles partially inside scissor area (single call)",
                  Vec4(0.3f, 0.3f, 0.4f, 0.4f), Vec4(0.2f, 0.2f, 0.6f, 0.6f), TRIANGLE, 30},
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

    addChild(createFramebufferClearTest(tc, rc, "clear_fixed_buffer", "Fixed point color clear", CLEAR_COLOR_FIXED));
    addChild(createFramebufferClearTest(tc, rc, "clear_int_buffer", "Integer color clear", CLEAR_COLOR_INT));
    addChild(
        createFramebufferClearTest(tc, rc, "clear_uint_buffer", "Unsigned integer buffer clear", CLEAR_COLOR_UINT));
    addChild(createFramebufferClearTest(tc, rc, "clear_depth_buffer", "Depth buffer clear", CLEAR_DEPTH));
    addChild(createFramebufferClearTest(tc, rc, "clear_stencil_buffer", "Stencil buffer clear", CLEAR_STENCIL));
    addChild(createFramebufferClearTest(tc, rc, "clear_depth_stencil_buffer", "Fixed point color buffer clear",
                                        CLEAR_DEPTH_STENCIL));

    addChild(createFramebufferBlitTest(tc, rc, "framebuffer_blit_center",
                                       "Blit to default framebuffer, scissor away edges",
                                       Vec4(0.1f, 0.1f, 0.8f, 0.8f)));
    addChild(createFramebufferBlitTest(tc, rc, "framebuffer_blit_corner",
                                       "Blit to default framebuffer, scissor all but a corner",
                                       Vec4(0.6f, 0.6f, 0.5f, 0.5f)));
    addChild(createFramebufferBlitTest(tc, rc, "framebuffer_blit_none",
                                       "Blit to default framebuffer, scissor area outside screen",
                                       Vec4(1.6f, 0.6f, 0.5f, 0.5f)));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp

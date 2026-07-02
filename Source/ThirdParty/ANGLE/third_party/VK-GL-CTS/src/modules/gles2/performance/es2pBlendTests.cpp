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
 * \brief Blend performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pBlendTests.hpp"
#include "glsShaderPerformanceCase.hpp"
#include "tcuTestLog.hpp"
#include "gluStrUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles2
{
namespace Performance
{

using namespace gls;
using namespace glw; // GL types
using tcu::TestLog;
using tcu::Vec4;

class BlendCase : public ShaderPerformanceCase
{
public:
    BlendCase(Context &context, const char *name, const char *description, GLenum modeRGB, GLenum modeAlpha,
              GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
    ~BlendCase(void);

    void init(void);

private:
    void setupRenderState(void);

    GLenum m_modeRGB;
    GLenum m_modeAlpha;
    GLenum m_srcRGB;
    GLenum m_dstRGB;
    GLenum m_srcAlpha;
    GLenum m_dstAlpha;
};

BlendCase::BlendCase(Context &context, const char *name, const char *description, GLenum modeRGB, GLenum modeAlpha,
                     GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
    : ShaderPerformanceCase(context.getTestContext(), context.getRenderContext(), name, description, CASETYPE_FRAGMENT)
    , m_modeRGB(modeRGB)
    , m_modeAlpha(modeAlpha)
    , m_srcRGB(srcRGB)
    , m_dstRGB(dstRGB)
    , m_srcAlpha(srcAlpha)
    , m_dstAlpha(dstAlpha)
{
}

BlendCase::~BlendCase(void)
{
}

void BlendCase::init(void)
{
    TestLog &log = m_testCtx.getLog();

    log << TestLog::Message << "modeRGB: " << glu::getBlendEquationStr(m_modeRGB) << TestLog::EndMessage;
    log << TestLog::Message << "modeAlpha: " << glu::getBlendEquationStr(m_modeAlpha) << TestLog::EndMessage;
    log << TestLog::Message << "srcRGB: " << glu::getBlendFactorStr(m_srcRGB) << TestLog::EndMessage;
    log << TestLog::Message << "dstRGB: " << glu::getBlendFactorStr(m_dstRGB) << TestLog::EndMessage;
    log << TestLog::Message << "srcAlpha: " << glu::getBlendFactorStr(m_srcAlpha) << TestLog::EndMessage;
    log << TestLog::Message << "dstAlpha: " << glu::getBlendFactorStr(m_dstAlpha) << TestLog::EndMessage;

    m_vertShaderSource = "attribute highp vec4 a_position;\n"
                         "attribute mediump vec4 a_color;\n"
                         "varying mediump vec4 v_color;\n"
                         "void main (void)\n"
                         "{\n"
                         "    gl_Position = a_position;\n"
                         "    v_color = a_color;\n"
                         "}\n";
    m_fragShaderSource = "varying mediump vec4 v_color;\n"
                         "void main (void)\n"
                         "{\n"
                         "    gl_FragColor = v_color;\n"
                         "}\n";

    m_attributes.push_back(AttribSpec("a_color", Vec4(0.0f, 0.5f, 0.5f, 1.0f), Vec4(0.5f, 1.0f, 0.0f, 0.5f),
                                      Vec4(0.5f, 0.0f, 1.0f, 0.5f), Vec4(1.0f, 0.5f, 0.5f, 0.0f)));

    ShaderPerformanceCase::init();
}

void BlendCase::setupRenderState(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    gl.enable(GL_BLEND);
    gl.blendEquationSeparate(m_modeRGB, m_modeAlpha);
    gl.blendFuncSeparate(m_srcRGB, m_dstRGB, m_srcAlpha, m_dstAlpha);

    GLU_EXPECT_NO_ERROR(gl.getError(), "After render state setup");
}

BlendTests::BlendTests(Context &context) : TestCaseGroup(context, "blend", "Blend Performance Tests")
{
}

BlendTests::~BlendTests(void)
{
}

void BlendTests::init(void)
{
    static const struct
    {
        const char *name;
        GLenum modeRGB;
        GLenum modeAlpha;
        GLenum srcRGB;
        GLenum dstRGB;
        GLenum srcAlpha;
        GLenum dstAlpha;
    } cases[] = {
        // Single blend func, factor one.
        {"add", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE, GL_ONE, GL_ONE, GL_ONE},
        {"subtract", GL_FUNC_SUBTRACT, GL_FUNC_SUBTRACT, GL_ONE, GL_ONE, GL_ONE, GL_ONE},
        {"reverse_subtract", GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_ONE, GL_ONE, GL_ONE, GL_ONE},

        // Porter-duff modes that can be implemented.
        {"dst_atop", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA, GL_ONE, GL_ZERO},
        {"dst_in", GL_FUNC_ADD, GL_FUNC_ADD, GL_ZERO, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA},
        {"dst_out", GL_FUNC_ADD, GL_FUNC_ADD, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA},
        {"dst_over", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA},
        {"src_atop", GL_FUNC_ADD, GL_FUNC_ADD, GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE},
        {"src_in", GL_FUNC_ADD, GL_FUNC_ADD, GL_DST_ALPHA, GL_ZERO, GL_DST_ALPHA, GL_ZERO},
        {"src_out", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE_MINUS_DST_ALPHA, GL_ZERO, GL_ONE_MINUS_DST_ALPHA, GL_ZERO},
        {"src_over", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA},
        {"multiply", GL_FUNC_ADD, GL_FUNC_ADD, GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO},
        {"screen", GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA}};

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
        addChild(new BlendCase(m_context, cases[caseNdx].name, "", cases[caseNdx].modeRGB, cases[caseNdx].modeAlpha,
                               cases[caseNdx].srcRGB, cases[caseNdx].dstRGB, cases[caseNdx].srcAlpha,
                               cases[caseNdx].dstAlpha));
}

} // namespace Performance
} // namespace gles2
} // namespace deqp

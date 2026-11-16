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
 * \brief gl_FragData[] tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderFragDataTests.hpp"

#include "glsShaderLibrary.hpp"

#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluDrawUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluObjectWrapper.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::string;
using tcu::TestLog;

namespace
{

enum IndexExprType
{
    INDEX_EXPR_STATIC = 0,
    INDEX_EXPR_UNIFORM,
    INDEX_EXPR_DYNAMIC,

    INDEX_EXPR_TYPE_LAST
};

static bool compareSingleColor(tcu::TestLog &log, const tcu::Surface &surface, tcu::RGBA expectedColor,
                               tcu::RGBA threshold)
{
    const int maxPrints = 10;
    int numFailedPixels = 0;

    log << TestLog::Message << "Expecting " << expectedColor << " with threshold " << threshold << TestLog::EndMessage;

    for (int y = 0; y < surface.getHeight(); y++)
    {
        for (int x = 0; x < surface.getWidth(); x++)
        {
            const tcu::RGBA resultColor = surface.getPixel(x, y);
            const bool isOk             = compareThreshold(resultColor, expectedColor, threshold);

            if (!isOk)
            {
                if (numFailedPixels < maxPrints)
                    log << TestLog::Message << "ERROR: Got " << resultColor << " at (" << x << ", " << y << ")!"
                        << TestLog::EndMessage;
                else if (numFailedPixels == maxPrints)
                    log << TestLog::Message << "..." << TestLog::EndMessage;

                numFailedPixels += 1;
            }
        }
    }

    if (numFailedPixels > 0)
    {
        log << TestLog::Message << "Found " << numFailedPixels << " invalid pixels, comparison FAILED!"
            << TestLog::EndMessage;
        log << TestLog::Image("ResultImage", "Result Image", surface);
        return false;
    }
    else
    {
        log << TestLog::Message << "Image comparison passed." << TestLog::EndMessage;
        return true;
    }
}

class FragDataIndexingCase : public TestCase
{
public:
    FragDataIndexingCase(Context &context, const char *name, const char *description, IndexExprType indexExprType)
        : TestCase(context, name, description)
        , m_indexExprType(indexExprType)
    {
    }

    static glu::ProgramSources genSources(const IndexExprType indexExprType)
    {
        const char *const fragIndexExpr = indexExprType == INDEX_EXPR_STATIC  ? "0" :
                                          indexExprType == INDEX_EXPR_UNIFORM ? "u_index" :
                                          indexExprType == INDEX_EXPR_DYNAMIC ? "int(v_index)" :
                                                                                nullptr;
        glu::ProgramSources sources;

        DE_ASSERT(fragIndexExpr);

        sources << glu::VertexSource("attribute highp vec4 a_position;\n"
                                     "attribute highp float a_index;\n"
                                     "attribute highp vec4 a_color;\n"
                                     "varying mediump float v_index;\n"
                                     "varying mediump vec4 v_color;\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_color = a_color;\n"
                                     "    v_index = a_index;\n"
                                     "}\n");

        sources << glu::FragmentSource(string("varying mediump vec4 v_color;\n"
                                              "varying mediump float v_index;\n"
                                              "uniform mediump int u_index;\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_FragData[") +
                                       fragIndexExpr +
                                       "] = v_color;\n"
                                       "}\n");

        return sources;
    }

    IterateResult iterate(void)
    {
        const glu::RenderContext &renderCtx = m_context.getRenderContext();
        const glw::Functions &gl            = renderCtx.getFunctions();
        const glu::ShaderProgram program(renderCtx, genSources(m_indexExprType));
        const int viewportW = de::min(renderCtx.getRenderTarget().getWidth(), 128);
        const int viewportH = de::min(renderCtx.getRenderTarget().getHeight(), 128);

        const float positions[]   = {-1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f};
        const float colors[]      = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                     0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
        const float indexValues[] = {0.0f, 0.0f, 0.0f, 0.0f};
        const uint8_t indices[]   = {0, 1, 2, 2, 1, 3};

        const glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("a_position", 2, 4, 0, &positions[0]),
                                                        glu::va::Float("a_color", 4, 4, 0, &colors[0]),
                                                        glu::va::Float("a_index", 1, 4, 0, &indexValues[0])};

        m_testCtx.getLog() << program;

        if (!program.isOk())
        {
            if (m_indexExprType == INDEX_EXPR_STATIC)
                TCU_FAIL("Compile failed");
            else
                throw tcu::NotSupportedError("Dynamic indexing of gl_FragData[] not supported");
        }

        gl.clearColor(1.0f, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        gl.viewport(0, 0, viewportW, viewportH);
        gl.useProgram(program.getProgram());
        gl.uniform1i(gl.getUniformLocation(program.getProgram(), "u_index"), 0);

        glu::draw(renderCtx, program.getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), &vertexArrays[0],
                  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "Rendering failed");

        {
            tcu::Surface result(viewportW, viewportH);
            const tcu::RGBA threshold =
                renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(1, 1, 1, 1);
            bool isOk;

            glu::readPixels(renderCtx, 0, 0, result.getAccess());
            GLU_EXPECT_NO_ERROR(gl.getError(), "Reading pixels failed");

            isOk = compareSingleColor(m_testCtx.getLog(), result, tcu::RGBA::green(), threshold);

            m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                    isOk ? "Pass" : "Image comparison failed");
        }

        return STOP;
    }

private:
    const IndexExprType m_indexExprType;
};

} // namespace

ShaderFragDataTests::ShaderFragDataTests(Context &context) : TestCaseGroup(context, "fragdata", "gl_FragData[] Tests")
{
}

ShaderFragDataTests::~ShaderFragDataTests(void)
{
}

void ShaderFragDataTests::init(void)
{
    addChild(new FragDataIndexingCase(m_context, "valid_static_index",
                                      "Valid gl_FragData[] assignment using static index", INDEX_EXPR_STATIC));
    addChild(new FragDataIndexingCase(m_context, "valid_uniform_index",
                                      "Valid gl_FragData[] assignment using uniform index", INDEX_EXPR_UNIFORM));
    addChild(new FragDataIndexingCase(m_context, "valid_dynamic_index",
                                      "Valid gl_FragData[] assignment using dynamic index", INDEX_EXPR_DYNAMIC));

    // Negative cases.
    {
        gls::ShaderLibrary library(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo());
        std::vector<tcu::TestNode *> negativeCases = library.loadShaderFile("shaders/fragdata.test");

        for (std::vector<tcu::TestNode *>::iterator i = negativeCases.begin(); i != negativeCases.end(); i++)
            addChild(*i);
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp

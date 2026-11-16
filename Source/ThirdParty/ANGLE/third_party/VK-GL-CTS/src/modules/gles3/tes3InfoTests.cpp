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
 * \brief Platform Information and Capability Tests.
 *//*--------------------------------------------------------------------*/

#include "tes3InfoTests.hpp"
#include "tcuTestLog.hpp"
#include "gluDefs.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace deqp
{
namespace gles3
{

class QueryStringCase : public TestCase
{
public:
    QueryStringCase(Context &context, const char *name, const char *description, uint32_t query)
        : TestCase(context, name, description)
        , m_query(query)
    {
    }

    IterateResult iterate(void)
    {
        const glw::Functions &gl = m_context.getRenderContext().getFunctions();
        const char *result       = (const char *)gl.getString(m_query);

        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetString() failed");

        m_testCtx.getLog() << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

        return STOP;
    }

private:
    uint32_t m_query;
};

class QueryExtensionsCase : public TestCase
{
public:
    QueryExtensionsCase(Context &context) : TestCase(context, "extensions", "Supported Extensions")
    {
    }

    IterateResult iterate(void)
    {
        const vector<string> extensions = m_context.getContextInfo().getExtensions();

        for (vector<string>::const_iterator i = extensions.begin(); i != extensions.end(); i++)
            m_testCtx.getLog() << tcu::TestLog::Message << *i << tcu::TestLog::EndMessage;

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

        return STOP;
    }
};

class RenderTargetInfoCase : public TestCase
{
public:
    RenderTargetInfoCase(Context &context) : TestCase(context, "render_target", "Render Target Info")
    {
    }

    IterateResult iterate(void)
    {
        const tcu::RenderTarget &rt = m_context.getRenderTarget();
        const tcu::PixelFormat &pf  = rt.getPixelFormat();

        m_testCtx.getLog() << tcu::TestLog::Integer("RedBits", "Red channel bits", "", QP_KEY_TAG_NONE, pf.redBits)
                           << tcu::TestLog::Integer("GreenBits", "Green channel bits", "", QP_KEY_TAG_NONE,
                                                    pf.greenBits)
                           << tcu::TestLog::Integer("BlueBits", "Blue channel bits", "", QP_KEY_TAG_NONE, pf.blueBits)
                           << tcu::TestLog::Integer("AlphaBits", "Alpha channel bits", "", QP_KEY_TAG_NONE,
                                                    pf.alphaBits)
                           << tcu::TestLog::Integer("DepthBits", "Depth bits", "", QP_KEY_TAG_NONE, rt.getDepthBits())
                           << tcu::TestLog::Integer("StencilBits", "Stencil bits", "", QP_KEY_TAG_NONE,
                                                    rt.getStencilBits())
                           << tcu::TestLog::Integer("NumSamples", "Number of samples", "", QP_KEY_TAG_NONE,
                                                    rt.getNumSamples())
                           << tcu::TestLog::Integer("Width", "Width", "", QP_KEY_TAG_NONE, rt.getWidth())
                           << tcu::TestLog::Integer("Height", "Height", "", QP_KEY_TAG_NONE, rt.getHeight());

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
};

InfoTests::InfoTests(Context &context) : TestCaseGroup(context, "info", "Platform information queries")
{
}

InfoTests::~InfoTests(void)
{
}

void InfoTests::init(void)
{
    addChild(new QueryStringCase(m_context, "vendor", "Vendor String", GL_VENDOR));
    addChild(new QueryStringCase(m_context, "renderer", "Renderer String", GL_RENDERER));
    addChild(new QueryStringCase(m_context, "version", "Version String", GL_VERSION));
    addChild(new QueryStringCase(m_context, "shading_language_version", "Shading Language Version String",
                                 GL_SHADING_LANGUAGE_VERSION));
    addChild(new QueryExtensionsCase(m_context));
    addChild(new RenderTargetInfoCase(m_context));
}

} // namespace gles3
} // namespace deqp

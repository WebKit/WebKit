#ifndef _GLSFBOCOMPLETENESSTESTS_HPP
#define _GLSFBOCOMPLETENESSTESTS_HPP

/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Common parts for ES2/3 framebuffer completeness tests.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "glsFboUtil.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

namespace deqp
{
namespace gls
{
namespace fboc
{

namespace details
{

using glu::RenderContext;
using tcu::TestCase;
using tcu::TestContext;
typedef TestCase::IterateResult IterateResult;
using std::string;
using tcu::TestCaseGroup;
using tcu::TestLog;

using namespace glw;
using namespace deqp::gls::FboUtil;
using namespace deqp::gls::FboUtil::config;

class Context
{
public:
    Context(TestContext &testCtx, RenderContext &renderCtx, CheckerFactory &factory);
    RenderContext &getRenderContext(void) const
    {
        return m_renderCtx;
    }
    TestContext &getTestContext(void) const
    {
        return m_testCtx;
    }
    const FboVerifier &getVerifier(void) const
    {
        return m_verifier;
    }
    const FormatDB &getCoreFormats(void) const
    {
        return m_coreFormats;
    }
    const FormatDB &getCtxFormats(void) const
    {
        return m_ctxFormats;
    }
    const FormatDB &getAllFormats(void) const
    {
        return m_allFormats;
    }
    bool haveMultiColorAtts(void) const
    {
        return m_haveMultiColorAtts;
    }
    void setHaveMulticolorAtts(bool have)
    {
        m_haveMultiColorAtts = have;
    }
    void addFormats(FormatEntries fmtRange);
    void addExtFormats(FormatExtEntries extRange);
    TestCaseGroup *createRenderableTests(void);
    TestCaseGroup *createAttachmentTests(void);
    TestCaseGroup *createSizeTests(void);

private:
    TestContext &m_testCtx;
    RenderContext &m_renderCtx;
    FormatDB m_coreFormats;
    FormatDB m_ctxFormats;
    FormatDB m_allFormats;
    FboVerifier m_verifier;
    bool m_haveMultiColorAtts;
};

class TestBase : public TestCase
{
public:
    Context &getContext(void) const
    {
        return m_ctx;
    }

protected:
    TestBase(Context &ctx, const string &name, const string &desc)
        : TestCase(ctx.getTestContext(), name.c_str(), desc.c_str())
        , m_ctx(ctx)
    {
    }
    void fail(const char *msg);
    void qualityWarning(const char *msg);
    void pass(void);
    void checkFbo(FboBuilder &builder);
    ImageFormat getDefaultFormat(GLenum attPoint, GLenum bufType) const;

    IterateResult iterate(void);

    virtual IterateResult build(FboBuilder &builder);

    void attachTargetToNew(GLenum target, GLenum bufType, ImageFormat format, GLsizei width, GLsizei height,
                           FboBuilder &builder);
    Context &m_ctx;
};

// Utilities for building
Image *makeImage(GLenum bufType, ImageFormat format, GLsizei width, GLsizei height, FboBuilder &builder);
Attachment *makeAttachment(GLenum bufType, ImageFormat format, GLsizei width, GLsizei height, FboBuilder &builder);

template <typename P>
class ParamTest : public TestBase
{
public:
    typedef P Params;
    ParamTest(Context &ctx, const Params &params)
        : TestBase(ctx, Params::getName(params), Params::getDescription(params))
        , m_params(params)
    {
    }

protected:
    Params m_params;
};

// Shorthand utility
const glw::Functions &gl(const TestBase &test);

} // namespace details

using details::Context;
using details::gl;
using details::ParamTest;
using details::TestBase;

} // namespace fboc
} // namespace gls
} // namespace deqp

#endif // _GLSFBOCOMPLETENESSTESTS_HPP

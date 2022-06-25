//
//  Copyright 2019 The ANGLE Project Authors. All rights reserved.
//  Use of this source code is governed by a BSD-style license that can be
//  found in the LICENSE file.
//
//  tes31Context_override.cpp:
//    Issue 3687: Overrides for dEQP's OpenGL ES 3.1 test context
//

// Keep the delta compared to dEQP at a minimum
// clang-format off

#include "tes31Context.hpp"
#include "gluRenderConfig.hpp"
#include "gluFboRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "gluDummyRenderContext.hpp"
#include "tcuCommandLine.hpp"

namespace deqp
{
namespace gles31
{

Context::Context (tcu::TestContext& testCtx, glu::ApiType apiType)
    : m_testCtx     (testCtx)
    , m_renderCtx   (DE_NULL)
    , m_contextInfo (DE_NULL)
    , m_apiType     (apiType)
{
    if (m_testCtx.getCommandLine().getRunMode() == tcu::RUNMODE_EXECUTE)
        createRenderContext();
    else
    {
        // \todo [2016-11-15 pyry] Many tests (erroneously) inspect context type
        //                         during test hierarchy construction. We should fix that
        //                         and revert empty context to advertise unknown context type.
        m_renderCtx = new glu::EmptyRenderContext(glu::ContextType(glu::ApiType::es(3,1)));
    }
}

Context::~Context (void)
{
    destroyRenderContext();
}

void Context::createRenderContext (void)
{
    DE_ASSERT(!m_renderCtx && !m_contextInfo);

    try
    {

// Issue 3687
// OpenGL ES 3.2 contexts are not fully supported yet. Creating a 3.2 context results in a number of test
// failures as they assume the existence of extensions that are not supported.
// Revert with Issue 3688
#if 0
        m_renderCtx        = glu::createDefaultRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(), m_apiType);
#else
        // Override the original behavior (above) to create a 3.1 context
        m_renderCtx     = glu::createDefaultRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(), glu::ApiType::es(3, 1));
#endif
        m_contextInfo   = glu::ContextInfo::create(*m_renderCtx);
    }
    catch (...)
    {
        destroyRenderContext();
        throw;
    }
}

void Context::destroyRenderContext (void)
{
    delete m_contextInfo;
    delete m_renderCtx;

    m_contextInfo   = DE_NULL;
    m_renderCtx     = DE_NULL;
}

const tcu::RenderTarget& Context::getRenderTarget (void) const
{
    return m_renderCtx->getRenderTarget();
}

} // gles31
} // deqp

// clang-format on

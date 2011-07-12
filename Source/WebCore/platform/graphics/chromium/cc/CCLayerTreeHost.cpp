/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCLayerTreeHost.h"

#include "TraceEvent.h"
#include "cc/CCLayerTreeHostCommitter.h"
#include "cc/CCLayerTreeHostImpl.h"


namespace WebCore {

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client)
    : m_client(client)
    , m_frameNumber(0)
{
}

void CCLayerTreeHost::init()
{
#if USE(THREADED_COMPOSITING)
    m_proxy = createLayerTreeHostImplProxy();
    ASSERT(m_proxy->isStarted());
    m_proxy->setNeedsCommitAndRedraw();
#endif
}

CCLayerTreeHost::~CCLayerTreeHost()
{
    TRACE_EVENT("CCLayerTreeHost::~CCLayerTreeHost", this, 0);
#if USE(THREADED_COMPOSITING)
    m_proxy->stop();
    m_proxy.clear();
#endif
}

void CCLayerTreeHost::beginCommit()
{
}

void CCLayerTreeHost::commitComplete()
{
    m_frameNumber++;
}

void CCLayerTreeHost::animateAndLayout(double frameBeginTime)
{
    m_client->animateAndLayout(frameBeginTime);
}

PassOwnPtr<CCLayerTreeHostCommitter> CCLayerTreeHost::createLayerTreeHostCommitter()
{
    return CCLayerTreeHostCommitter::create();
}

void CCLayerTreeHost::setNeedsCommitAndRedraw()
{
#if USE(THREADED_COMPOSITING)
    m_proxy->setNeedsCommitAndRedraw();
#endif
}

void CCLayerTreeHost::setNeedsRedraw()
{
    TRACE_EVENT("CCLayerTreeHost::setNeedsRedraw", this, 0);
#if USE(THREADED_COMPOSITING)
    m_proxy->setNeedsRedraw();
#endif
}

}

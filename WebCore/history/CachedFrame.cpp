/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "config.h"
#include "CachedPage.h"

#include "CachedFramePlatformData.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include <wtf/RefCountedLeakCounter.h>

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#endif

namespace WebCore {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter& cachedFrameCounter()
{
    DEFINE_STATIC_LOCAL(WTF::RefCountedLeakCounter, counter, ("CachedFrame"));
    return counter;
}
#endif

CachedFrame::CachedFrame(Frame* frame)
    : m_document(frame->document())
    , m_documentLoader(frame->loader()->documentLoader())
    , m_view(frame->view())
    , m_mousePressNode(frame->eventHandler()->mousePressNode())
    , m_URL(frame->loader()->url())
    , m_cachedFrameScriptData(frame)
{
#ifndef NDEBUG
    cachedFrameCounter().increment();
#endif
    m_document->documentWillBecomeInactive(); 
    frame->clearTimers();
    m_document->setInPageCache(true);
}

CachedFrame::~CachedFrame()
{
#ifndef NDEBUG
    cachedFrameCounter().decrement();
#endif

    clear();
}

void CachedFrame::restore(Frame* frame)
{
    ASSERT(m_document->view() == m_view);
    
    m_cachedFrameScriptData.restore(frame);

#if ENABLE(SVG)
    if (m_document && m_document->svgExtensions())
        m_document->accessSVGExtensions()->unpauseAnimations();
#endif

    frame->animation()->resumeAnimations(m_document.get());
    frame->eventHandler()->setMousePressNode(mousePressNode());
}

void CachedFrame::clear()
{
    if (!m_document)
        return;

    if (m_cachedFramePlatformData)
        m_cachedFramePlatformData->clear();
        
    ASSERT(m_view);
    ASSERT(m_document->frame() == m_view->frame());

    if (m_document->inPageCache()) {
        Frame::clearTimers(m_view.get(), m_document.get());

        m_document->setInPageCache(false);
        // FIXME: We don't call willRemove here. Why is that OK?
        m_document->detach();
        m_document->removeAllEventListenersFromAllNodes();

        m_view->clearFrame();
    }

    ASSERT(!m_document->inPageCache());

    m_document = 0;
    m_view = 0;
    m_mousePressNode = 0;
    m_URL = KURL();

    m_cachedFramePlatformData.clear();

    m_cachedFrameScriptData.clear();
}

void CachedFrame::setCachedFramePlatformData(CachedFramePlatformData* data)
{
    m_cachedFramePlatformData.set(data);
}

CachedFramePlatformData* CachedFrame::cachedFramePlatformData()
{
    return m_cachedFramePlatformData.get();
}

} // namespace WebCore

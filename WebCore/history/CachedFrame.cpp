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
#include "CString.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "Logging.h"
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
    , m_url(frame->loader()->url())
{
#ifndef NDEBUG
    cachedFrameCounter().increment();
#endif
    ASSERT(m_document);
    ASSERT(m_documentLoader);
    ASSERT(m_view);

    // Active DOM objects must be suspended before we cached the frame script data
    m_document->suspendActiveDOMObjects();
    m_cachedFrameScriptData.set(new ScriptCachedFrameData(frame));
    
    m_document->documentWillBecomeInactive(); 
    frame->clearTimers();
    m_document->setInPageCache(true);
    
    frame->loader()->client()->savePlatformDataToCachedFrame(this);

    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        m_childFrames.append(CachedFrame::create(child));

    LOG(PageCache, "Finished creating CachedFrame with url %s and documentloader %p\n", m_url.string().utf8().data(), m_documentLoader.get());
}

CachedFrame::~CachedFrame()
{
#ifndef NDEBUG
    cachedFrameCounter().decrement();
#endif

    clear();
}

void CachedFrame::restore()
{
    ASSERT(m_document->view() == m_view);
    
    Frame* frame = m_view->frame();
    m_cachedFrameScriptData->restore(frame);

#if ENABLE(SVG)
    if (m_document->svgExtensions())
        m_document->accessSVGExtensions()->unpauseAnimations();
#endif

    frame->animation()->resumeAnimations(m_document.get());
    frame->eventHandler()->setMousePressNode(mousePressNode());
    m_document->resumeActiveDOMObjects();

    // It is necessary to update any platform script objects after restoring the
    // cached page.
    frame->script()->updatePlatformScriptObjects();
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

        // FIXME: Why do we need to call removeAllEventListeners here? When the document is in page cache, this method won't work
        // fully anyway, because the document won't be able to access its DOMWindow object (due to being frameless).
        m_document->removeAllEventListeners();

        m_document->setInPageCache(false);
        // FIXME: We don't call willRemove here. Why is that OK?
        m_document->detach();
        m_view->clearFrame();
    }

    ASSERT(!m_document->inPageCache());

    m_document = 0;
    m_view = 0;
    m_mousePressNode = 0;
    m_url = KURL();

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

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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "config.h"
#include "CachedFrame.h"

#include "CSSAnimationController.h"
#include "CachedFramePlatformData.h"
#include "CachedPage.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentTimeline.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "Logging.h"
#include "Page.h"
#include "PageCache.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "ScriptController.h"
#include "SerializedScriptValue.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY) || ENABLE(TOUCH_EVENTS)
#include "Chrome.h"
#include "ChromeClient.h"
#endif

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, cachedFrameCounter, ("CachedFrame"));

CachedFrameBase::CachedFrameBase(Frame& frame)
    : m_document(frame.document())
    , m_documentLoader(frame.loader().documentLoader())
    , m_view(frame.view())
    , m_url(frame.document()->url())
    , m_isMainFrame(!frame.tree().parent())
{
}

CachedFrameBase::~CachedFrameBase()
{
#ifndef NDEBUG
    cachedFrameCounter.decrement();
#endif
    // CachedFrames should always have had destroy() called by their parent CachedPage
    ASSERT(!m_document);
}

void CachedFrameBase::pruneDetachedChildFrames()
{
    for (size_t i = m_childFrames.size(); i;) {
        --i;
        if (m_childFrames[i]->view()->frame().page())
            continue;
        m_childFrames[i]->destroy();
        m_childFrames.remove(i);
    }
}

void CachedFrameBase::restore()
{
    ASSERT(m_document->view() == m_view);

    if (m_isMainFrame)
        m_view->setParentVisible(true);

    Frame& frame = m_view->frame();
    m_cachedFrameScriptData->restore(frame);

    if (m_document->svgExtensions())
        m_document->accessSVGExtensions().unpauseAnimations();

    m_document->resume(ReasonForSuspension::PageCache);

    // It is necessary to update any platform script objects after restoring the
    // cached page.
    frame.script().updatePlatformScriptObjects();

    frame.loader().client().didRestoreFromPageCache();

    pruneDetachedChildFrames();

    // Reconstruct the FrameTree. And open the child CachedFrames in their respective FrameLoaders.
    for (auto& childFrame : m_childFrames) {
        ASSERT(childFrame->view()->frame().page());
        frame.tree().appendChild(childFrame->view()->frame());
        childFrame->open();
        ASSERT_WITH_SECURITY_IMPLICATION(m_document == frame.document());
    }

#if PLATFORM(IOS_FAMILY)
    if (m_isMainFrame) {
        frame.loader().client().didRestoreFrameHierarchyForCachedFrame();

        if (DOMWindow* domWindow = m_document->domWindow()) {
            // FIXME: Add SCROLL_LISTENER to the list of event types on Document, and use m_document->hasListenerType(). See <rdar://problem/9615482>.
            // FIXME: Can use Document::hasListenerType() now.
            if (domWindow->scrollEventListenerCount() && frame.page())
                frame.page()->chrome().client().setNeedsScrollNotifications(frame, true);
        }
    }
#endif

    frame.view()->didRestoreFromPageCache();
}

CachedFrame::CachedFrame(Frame& frame)
    : CachedFrameBase(frame)
{
#ifndef NDEBUG
    cachedFrameCounter.increment();
#endif
    ASSERT(m_document);
    ASSERT(m_documentLoader);
    ASSERT(m_view);
    ASSERT(m_document->pageCacheState() == Document::InPageCache);

    // Create the CachedFrames for all Frames in the FrameTree.
    for (Frame* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        m_childFrames.append(std::make_unique<CachedFrame>(*child));

    // Active DOM objects must be suspended before we cache the frame script data.
    m_document->suspend(ReasonForSuspension::PageCache);

    m_cachedFrameScriptData = std::make_unique<ScriptCachedFrameData>(frame);

    m_document->domWindow()->suspendForPageCache();

    // Clear FrameView to reset flags such as 'firstVisuallyNonEmptyLayoutCallbackPending' so that the
    // 'DidFirstVisuallyNonEmptyLayout' callback gets called against when restoring from PageCache.
    m_view->resetLayoutMilestones();

    frame.loader().client().savePlatformDataToCachedFrame(this);

    // documentWillSuspendForPageCache() can set up a layout timer on the FrameView, so clear timers after that.
    frame.clearTimers();

    // Deconstruct the FrameTree, to restore it later.
    // We do this for two reasons:
    // 1 - We reuse the main frame, so when it navigates to a new page load it needs to start with a blank FrameTree.
    // 2 - It's much easier to destroy a CachedFrame while it resides in the PageCache if it is disconnected from its parent.
    for (unsigned i = 0; i < m_childFrames.size(); ++i)
        frame.tree().removeChild(m_childFrames[i]->view()->frame());

    if (!m_isMainFrame)
        frame.page()->decrementSubframeCount();

    frame.loader().client().didSaveToPageCache();

#ifndef NDEBUG
    if (m_isMainFrame)
        LOG(PageCache, "Finished creating CachedFrame for main frame url '%s' and DocumentLoader %p\n", m_url.string().utf8().data(), m_documentLoader.get());
    else
        LOG(PageCache, "Finished creating CachedFrame for child frame with url '%s' and DocumentLoader %p\n", m_url.string().utf8().data(), m_documentLoader.get());
#endif

#if PLATFORM(IOS_FAMILY)
    if (m_isMainFrame) {
        if (DOMWindow* domWindow = m_document->domWindow()) {
            if (domWindow->scrollEventListenerCount() && frame.page())
                frame.page()->chrome().client().setNeedsScrollNotifications(frame, false);
        }
    }
#endif

    m_document->detachFromCachedFrame(*this);

    ASSERT_WITH_SECURITY_IMPLICATION(!m_documentLoader->isLoading());
}

void CachedFrame::open()
{
    ASSERT(m_view);
    ASSERT(m_document);
    if (!m_isMainFrame)
        m_view->frame().page()->incrementSubframeCount();

    m_document->attachToCachedFrame(*this);

    m_view->frame().loader().open(*this);
}

void CachedFrame::clear()
{
    if (!m_document)
        return;

    // clear() should only be called for Frames representing documents that are no longer in the page cache.
    // This means the CachedFrame has been:
    // 1 - Successfully restore()'d by going back/forward.
    // 2 - destroy()'ed because the PageCache is pruning or the WebView was closed.
    ASSERT(m_document->pageCacheState() == Document::NotInPageCache);
    ASSERT(m_view);
    ASSERT(!m_document->frame() || m_document->frame() == &m_view->frame());

    for (int i = m_childFrames.size() - 1; i >= 0; --i)
        m_childFrames[i]->clear();

    m_document = nullptr;
    m_view = nullptr;
    m_url = URL();

    m_cachedFramePlatformData = nullptr;
    m_cachedFrameScriptData = nullptr;
}

void CachedFrame::destroy()
{
    if (!m_document)
        return;
    
    // Only CachedFrames that are still in the PageCache should be destroyed in this manner
    ASSERT(m_document->pageCacheState() == Document::InPageCache);
    ASSERT(m_view);
    ASSERT(!m_document->frame());

    m_document->domWindow()->willDestroyCachedFrame();

    if (!m_isMainFrame && m_view->frame().page()) {
        m_view->frame().loader().detachViewsAndDocumentLoader();
        m_view->frame().detachFromPage();
    }
    
    for (int i = m_childFrames.size() - 1; i >= 0; --i)
        m_childFrames[i]->destroy();

    if (m_cachedFramePlatformData)
        m_cachedFramePlatformData->clear();

    Frame::clearTimers(m_view.get(), m_document.get());

    m_view->frame().animation().detachFromDocument(m_document.get());

    // FIXME: Why do we need to call removeAllEventListeners here? When the document is in page cache, this method won't work
    // fully anyway, because the document won't be able to access its DOMWindow object (due to being frameless).
    m_document->removeAllEventListeners();

    m_document->setPageCacheState(Document::NotInPageCache);
    m_document->prepareForDestruction();

    clear();
}

void CachedFrame::setCachedFramePlatformData(std::unique_ptr<CachedFramePlatformData> data)
{
    m_cachedFramePlatformData = WTFMove(data);
}

CachedFramePlatformData* CachedFrame::cachedFramePlatformData()
{
    return m_cachedFramePlatformData.get();
}

void CachedFrame::setHasInsecureContent(HasInsecureContent hasInsecureContent)
{
    m_hasInsecureContent = hasInsecureContent;
}

int CachedFrame::descendantFrameCount() const
{
    int count = m_childFrames.size();
    for (size_t i = 0; i < m_childFrames.size(); ++i)
        count += m_childFrames[i]->descendantFrameCount();
    
    return count;
}

} // namespace WebCore

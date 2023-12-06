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

#include "BackForwardCache.h"
#include "CachedFramePlatformData.h"
#include "CachedPage.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NavigationDisabler.h"
#include "Page.h"
#include "RemoteFrame.h"
#include "RemoteFrameView.h"
#include "RenderWidget.h"
#include "SVGDocumentExtensions.h"
#include "ScriptController.h"
#include "SerializedScriptValue.h"
#include "StyleTreeResolver.h"
#include "WindowEventLoop.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY) || ENABLE(TOUCH_EVENTS)
#include "Chrome.h"
#include "ChromeClient.h"
#endif

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, cachedFrameCounter, ("CachedFrame"));

CachedFrameBase::CachedFrameBase(Frame& frame)
    : m_document(is<LocalFrame>(frame) ? downcast<LocalFrame>(frame).document() : nullptr)
    , m_documentLoader(is<LocalFrame>(frame) ? downcast<LocalFrame>(frame).loader().documentLoader() : nullptr)
    , m_view(frame.virtualView())
    , m_url(is<LocalFrame>(frame) ? downcast<LocalFrame>(frame).document()->url() : URL())
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
    m_childFrames.removeAllMatching([] (auto& childFrame) {
        if (childFrame->view()->frame().page())
            return false;
        childFrame->destroy();
        return true;
    });
}

void CachedFrameBase::restore()
{
    ASSERT(m_document->view() == m_view);

    if (m_isMainFrame)
        m_view->setParentVisible(true);

    Ref frame = m_view->frame();
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
    {
        Style::PostResolutionCallbackDisabler disabler(*m_document);
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        NavigationDisabler disableNavigation { nullptr }; // Disable navigation globally.

        if (localFrame)
            m_cachedFrameScriptData->restore(*localFrame);

        if (m_document->svgExtensions())
            m_document->accessSVGExtensions().unpauseAnimations();

        m_document->resume(ReasonForSuspension::BackForwardCache);

        // It is necessary to update any platform script objects after restoring the
        // cached page.
        if (localFrame) {
            localFrame->script().updatePlatformScriptObjects();
            localFrame->loader().client().didRestoreFromBackForwardCache();
        }

        pruneDetachedChildFrames();

        // Reconstruct the FrameTree. And open the child CachedFrames in their respective FrameLoaders.
        for (auto& childFrame : m_childFrames) {
            ASSERT(childFrame->view()->frame().page());
            frame->tree().appendChild(childFrame->view()->frame());
            childFrame->open();
            if (localFrame)
                RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_document == localFrame->document());
            else
                RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_document);
        }
    }

#if PLATFORM(IOS_FAMILY)
    if (m_isMainFrame && localFrame) {
        localFrame->loader().client().didRestoreFrameHierarchyForCachedFrame();

        if (auto* domWindow = m_document->domWindow()) {
            // FIXME: Use Document::hasListenerType(). See <rdar://problem/9615482>.
            if (domWindow->scrollEventListenerCount() && frame->page())
                frame->page()->chrome().client().setNeedsScrollNotifications(*localFrame, true);
        }
    }
#endif

    if (localFrame)
        localFrame->view()->didRestoreFromBackForwardCache();
}

CachedFrame::CachedFrame(Frame& frame)
    : CachedFrameBase(frame)
{
#ifndef NDEBUG
    cachedFrameCounter.increment();
#endif
    ASSERT(m_document || is<RemoteFrame>(frame));
    ASSERT(m_documentLoader || is<RemoteFrame>(frame));
    ASSERT(m_view);
    ASSERT(!m_document || m_document->backForwardCacheState() == Document::InBackForwardCache);

    // Create the CachedFrames for all Frames in the FrameTree.
    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        m_childFrames.append(makeUniqueRef<CachedFrame>(*child));
    }

    if (m_document) {
        RELEASE_ASSERT(m_document->domWindow());
        RELEASE_ASSERT(m_document->domWindow()->frame());

        // Active DOM objects must be suspended before we cache the frame script data.
        m_document->suspend(ReasonForSuspension::BackForwardCache);
    }

    m_cachedFrameScriptData = is<LocalFrame>(frame) ? makeUnique<ScriptCachedFrameData>(downcast<LocalFrame>(frame)) : nullptr;

    if (m_document)
        m_document->domWindow()->suspendForBackForwardCache();

    // Clear FrameView to reset flags such as 'firstVisuallyNonEmptyLayoutCallbackPending' so that the
    // 'DidFirstVisuallyNonEmptyLayout' callback gets called against when restoring from the BackForwardCache.
    RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get());
    if (localFrameView)
        localFrameView->resetLayoutMilestones();

    // The main frame is reused for the navigation and the opener link to its should thus persist.
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    if (!frame.isMainFrame() && localFrame)
        localFrame->loader().detachFromAllOpenedFrames();

    if (localFrame)
        localFrame->loader().client().savePlatformDataToCachedFrame(this);

    // documentWillSuspendForBackForwardCache() can set up a layout timer on the FrameView, so clear timers after that.
    if (localFrame)
        localFrame->clearTimers();

    // Deconstruct the FrameTree, to restore it later.
    // We do this for two reasons:
    // 1 - We reuse the main frame, so when it navigates to a new page load it needs to start with a blank FrameTree.
    // 2 - It's much easier to destroy a CachedFrame while it resides in the BackForwardCache if it is disconnected from its parent.
    Vector<Ref<Frame>> children;
    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        children.append(*child);
    for (auto& child : children)
        frame.tree().removeChild(child);

#ifndef NDEBUG
    if (m_isMainFrame)
        LOG(BackForwardCache, "Finished creating CachedFrame for main frame url '%s' and DocumentLoader %p\n", m_url.string().utf8().data(), m_documentLoader.get());
    else
        LOG(BackForwardCache, "Finished creating CachedFrame for child frame with url '%s' and DocumentLoader %p\n", m_url.string().utf8().data(), m_documentLoader.get());
#endif

#if PLATFORM(IOS_FAMILY)
    if (m_isMainFrame && localFrame) {
        if (auto* domWindow = m_document->domWindow()) {
            if (domWindow->scrollEventListenerCount() && frame.page())
                frame.page()->chrome().client().setNeedsScrollNotifications(*localFrame, false);
        }
    }
#endif

    if (m_document)
        m_document->detachFromCachedFrame(*this);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_documentLoader || !m_documentLoader->isLoading());
}

void CachedFrame::open()
{
    ASSERT(m_view);
    ASSERT(m_document || is<RemoteFrameView>(m_view.get()));

    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get()))
        localFrameView->frame().loader().open(*this);
}

void CachedFrame::clear()
{
    if (!m_document)
        return;

    // clear() should only be called for Frames representing documents that are no longer in the back/forward cache.
    // This means the CachedFrame has been:
    // 1 - Successfully restore()'d by going back/forward.
    // 2 - destroy()'ed because the BackForwardCache is pruning or the WebView was closed.
    ASSERT(m_document->backForwardCacheState() == Document::NotInBackForwardCache);
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
    
    // Only CachedFrames that are still in the BackForwardCache should be destroyed in this manner
    ASSERT(m_document->backForwardCacheState() == Document::InBackForwardCache);
    ASSERT(m_view);
    ASSERT(!m_document->frame());

    m_document->domWindow()->willDestroyCachedFrame();

    Ref frame = m_view->frame();
    if (!m_isMainFrame && m_view->frame().page()) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frame.get()))
            localFrame->loader().detachViewsAndDocumentLoader();
        frame->detachFromPage();
    }
    
    for (int i = m_childFrames.size() - 1; i >= 0; --i)
        m_childFrames[i]->destroy();

    if (m_cachedFramePlatformData)
        m_cachedFramePlatformData->clear();

    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get()); localFrameView && m_document)
        LocalFrame::clearTimers(localFrameView.get(), m_document.get());

    // FIXME: Why do we need to call removeAllEventListeners here? When the document is in back/forward cache, this method won't work
    // fully anyway, because the document won't be able to access its LocalDOMWindow object (due to being frameless).
    m_document->removeAllEventListeners();

    m_document->setBackForwardCacheState(Document::NotInBackForwardCache);
    m_document->willBeRemovedFromFrame();

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

size_t CachedFrame::descendantFrameCount() const
{
    size_t count = m_childFrames.size();
    for (const auto& childFrame : m_childFrames)
        count += childFrame->descendantFrameCount();
    return count;
}

UsedLegacyTLS CachedFrame::usedLegacyTLS() const
{
    if (auto* document = this->document()) {
        if (document->usedLegacyTLS())
            return UsedLegacyTLS::Yes;
    }
    
    for (const auto& cachedFrame : m_childFrames) {
        if (cachedFrame->usedLegacyTLS() == UsedLegacyTLS::Yes)
            return UsedLegacyTLS::Yes;
    }
    
    return UsedLegacyTLS::No;
}

HasInsecureContent CachedFrame::hasInsecureContent() const
{
    if (auto* document = this->document()) {
        if (!document->isSecureContext() || !document->foundMixedContent().isEmpty())
            return HasInsecureContent::Yes;
    }

    for (const auto& cachedFrame : m_childFrames) {
        if (cachedFrame->hasInsecureContent() == HasInsecureContent::Yes)
            return HasInsecureContent::Yes;
    }

    return HasInsecureContent::No;
}

WasPrivateRelayed CachedFrame::wasPrivateRelayed() const
{
    if (auto* document = this->document()) {
        if (document->wasPrivateRelayed())
            return WasPrivateRelayed::Yes;
    }

    bool allFramesRelayed { false };
    for (const auto& cachedFrame : m_childFrames)
        allFramesRelayed |= cachedFrame->wasPrivateRelayed() == WasPrivateRelayed::Yes;

    return allFramesRelayed ? WasPrivateRelayed::Yes : WasPrivateRelayed::No;
}

} // namespace WebCore

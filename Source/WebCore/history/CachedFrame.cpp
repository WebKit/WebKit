/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "DocumentWindow.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NavigationDisabler.h"
#include "RemoteFrame.h"
#include "RemoteFrameView.h"
#include "RenderStyleInlines.h"
#include "RenderWidgetInlines.h"
#include "SVGDocumentExtensions.h"
#include "ScriptController.h"
#include "SerializedScriptValue.h"
#include "StyleTreeResolver.h"
#include "WindowEventLoop.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY) || ENABLE(TOUCH_EVENTS)
#include "Chrome.h"
#include "ChromeClient.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CachedFrame);

CachedFrameBase::CachedFrameBase(Frame& frame)
    : m_view(frame.virtualView())
    , m_isMainFrame(!frame.tree().parent())
{
    if (auto* localFrame = dynamicDowncast<LocalFrame>(frame))
        initializeWithLocalFrame(*localFrame);
}

void CachedFrameBase::initializeWithLocalFrame(LocalFrame& frame)
{
    m_document = frame.document();
    m_documentLoader = frame.loader().documentLoader();
    m_url = frame.document()->url();
}

CachedFrameBase::~CachedFrameBase()
{
    // CachedFrames should always have had destroy() called by their parent CachedPage
    ASSERT(!m_document);
}

RefPtr<FrameView> CachedFrameBase::protectedView() const
{
    return m_view;
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
    RefPtr view = m_view;
    ASSERT(m_document->view() == view);

    if (m_isMainFrame)
        view->setParentVisible(true);

    Ref frame = view->frame();
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
    {
        Ref document = *m_document;
        Style::PostResolutionCallbackDisabler disabler(document);
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        NavigationDisabler disableNavigation { nullptr }; // Disable navigation globally.

        if (localFrame)
            m_cachedFrameScriptData->restore(*localFrame);

        if (CheckedPtr svgExtensions = document->svgExtensionsIfExists())
            svgExtensions->unpauseAnimations();

        document->resume(ReasonForSuspension::BackForwardCache);

        // It is necessary to update any platform script objects after restoring the
        // cached page.
        if (localFrame) {
            localFrame->checkedScript()->updatePlatformScriptObjects();
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

        if (RefPtr window = m_document->window(); window && window->scrollEventListenerCount()) {
            // FIXME: Use Document::hasListenerType(). See <rdar://problem/9615482>.
            if (RefPtr page = frame->page())
                page->chrome().client().setNeedsScrollNotifications(*localFrame, true);
        }
    }
#endif

    if (localFrame)
        localFrame->protectedView()->didRestoreFromBackForwardCache();
}

CachedFrame::CachedFrame(Frame& frame)
    : CachedFrameBase(frame)
{
    RefPtr document = m_document;
    ASSERT(document || is<RemoteFrame>(frame));
    ASSERT(m_documentLoader || is<RemoteFrame>(frame));
    ASSERT(m_view);
    ASSERT(!document || document->backForwardCacheState() == Document::InBackForwardCache);

    // Create the CachedFrames for all Frames in the FrameTree.
    for (RefPtr child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        m_childFrames.append(makeUniqueRef<CachedFrame>(*child));

    if (document) {
        RELEASE_ASSERT(document->window());
        RELEASE_ASSERT(document->window()->frame());

        // Active DOM objects must be suspended before we cache the frame script data.
        document->suspend(ReasonForSuspension::BackForwardCache);
    }

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    m_cachedFrameScriptData = localFrame ? makeUnique<ScriptCachedFrameData>(*localFrame) : nullptr;

    if (document)
        document->protectedWindow()->suspendForBackForwardCache();

    // Clear FrameView to reset flags such as 'firstVisuallyNonEmptyLayoutCallbackPending' so that the
    // 'DidFirstVisuallyNonEmptyLayout' callback gets called against when restoring from the BackForwardCache.
    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get()))
        localFrameView->resetLayoutMilestones();

    // The main frame is reused for the navigation and the opener link to its should thus persist.
    if (localFrame) {
        Ref frameLoader = localFrame->loader();
        if (!frame.isMainFrame())
            localFrame->detachFromAllOpenedFrames();

        frameLoader->client().savePlatformDataToCachedFrame(this);

        // documentWillSuspendForBackForwardCache() can set up a layout timer on the FrameView, so clear timers after that.
        localFrame->clearTimers();
    }

    // Deconstruct the FrameTree, to restore it later.
    // We do this for two reasons:
    // 1 - We reuse the main frame, so when it navigates to a new page load it needs to start with a blank FrameTree.
    // 2 - It's much easier to destroy a CachedFrame while it resides in the BackForwardCache if it is disconnected from its parent.
    Vector<Ref<Frame>> children;
    for (RefPtr child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
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
        if (RefPtr window = document->window(); window && window->scrollEventListenerCount()) {
            if (RefPtr page = frame.page())
                page->chrome().client().setNeedsScrollNotifications(*localFrame, false);
        }
    }
#endif

    if (document)
        document->detachFromCachedFrame(*this);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_documentLoader || !m_documentLoader->isLoading());
}

void CachedFrame::open()
{
    ASSERT(m_view);
    ASSERT(m_document || is<RemoteFrameView>(m_view.get()));

    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get()))
        localFrameView->protectedFrame()->loader().open(*this);
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
    RefPtr document = m_document;
    if (!document)
        return;
    
    // Only CachedFrames that are still in the BackForwardCache should be destroyed in this manner
    ASSERT(document->backForwardCacheState() == Document::InBackForwardCache);
    ASSERT(m_view);
    ASSERT(!document->frame());

    document->protectedWindow()->willDestroyCachedFrame();

    Ref frame = m_view->frame();
    if (!m_isMainFrame && m_view->frame().page()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get()))
            localFrame->loader().detachViewsAndDocumentLoader();
        frame->detachFromPage();
    }
    
    for (int i = m_childFrames.size() - 1; i >= 0; --i)
        m_childFrames[i]->destroy();

    if (m_cachedFramePlatformData)
        m_cachedFramePlatformData->clear();

    if (RefPtr localFrameView = dynamicDowncast<LocalFrameView>(m_view.get()); localFrameView)
        LocalFrame::clearTimers(localFrameView.get(), document.get());

    // FIXME: Why do we need to call removeAllEventListeners here? When the document is in back/forward cache, this method won't work
    // fully anyway, because the document won't be able to access its LocalDOMWindow object (due to being frameless).
    document->removeAllEventListeners();

    document->setBackForwardCacheState(Document::NotInBackForwardCache);
    document->willBeRemovedFromFrame();

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

// FIXME: Remove all uses of HasInsecureContent across the codebase since we no longer allow insecure content.
HasInsecureContent CachedFrame::hasInsecureContent() const
{
    if (auto* document = this->document()) {
        if (!document->isSecureContext())
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

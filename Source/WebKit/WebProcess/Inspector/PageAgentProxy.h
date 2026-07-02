/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/PageAgentInstrumentation.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {
class DOMWrapperWorld;
class DocumentLoader;
class LocalFrame;
class Page;
class RenderObject;
}

namespace WebKit {

class WebPage;

// PageAgentProxy registers on a page's InstrumentingAgents (via PageInspectorController)
// so page-level instrumentation hooks resolve to this proxy under Site Isolation.
// Each instance forwards page lifecycle events to ProxyingPageAgent in the UIProcess via IPC.
class PageAgentProxy : public Inspector::PageAgentInstrumentation, public CanMakeCheckedPtr<PageAgentProxy> {
    WTF_MAKE_TZONE_ALLOCATED(PageAgentProxy);
    WTF_MAKE_NONCOPYABLE(PageAgentProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageAgentProxy);
public:
    PageAgentProxy(WebCore::WebAgentContext&, WebPage&);
    ~PageAgentProxy() override;

    // AbstractCanMakeCheckedPtr overrides
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    // InspectorAgentBase (via PageAgentInstrumentation)
    void didCreateFrontendAndBackend() final;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final;

    // PageAgentInstrumentation
    Inspector::CommandResult<void> enable() final;
    Inspector::CommandResult<void> disable() final;

    void domContentEventFired() final;
    void loadEventFired() final;
    void frameNavigated(WebCore::LocalFrame&) final;
    void frameDetached(WebCore::LocalFrame&) final;
    void loaderDetachedFromFrame(WebCore::DocumentLoader&) final;
    void accessibilitySettingsDidChange() final;
    void defaultUserPreferencesDidChange() final;
#if ENABLE(DARK_MODE_CSS)
    void defaultAppearanceDidChange() final;
#endif
    void applyUserAgentOverride(String&) final;
    void applyEmulatedMedia(AtomString&) final;
    void didClearWindowObjectInWorld(WebCore::LocalFrame&, WebCore::DOMWrapperWorld&) final;
    void didPaint(WebCore::RenderObject&, const WebCore::LayoutRect&) final;
    void didLayout() final;
    void didScroll() final;
    void didRecalculateStyle() final;

private:
    WeakRef<WebCore::Page> m_inspectedPage;
    WeakRef<WebPage> m_page;
};

} // namespace WebKit

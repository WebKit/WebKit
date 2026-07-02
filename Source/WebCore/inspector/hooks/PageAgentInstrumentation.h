/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <WebCore/InspectorWebAgentBase.h>
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/Forward.h>

namespace WebCore {
class DOMWrapperWorld;
class DocumentLoader;
class LayoutRect;
class LocalFrame;
class RenderObject;
}

namespace Inspector {

// PageAgentInstrumentation is the abstract interface for page-level instrumentation hooks.
// Both InspectorPageAgent (WebCore, handles commands + instrumentation in legacy mode) and
// PageAgentProxy (WebKit, forwards events via IPC under Site Isolation) can implement this.
// InstrumentingAgents has a separate PageProxy slot so both the in-process agent and the
// cross-process proxy can coexist.
//
// Inherits InspectorAgentBase so subclasses register with AgentRegistry, and
// AbstractCanMakeCheckedPtr so InstrumentingAgents can hold CheckedPtr to it.
// Concrete subclasses must use CanMakeCheckedPtr and WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR.
class PageAgentInstrumentation : public WebCore::InspectorAgentBase, public AbstractCanMakeCheckedPtr {
public:
    ~PageAgentInstrumentation() override = default;

    virtual CommandResult<void> enable() = 0;
    virtual CommandResult<void> disable() = 0;

    // Frame lifecycle events
    // FIXME: domContentEventFired/loadEventFired are not yet dispatched to enabledPageProxy()
    // from InspectorInstrumentation; only the in-process InspectorPageAgent receives them.
    // Wire dispatch in a follow-up: https://bugs.webkit.org/show_bug.cgi?id=308898
    virtual void domContentEventFired() = 0;
    virtual void loadEventFired() = 0;
    virtual void frameNavigated(WebCore::LocalFrame&) = 0;
    virtual void frameDetached(WebCore::LocalFrame&) = 0;
    virtual void loaderDetachedFromFrame(WebCore::DocumentLoader&) = 0;

    // User preference / accessibility events
    virtual void accessibilitySettingsDidChange() = 0;
    virtual void defaultUserPreferencesDidChange() = 0;
#if ENABLE(DARK_MODE_CSS)
    virtual void defaultAppearanceDidChange() = 0;
#endif

    // Emulation hooks called from WebCore via InspectorInstrumentation
    virtual void applyUserAgentOverride(String&) = 0;
    virtual void applyEmulatedMedia(AtomString&) = 0;

    // Bootstrap script injection
    virtual void didClearWindowObjectInWorld(WebCore::LocalFrame&, WebCore::DOMWrapperWorld&) = 0;

    // Overlay / paint rect hooks
    virtual void didPaint(WebCore::RenderObject&, const WebCore::LayoutRect&) = 0;
    virtual void didLayout() = 0;
    virtual void didScroll() = 0;
    virtual void didRecalculateStyle() = 0;

protected:
    PageAgentInstrumentation(WebCore::WebAgentContext& context)
        : InspectorAgentBase("Page"_s, context)
    {
    }
};

} // namespace Inspector

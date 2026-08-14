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

#include "config.h"
#include "EmulationManager.h"

#include "WebInspectorBackend.h"
#include "WebPage.h"
#include <WebCore/Document.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/Page.h>

namespace Inspector {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmulationManager);

Ref<EmulationManager> EmulationManager::create(WebKit::WebInspectorBackend& backend)
{
    return adoptRef(*new EmulationManager(backend));
}

EmulationManager::EmulationManager(WebKit::WebInspectorBackend& backend)
    : m_backend(backend)
{
}

EmulationManager::~EmulationManager() = default;

void EmulationManager::setEmulatedMedia(const String& media)
{
    if (media.isEmpty())
        m_overrides.emulatedMedia = std::nullopt;
    else
        m_overrides.emulatedMedia = media;
}

void EmulationManager::applyLocally()
{
    RefPtr page = m_backend->page();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    // Mirrors InspectorPageAgent::setEmulatedMedia's layout kick. See webkit.org/b/308898.
    // FIXME: Schedule a rendering update instead of synchronously updating the layout (bugs.webkit.org/b/308898).
    // Intentionally distinct from the frame-scoped applyLocally(LocalFrame&) below: this set-time path
    // is page-wide (updateStyleAfterChangeInEnvironment()), whereas the per-frame join path forces a
    // scheduleFullStyleRebuild() on a single freshly-instrumented document. Do not merge them -- a
    // page-wide full style rebuild on every emulation set would be a perf regression.
    corePage->updateStyleAfterChangeInEnvironment();
    corePage->forEachLocalFrame([](WebCore::LocalFrame& frame) {
        if (RefPtr document = frame.document()) {
            document->updateLayout();
            document->evaluateMediaQueriesAndReportChanges();
        }
    });
}

void EmulationManager::applyLocally(WebCore::LocalFrame& frame)
{
    RefPtr document = frame.document();
    if (!document)
        return;

    // Frame-scoped, synchronous sibling of the page-wide applyLocally() above. A cross-origin iframe
    // that is already present when page instrumentation is enabled under Site Isolation had its
    // PageAgentProxy installed after its document committed, so nothing recalculated its author
    // styles against the now-instrumented frame; without this it reports the default UA color
    // instead of its author stylesheet. Force a full style rebuild + synchronous layout on just this
    // document, then re-evaluate its media queries so any active emulated-media override the proxy
    // inherited takes effect. Scoped to one frame (not corePage->updateStyleAfterChangeInEnvironment(),
    // which is page-wide) and synchronous to match the set-time path. See webkit.org/b/308896.
    document->scheduleFullStyleRebuild();

    // Only force a synchronous layout + media-query re-evaluation when an emulated-media override
    // is actually set. The scheduleFullStyleRebuild() above unconditionally recalcs the freshly
    // committed cross-origin iframe's author styles; but when no override is active there is nothing
    // to re-evaluate, and forcing updateLayout() on every newly-instrumented frame is a perf regression.
    if (m_overrides.emulatedMedia) {
        document->updateLayout();
        document->evaluateMediaQueriesAndReportChanges();
    }
}

} // namespace Inspector

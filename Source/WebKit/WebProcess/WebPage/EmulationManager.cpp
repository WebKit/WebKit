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
#include <WebCore/DocumentPage.h>
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
    else {
        m_overrides.emulatedMedia = media;
        // A frame can resolve style under this value before the next page-wide apply (the
        // InspectorPageAgent kick, or applyLocally(LocalFrame&) at enable), so a later clear must
        // not be skipped as a no-op.
        m_mayHaveAppliedEmulatedMedia = true;
    }
}

void EmulationManager::applyLocally()
{
    RefPtr page = m_backend->page();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    if (!m_overrides.emulatedMedia && !m_mayHaveAppliedEmulatedMedia)
        return;
    m_mayHaveAppliedEmulatedMedia = !!m_overrides.emulatedMedia;

    // Mirrors InspectorPageAgent::setEmulatedMedia's layout kick. See webkit.org/b/308898.
    // FIXME: Schedule a rendering update instead of synchronously updating the layout (bugs.webkit.org/b/308898).
    // Intentionally distinct from the frame-scoped applyLocally(LocalFrame&) below: this set-time path
    // is page-wide (updateStyleAfterChangeInEnvironment()), whereas the per-frame join path forces a
    // scheduleFullStyleRebuild() on a single freshly-instrumented document. Do not merge them -- a
    // page-wide full style rebuild on every emulation set would be a perf regression.
    corePage->updateStyleAfterChangeInEnvironment();
    corePage->forEachLocalFrame([&](WebCore::LocalFrame& frame) {
        // evaluateMediaQueriesAndReportChanges() dispatches change events synchronously, and a
        // listener can detach a frame later in this snapshot. Skip frames no longer in the page.
        if (!isFrameInPage(frame, *corePage))
            return;
        if (RefPtr document = frame.document()) {
            document->updateLayout();
            document->evaluateMediaQueriesAndReportChanges();
        }
    });
}

bool EmulationManager::isFrameInPage(WebCore::LocalFrame& frame, WebCore::Page& page)
{
    return frame.page() == &page && (frame.isMainFrame() || frame.tree().parent());
}

void EmulationManager::applyLocally(WebCore::LocalFrame& frame)
{
    // Frame-scoped, synchronous sibling of the page-wide applyLocally() above, for a frame that was
    // already committed when page instrumentation was enabled: its style was resolved before its
    // PageAgentProxy existed, so an override this process already holds has not reached it. With no
    // override there is nothing to re-evaluate, so enabling instrumentation costs nothing. Scoped to one
    // document (not corePage->updateStyleAfterChangeInEnvironment(), which is page-wide). See webkit.org/b/308898.
    if (!m_overrides.emulatedMedia)
        return;

    RefPtr document = frame.document();
    if (!document)
        return;

    m_mayHaveAppliedEmulatedMedia = true;
    document->scheduleFullStyleRebuild();
    document->updateLayout();
    document->evaluateMediaQueriesAndReportChanges();
}

} // namespace Inspector

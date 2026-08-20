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

#include <WebCore/EmulationOverrides.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebProcessProxy;
}

namespace Inspector {

// The typed, source-side (UIProcess) counterpart to the WebProcess-side EmulationManager sink.
// Holds the inspector emulation overrides for the inspected page as a strongly-typed
// Inspector::EmulationOverrides, and serializes them to every WebContent process that joins the
// inspected page under Site Isolation -- whether by a cross-origin frame spawn or a process swap --
// so a late-joining process inherits the same emulation config as the existing ones. Only
// emulatedMedia is materialized today. See webkit.org/b/308897.
class InspectorBackendSyncState {
public:
    // Update the emulated media type. An empty string clears the override, matching
    // Page.setEmulatedMedia("").
    void setEmulatedMedia(const String& media)
    {
        if (media.isEmpty())
            m_overrides.emulatedMedia = std::nullopt;
        else
            m_overrides.emulatedMedia = media;
    }

    void clear() { m_overrides = { }; }

    // Send the current typed overrides to a process that has just begun instrumentation.
    void sendTo(WebKit::WebProcessProxy&, WebCore::PageIdentifier) const;

private:
    Inspector::EmulationOverrides m_overrides;
};

} // namespace Inspector

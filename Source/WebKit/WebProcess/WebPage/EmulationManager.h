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

#include "WebInspectorBackend.h"
#include <WebCore/EmulationOverrides.h>
#include <wtf/CheckedRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class LocalFrame;
}

namespace Inspector {

// WebProcess-side holder for a page's emulation overrides. Owned by the WebInspectorBackend that
// created it and reaches the page through the backend. See webkit.org/b/308897.
class EmulationManager : public RefCounted<EmulationManager> {
    WTF_MAKE_TZONE_ALLOCATED(EmulationManager);
public:
    static Ref<EmulationManager> create(WebKit::WebInspectorBackend&);
    ~EmulationManager();

    // An empty string clears the override, matching Page.setEmulatedMedia("").
    void setEmulatedMedia(const String&);

    const EmulationOverrides& overrides() const { return m_overrides; }

    // Re-evaluate media queries on the owning page against the current overrides. The values reach
    // a frame via its PageAgentProxy::applyEmulatedMedia hook; this just forces the re-evaluation.
    void applyLocally();

    // Frame-scoped, synchronous sibling of applyLocally(): recalc author styles + re-evaluate media
    // queries for a single frame that just had page instrumentation installed under Site Isolation,
    // honoring the current overrides. See webkit.org/b/308896.
    void applyLocally(WebCore::LocalFrame&);

private:
    explicit EmulationManager(WebKit::WebInspectorBackend&);

    const CheckedRef<WebKit::WebInspectorBackend> m_backend;
    EmulationOverrides m_overrides;
};

} // namespace Inspector

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

#pragma once

#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class DOMStorageFrontendDispatcher;
}

namespace WebCore {

class LocalFrame;
class StorageArea;

// FrameDOMStorageAgent is the per-frame DOMStorage agent for Site Isolation. Each
// LocalFrame owns one, and it only ever serves its own frame's storage areas (which
// are the only ones reachable in this frame's process). This deliberately replaces the
// page-level InspectorDOMStorageAgent's origin-based frame-tree lookup, which cannot
// reach cross-origin (out-of-process RemoteFrame) storage under Site Isolation.
class FrameDOMStorageAgent final : public InspectorAgentBase, public Inspector::DOMStorageBackendDispatcherHandler, public CanMakeCheckedPtr<FrameDOMStorageAgent> {
    WTF_MAKE_NONCOPYABLE(FrameDOMStorageAgent);
    WTF_MAKE_TZONE_ALLOCATED(FrameDOMStorageAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameDOMStorageAgent);
public:
    explicit FrameDOMStorageAgent(FrameAgentContext&);
    ~FrameDOMStorageAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // DOMStorageBackendDispatcherHandler
    Inspector::CommandResult<void> enable() override;
    Inspector::CommandResult<void> disable() override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOMStorage::Item>>> getDOMStorageItems(Ref<JSON::Object>&& storageId) override;
    Inspector::CommandResult<void> setDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key, const String& value) override;
    Inspector::CommandResult<void> removeDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key) override;
    Inspector::CommandResult<void> clearDOMStorageItems(Ref<JSON::Object>&& storageId) override;

private:
    RefPtr<StorageArea> findStorageArea(Inspector::Protocol::ErrorString&, const JSON::Object& storageId, RefPtr<LocalFrame>& targetFrame);

    const UniqueRef<Inspector::DOMStorageFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::DOMStorageBackendDispatcher> m_backendDispatcher;

    WeakRef<LocalFrame> m_inspectedFrame;
};

} // namespace WebCore

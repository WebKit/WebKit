/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(REMOTE_INSPECTOR)
#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerContextData.h"
#include <JavaScriptCore/RemoteInspectionTarget.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class ServiceWorkerThreadProxy;

class ServiceWorkerDebuggable final : public Inspector::RemoteInspectionTarget {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ServiceWorkerDebuggable);
public:
    ServiceWorkerDebuggable(ServiceWorkerThreadProxy&, const ServiceWorkerContextData&);
    ~ServiceWorkerDebuggable() = default;

    Inspector::RemoteControllableTarget::Type type() const final { return Inspector::RemoteControllableTarget::Type::ServiceWorker; }

    String name() const final { return "ServiceWorker"_s; }
    String url() const final { return m_scopeURL; }
    bool hasLocalDebugger() const final { return false; }

    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false) final;
    void disconnect(Inspector::FrontendChannel&) final;
    void dispatchMessageFromRemote(const String& message) final;

private:
    ServiceWorkerThreadProxy& m_serviceWorkerThreadProxy;
    String m_scopeURL;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CONTROLLABLE_TARGET(WebCore::ServiceWorkerDebuggable, ServiceWorker);

#endif // ENABLE(SERVICE_WORKER)
#endif // ENABLE(REMOTE_INSPECTOR)

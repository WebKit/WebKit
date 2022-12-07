/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "TransferredMessagePort.h"
#include "WorkerGlobalScope.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class SharedWorkerThread;
struct WorkerParameters;

class SharedWorkerGlobalScope final : public WorkerGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(SharedWorkerGlobalScope);
public:
    template<typename... Args> static Ref<SharedWorkerGlobalScope> create(Args&&... args)
    {
        auto scope = adoptRef(*new SharedWorkerGlobalScope(std::forward<Args>(args)...));
        scope->addToContextsMap();
        return scope;
    }
    ~SharedWorkerGlobalScope();

    Type type() const final { return Type::SharedWorker; }
    const String& name() const { return m_name; }
    SharedWorkerThread& thread();

    void postConnectEvent(TransferredMessagePort&&, const String& sourceOrigin);

private:
    SharedWorkerGlobalScope(const String& name, const WorkerParameters&, Ref<SecurityOrigin>&&, SharedWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<WorkerClient>&&);

    EventTargetInterface eventTargetInterface() const final { return SharedWorkerGlobalScopeEventTargetInterfaceType; }
    FetchOptions::Destination destination() const final { return FetchOptions::Destination::Sharedworker; }

    String m_name;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SharedWorkerGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context) { return is<WebCore::WorkerGlobalScope>(context) && downcast<WebCore::WorkerGlobalScope>(context).type() == WebCore::WorkerGlobalScope::Type::SharedWorker; }
static bool isType(const WebCore::WorkerGlobalScope& context) { return context.type() == WebCore::WorkerGlobalScope::Type::SharedWorker; }
SPECIALIZE_TYPE_TRAITS_END()

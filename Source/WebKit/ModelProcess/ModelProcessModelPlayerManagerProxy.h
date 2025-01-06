/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)

#include "Connection.h"
#include "MessageReceiver.h"
#include "ModelConnectionToWebProcess.h"
#include "SharedPreferencesForWebProcess.h"
#include <WebCore/ModelPlayerIdentifier.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class ModelProcessModelPlayerProxy;

class ModelProcessModelPlayerManagerProxy
    : public RefCounted<ModelProcessModelPlayerManagerProxy>
    , public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(ModelProcessModelPlayerManagerProxy);
public:
    static Ref<ModelProcessModelPlayerManagerProxy> create(ModelConnectionToWebProcess& modelConnectionToWebProcess)
    {
        return adoptRef(*new ModelProcessModelPlayerManagerProxy(modelConnectionToWebProcess));
    }

    ~ModelProcessModelPlayerManagerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    ModelConnectionToWebProcess* modelConnectionToWebProcess() { return m_modelConnectionToWebProcess.get(); }
    void clear();

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);

private:
    explicit ModelProcessModelPlayerManagerProxy(ModelConnectionToWebProcess&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void createModelPlayer(WebCore::ModelPlayerIdentifier);
    void deleteModelPlayer(WebCore::ModelPlayerIdentifier);

    HashMap<WebCore::ModelPlayerIdentifier, Ref<ModelProcessModelPlayerProxy>> m_proxies;
    WeakPtr<ModelConnectionToWebProcess> m_modelConnectionToWebProcess;
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)

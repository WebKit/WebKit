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

#include "ModelProcessConnection.h"
#include <WebCore/ModelPlayerIdentifier.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class ModelPlayer;
class ModelPlayerClient;
}

namespace WebKit {

class WebPage;
class ModelProcessModelPlayer;

class ModelProcessModelPlayerManager
    : public ModelProcessConnection::Client
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ModelProcessModelPlayerManager> {
    WTF_MAKE_TZONE_ALLOCATED(ModelProcessModelPlayerManager);
public:
    static Ref<ModelProcessModelPlayerManager> create();
    ~ModelProcessModelPlayerManager();

    ModelProcessConnection& modelProcessConnection();

    Ref<ModelProcessModelPlayer> createModelProcessModelPlayer(WebPage&, WebCore::ModelPlayerClient&);
    void deleteModelProcessModelPlayer(WebCore::ModelPlayer&);

    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);

    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

private:
    ModelProcessModelPlayerManager();

    HashMap<WebCore::ModelPlayerIdentifier, WeakPtr<ModelProcessModelPlayer>> m_players;
    ThreadSafeWeakPtr<ModelProcessConnection> m_modelProcessConnection;
};

}

#endif // ENABLE(MODEL_PROCESS)

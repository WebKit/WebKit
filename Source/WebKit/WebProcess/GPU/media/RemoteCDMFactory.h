/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "RemoteCDMIdentifier.h"
#include "RemoteCDMInstanceIdentifier.h"
#include "RemoteCDMInstanceSessionIdentifier.h"
#include "WebProcessSupplement.h"
#include <WebCore/CDMFactory.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class Settings;
}

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class GPUProcessConnection;
class RemoteCDM;
class RemoteCDMInstanceSession;
class WebProcess;

class RemoteCDMFactory final
    : public WebCore::CDMFactory
    , public WebProcessSupplement
    , public CanMakeWeakPtr<RemoteCDMFactory> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteCDMFactory);
public:
    explicit RemoteCDMFactory(WebProcess&);
    virtual ~RemoteCDMFactory();

    void ref() const;
    void deref() const;

    static ASCIILiteral supplementName();

    GPUProcessConnection& gpuProcessConnection();

    void registerFactory(Vector<WebCore::CDMFactory*>&);

    void didReceiveSessionMessage(IPC::Connection&, IPC::Decoder&);

    void addSession(RemoteCDMInstanceSession&);
    void removeSession(RemoteCDMInstanceSessionIdentifier);

    void removeInstance(RemoteCDMInstanceIdentifier);

private:
    std::unique_ptr<WebCore::CDMPrivate> createCDM(const String&, const WebCore::CDMPrivateClient&) final;
    bool supportsKeySystem(const String&) final;

    WeakRef<WebProcess> m_webProcess;
    HashMap<RemoteCDMInstanceSessionIdentifier, WeakPtr<RemoteCDMInstanceSession>> m_sessions;
    HashMap<RemoteCDMIdentifier, std::unique_ptr<RemoteCDM>> m_cdms;
};

}

#endif

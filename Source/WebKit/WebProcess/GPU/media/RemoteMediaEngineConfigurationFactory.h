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

#if ENABLE(GPU_PROCESS)

#include "RemoteLegacyCDMIdentifier.h"
#include "RemoteLegacyCDMSessionIdentifier.h"
#include "WebProcessSupplement.h"
#include <WebCore/MediaEngineConfigurationFactory.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class GPUProcessConnection;
class WebProcess;

class RemoteMediaEngineConfigurationFactory final
    : public WebProcessSupplement
    , public CanMakeWeakPtr<RemoteMediaEngineConfigurationFactory> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteMediaEngineConfigurationFactory(WebProcess&);
    virtual ~RemoteMediaEngineConfigurationFactory();

    void registerFactory();

    static const char* supplementName();
    WebProcess& process() const { return m_process; }

    GPUProcessConnection& gpuProcessConnection();

    void didReceiveSessionMessage(IPC::Connection&, IPC::Decoder&);

private:
    void createDecodingConfiguration(WebCore::MediaDecodingConfiguration&&, WebCore::MediaEngineConfigurationFactory::DecodingConfigurationCallback&&);
    void createEncodingConfiguration(WebCore::MediaEncodingConfiguration&&, WebCore::MediaEngineConfigurationFactory::EncodingConfigurationCallback&&);

    WebProcess& m_process;
};

}

#endif

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

#include "config.h"
#include "RemoteMediaEngineConfigurationFactory.h"

#if ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "RemoteMediaEngineConfigurationFactoryProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/MediaCapabilitiesDecodingInfo.h>
#include <WebCore/MediaCapabilitiesEncodingInfo.h>
#include <WebCore/MediaDecodingConfiguration.h>
#include <WebCore/MediaEncodingConfiguration.h>
#include <WebCore/MediaEngineConfigurationFactory.h>

namespace WebKit {
using namespace WebCore;

RemoteMediaEngineConfigurationFactory::RemoteMediaEngineConfigurationFactory(WebProcess& process)
    : m_process(process)
{
}

RemoteMediaEngineConfigurationFactory::~RemoteMediaEngineConfigurationFactory() = default;

void RemoteMediaEngineConfigurationFactory::registerFactory()
{
    MediaEngineConfigurationFactory::clearFactories();

    auto createDecodingConfiguration = [weakThis = WeakPtr { *this }] (MediaDecodingConfiguration&& configuration, MediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback) {
        if (!weakThis) {
            callback({{ }, WTFMove(configuration)});
            return;
        }

        weakThis->createDecodingConfiguration(WTFMove(configuration), WTFMove(callback));
    };

#if PLATFORM(COCOA)
    MediaEngineConfigurationFactory::CreateEncodingConfiguration createEncodingConfiguration = nullptr;
#else
    auto createEncodingConfiguration = [weakThis = WeakPtr { *this }] (MediaEncodingConfiguration&& configuration, MediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback) {
        if (!weakThis) {
            callback({{ }, WTFMove(configuration)});
            return;
        }

        weakThis->createEncodingConfiguration(WTFMove(configuration), WTFMove(callback));
    };
#endif

    MediaEngineConfigurationFactory::installFactory({ WTFMove(createDecodingConfiguration), WTFMove(createEncodingConfiguration) });
}

const char* RemoteMediaEngineConfigurationFactory::supplementName()
{
    return "RemoteMediaEngineConfigurationFactory";
}

GPUProcessConnection& RemoteMediaEngineConfigurationFactory::gpuProcessConnection()
{
    return m_process.ensureGPUProcessConnection();
}

void RemoteMediaEngineConfigurationFactory::createDecodingConfiguration(MediaDecodingConfiguration&& configuration, MediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback)
{
    gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteMediaEngineConfigurationFactoryProxy::CreateDecodingConfiguration(WTFMove(configuration)), [callback = WTFMove(callback)] (MediaCapabilitiesDecodingInfo&& info) mutable {
        callback(WTFMove(info));
    });
}

void RemoteMediaEngineConfigurationFactory::createEncodingConfiguration(MediaEncodingConfiguration&& configuration, MediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback)
{
    gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteMediaEngineConfigurationFactoryProxy::CreateEncodingConfiguration(WTFMove(configuration)), [callback = WTFMove(callback)] (MediaCapabilitiesEncodingInfo&& info) mutable {
        callback(WTFMove(info));
    });
}

}

#endif // ENABLE(GPU_PROCESS)

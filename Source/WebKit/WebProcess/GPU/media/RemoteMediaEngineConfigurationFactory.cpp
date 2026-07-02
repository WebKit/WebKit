/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include <WebCore/PlatformMediaCapabilitiesDecodingInfo.h>
#include <WebCore/PlatformMediaCapabilitiesEncodingInfo.h>
#include <WebCore/PlatformMediaDecodingConfiguration.h>
#include <WebCore/PlatformMediaEncodingConfiguration.h>
#include <WebCore/PlatformMediaEngineConfigurationFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaEngineConfigurationFactory);

RemoteMediaEngineConfigurationFactory::RemoteMediaEngineConfigurationFactory(WebProcess& webProcess)
    : m_webProcess(webProcess)
{
}

RemoteMediaEngineConfigurationFactory::~RemoteMediaEngineConfigurationFactory() = default;

void RemoteMediaEngineConfigurationFactory::registerFactory()
{
    PlatformMediaEngineConfigurationFactory::clearFactories();

    auto createDecodingConfiguration = [weakThis = WeakPtr { *this }](PlatformMediaDecodingConfiguration&& configuration, PlatformMediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback) {
        if (!weakThis) {
            callback({{ }, WTF::move(configuration)});
            return;
        }

        weakThis->createDecodingConfiguration(WTF::move(configuration), WTF::move(callback));
    };

#if PLATFORM(COCOA)
    PlatformMediaEngineConfigurationFactory::CreateEncodingConfiguration createEncodingConfiguration = nullptr;
#else
    auto createEncodingConfiguration = [weakThis = WeakPtr { *this }](PlatformMediaEncodingConfiguration&& configuration, PlatformMediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback) {
        if (!weakThis) {
            callback({{ }, WTF::move(configuration)});
            return;
        }

        weakThis->createEncodingConfiguration(WTF::move(configuration), WTF::move(callback));
    };
#endif

    PlatformMediaEngineConfigurationFactory::installFactory({ WTF::move(createDecodingConfiguration), WTF::move(createEncodingConfiguration) });
}

ASCIILiteral RemoteMediaEngineConfigurationFactory::supplementName()
{
    return "RemoteMediaEngineConfigurationFactory"_s;
}

GPUProcessConnection& RemoteMediaEngineConfigurationFactory::gpuProcessConnection()
{
    return WebProcess::singleton().ensureGPUProcessConnection();
}

void RemoteMediaEngineConfigurationFactory::createDecodingConfiguration(PlatformMediaDecodingConfiguration&& configuration, PlatformMediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback)
{
    if (!m_webProcess->mediaPlaybackEnabled())
        return callback({ });

    gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteMediaEngineConfigurationFactoryProxy::CreateDecodingConfiguration(WTF::move(configuration)), [callback = WTF::move(callback)](PlatformMediaCapabilitiesDecodingInfo&& info) mutable {
        callback(WTF::move(info));
    });
}

void RemoteMediaEngineConfigurationFactory::createEncodingConfiguration(PlatformMediaEncodingConfiguration&& configuration, PlatformMediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback)
{
    if (!m_webProcess->mediaPlaybackEnabled())
        return callback({ });

    gpuProcessConnection().connection().sendWithAsyncReply(Messages::RemoteMediaEngineConfigurationFactoryProxy::CreateEncodingConfiguration(WTF::move(configuration)), [callback = WTF::move(callback)](PlatformMediaCapabilitiesEncodingInfo&& info) mutable {
        callback(WTF::move(info));
    });
}

}

#endif // ENABLE(GPU_PROCESS)

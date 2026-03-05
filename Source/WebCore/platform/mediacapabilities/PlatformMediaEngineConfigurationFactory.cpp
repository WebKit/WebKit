/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformMediaEngineConfigurationFactory.h"

#include "MediaSessionManagerInterface.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaCapabilitiesEncodingInfo.h"
#include "PlatformMediaDecodingConfiguration.h"
#include "PlatformMediaEncodingConfiguration.h"
#include "PlatformMediaEngineConfigurationFactoryMock.h"
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include "PlatformMediaEngineConfigurationFactoryCocoa.h"
#endif

#if USE(GSTREAMER)
#include "PlatformMediaEngineConfigurationFactoryGStreamer.h"
#endif

namespace WebCore {

static bool& mockEnabled()
{
    static bool enabled;
    return enabled;
}

using FactoryVector = Vector<PlatformMediaEngineConfigurationFactory::MediaEngineFactory>;
static FactoryVector defaultFactories()
{
    FactoryVector factories;
#if PLATFORM(COCOA)
    factories.append({ &createMediaPlayerDecodingConfigurationCocoa, nullptr });
#endif
#if USE(GSTREAMER)
    factories.append({ &createMediaPlayerDecodingConfigurationGStreamer, &createMediaPlayerEncodingConfigurationGStreamer });
#endif
    return factories;
}

static FactoryVector& factories()
{
    static NeverDestroyed factories = defaultFactories();
    return factories;
}

void PlatformMediaEngineConfigurationFactory::clearFactories()
{
    factories().clear();
}

void PlatformMediaEngineConfigurationFactory::resetFactories()
{
    factories() = defaultFactories();
}

void PlatformMediaEngineConfigurationFactory::installFactory(MediaEngineFactory&& factory)
{
    factories().append(WTF::move(factory));
}

bool PlatformMediaEngineConfigurationFactory::hasDecodingConfigurationFactory()
{
    return mockEnabled() || std::ranges::any_of(factories(), [](auto& factory) { return (bool)factory.createDecodingConfiguration; });
}

bool PlatformMediaEngineConfigurationFactory::hasEncodingConfigurationFactory()
{
    return mockEnabled() || std::ranges::any_of(factories(), [](auto& factory) { return (bool)factory.createEncodingConfiguration; });
}

void PlatformMediaEngineConfigurationFactory::createDecodingConfiguration(PlatformMediaDecodingConfiguration&& config, DecodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        PlatformMediaEngineConfigurationFactoryMock::createDecodingConfiguration(WTF::move(config), WTF::move(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, std::span<const MediaEngineFactory> nextFactories, PlatformMediaDecodingConfiguration&& config, DecodingConfigurationCallback&& callback) mutable {
        if (nextFactories.empty()) {
            callback({ { }, WTF::move(config) });
            return;
        }

        auto& factory = nextFactories[0];
        if (!factory.createDecodingConfiguration) {
            callback({ { }, WTF::move(config) });
            return;
        }

        factory.createDecodingConfiguration(WTF::move(config), [factoryCallback, nextFactories, config, callback = WTF::move(callback)] (PlatformMediaCapabilitiesDecodingInfo&& info) mutable {
            if (info.supported) {
                callback(WTF::move(info));
                return;
            }

            factoryCallback(factoryCallback, nextFactories.subspan(1), WTF::move(info.configuration), WTF::move(callback));
        });
    };
    factoryCallback(factoryCallback, factories().span(), WTF::move(config), WTF::move(callback));
}

void PlatformMediaEngineConfigurationFactory::createEncodingConfiguration(PlatformMediaEncodingConfiguration&& config, EncodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        PlatformMediaEngineConfigurationFactoryMock::createEncodingConfiguration(WTF::move(config), WTF::move(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, std::span<const MediaEngineFactory> nextFactories, PlatformMediaEncodingConfiguration&& config, EncodingConfigurationCallback&& callback) mutable {
        if (nextFactories.empty()) {
            callback(PlatformMediaCapabilitiesEncodingInfo { });
            return;
        }

        auto& factory = nextFactories[0];
        if (!factory.createEncodingConfiguration) {
            callback(PlatformMediaCapabilitiesEncodingInfo { });
            return;
        }

        factory.createEncodingConfiguration(WTF::move(config), [factoryCallback, nextFactories, callback = WTF::move(callback)] (auto&& info) mutable {
            if (info.supported) {
                callback(WTF::move(info));
                return;
            }

            factoryCallback(factoryCallback, nextFactories.subspan(1), WTF::move(info.configuration), WTF::move(callback));
        });
    };
    factoryCallback(factoryCallback, factories().span(), WTF::move(config), WTF::move(callback));
}

void PlatformMediaEngineConfigurationFactory::enableMock()
{
    mockEnabled() = true;
}

void PlatformMediaEngineConfigurationFactory::disableMock()
{
    mockEnabled() = false;
}

static PlatformMediaEngineConfigurationFactory::MediaSessionManagerProvider& mediaSessionManagerProvider()
{
    static NeverDestroyed<PlatformMediaEngineConfigurationFactory::MediaSessionManagerProvider> provider;
    return provider.get();
}

void PlatformMediaEngineConfigurationFactory::setMediaSessionManagerProvider(MediaSessionManagerProvider&& provider)
{
    mediaSessionManagerProvider() = WTF::move(provider);
}

RefPtr<MediaSessionManagerInterface> PlatformMediaEngineConfigurationFactory::mediaSessionManagerForPageIdentifier(PageIdentifier pageIdentifier)
{
    if (mediaSessionManagerProvider())
        return mediaSessionManagerProvider()(pageIdentifier);

    return nullptr;
}

}

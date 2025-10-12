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
#include "MediaEngineConfigurationFactory.h"

#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"
#include "MediaEngineConfigurationFactoryMock.h"
#include "MediaSessionManagerInterface.h"
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include "MediaEngineConfigurationFactoryCocoa.h"
#endif

#if USE(GSTREAMER)
#include "MediaEngineConfigurationFactoryGStreamer.h"
#endif

namespace WebCore {

static bool& mockEnabled()
{
    static bool enabled;
    return enabled;
}

using FactoryVector = Vector<MediaEngineConfigurationFactory::MediaEngineFactory>;
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

void MediaEngineConfigurationFactory::clearFactories()
{
    factories().clear();
}

void MediaEngineConfigurationFactory::resetFactories()
{
    factories() = defaultFactories();
}

void MediaEngineConfigurationFactory::installFactory(MediaEngineFactory&& factory)
{
    factories().append(WTFMove(factory));
}

bool MediaEngineConfigurationFactory::hasDecodingConfigurationFactory()
{
    return mockEnabled() || std::ranges::any_of(factories(), [](auto& factory) { return (bool)factory.createDecodingConfiguration; });
}

bool MediaEngineConfigurationFactory::hasEncodingConfigurationFactory()
{
    return mockEnabled() || std::ranges::any_of(factories(), [](auto& factory) { return (bool)factory.createEncodingConfiguration; });
}

void MediaEngineConfigurationFactory::createDecodingConfiguration(MediaDecodingConfiguration&& config, DecodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        MediaEngineConfigurationFactoryMock::createDecodingConfiguration(WTFMove(config), WTFMove(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, std::span<const MediaEngineFactory> nextFactories, MediaDecodingConfiguration&& config, DecodingConfigurationCallback&& callback) mutable {
        if (nextFactories.empty()) {
            callback({ { }, WTFMove(config) });
            return;
        }

        auto& factory = nextFactories[0];
        if (!factory.createDecodingConfiguration) {
            callback({ { }, WTFMove(config) });
            return;
        }

        factory.createDecodingConfiguration(WTFMove(config), [factoryCallback, nextFactories, config, callback = WTFMove(callback)] (MediaCapabilitiesDecodingInfo&& info) mutable {
            if (info.supported) {
                callback(WTFMove(info));
                return;
            }

            factoryCallback(factoryCallback, nextFactories.subspan(1), WTFMove(info.configuration), WTFMove(callback));
        });
    };
    factoryCallback(factoryCallback, factories().span(), WTFMove(config), WTFMove(callback));
}

void MediaEngineConfigurationFactory::createEncodingConfiguration(MediaEncodingConfiguration&& config, EncodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        MediaEngineConfigurationFactoryMock::createEncodingConfiguration(WTFMove(config), WTFMove(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, std::span<const MediaEngineFactory> nextFactories, MediaEncodingConfiguration&& config, EncodingConfigurationCallback&& callback) mutable {
        if (nextFactories.empty()) {
            callback({ });
            return;
        }

        auto& factory = nextFactories[0];
        if (!factory.createEncodingConfiguration) {
            callback({ });
            return;
        }

        factory.createEncodingConfiguration(WTFMove(config), [factoryCallback, nextFactories, callback = WTFMove(callback)] (auto&& info) mutable {
            if (info.supported) {
                callback(WTFMove(info));
                return;
            }

            factoryCallback(factoryCallback, nextFactories.subspan(1), WTFMove(info.configuration), WTFMove(callback));
        });
    };
    factoryCallback(factoryCallback, factories().span(), WTFMove(config), WTFMove(callback));
}

void MediaEngineConfigurationFactory::enableMock()
{
    mockEnabled() = true;
}

void MediaEngineConfigurationFactory::disableMock()
{
    mockEnabled() = false;
}

static MediaEngineConfigurationFactory::MediaSessionManagerProvider& mediaSessionManagerProvider()
{
    static NeverDestroyed<MediaEngineConfigurationFactory::MediaSessionManagerProvider> provider;
    return provider.get();
}

void MediaEngineConfigurationFactory::setMediaSessionManagerProvider(MediaSessionManagerProvider&& provider)
{
    mediaSessionManagerProvider() = WTFMove(provider);
}

RefPtr<MediaSessionManagerInterface> MediaEngineConfigurationFactory::mediaSessionManagerForPageIdentifier(PageIdentifier pageIdentifier)
{
    if (mediaSessionManagerProvider())
        return mediaSessionManagerProvider()(pageIdentifier);

    return nullptr;
}

}

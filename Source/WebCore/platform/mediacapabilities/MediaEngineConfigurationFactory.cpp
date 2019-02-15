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

#include "MediaCapabilitiesInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"
#include "MediaEngineConfigurationFactoryMock.h"
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

struct MediaEngineFactory {
    void(*createDecodingConfiguration)(MediaDecodingConfiguration&, MediaEngineConfigurationFactory::DecodingConfigurationCallback&&);
    void(*createEncodingConfiguration)(MediaEncodingConfiguration&, MediaEngineConfigurationFactory::EncodingConfigurationCallback&&);
};

using FactoryVector = Vector<MediaEngineFactory>;
static const FactoryVector& factories()
{
    static NeverDestroyed<FactoryVector> factories = makeNeverDestroyed(FactoryVector({
#if PLATFORM(COCOA)
        { &createMediaPlayerDecodingConfigurationCocoa, nullptr },
#endif
#if USE(GSTREAMER)
        { &createMediaPlayerDecodingConfigurationGStreamer, nullptr },
#endif
    }));
    return factories;
}

void MediaEngineConfigurationFactory::createDecodingConfiguration(MediaDecodingConfiguration&& config, MediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        MediaEngineConfigurationFactoryMock::createDecodingConfiguration(config, WTFMove(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, auto nextFactory, auto config, auto&& callback) mutable {
        if (nextFactory == factories().end()) {
            callback({ });
            return;
        }

        auto& factory = *nextFactory;
        if (!factory.createDecodingConfiguration) {
            callback({ });
            return;
        }

        factory.createDecodingConfiguration(config, [factoryCallback, nextFactory, config, callback = WTFMove(callback)] (auto&& info) mutable {
            if (info.supported) {
                callback(WTFMove(info));
                return;
            }

            factoryCallback(factoryCallback, ++nextFactory, config, WTFMove(callback));
        });
    };
    factoryCallback(factoryCallback, factories().begin(), config, WTFMove(callback));
}

void MediaEngineConfigurationFactory::createEncodingConfiguration(MediaEncodingConfiguration&& config, MediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback)
{
    if (mockEnabled()) {
        MediaEngineConfigurationFactoryMock::createEncodingConfiguration(config, WTFMove(callback));
        return;
    }

    auto factoryCallback = [] (auto factoryCallback, auto nextFactory, auto config, auto&& callback) mutable {
        if (nextFactory == factories().end()) {
            callback({ });
            return;
        }

        auto& factory = *nextFactory;
        if (!factory.createEncodingConfiguration) {
            callback({ });
            return;
        }

        factory.createEncodingConfiguration(config, [factoryCallback, nextFactory, config, callback = WTFMove(callback)] (auto&& info) mutable {
            if (info.supported) {
                callback(WTFMove(info));
                return;
            }

            factoryCallback(factoryCallback, ++nextFactory, config, WTFMove(callback));
        });
    };
    factoryCallback(factoryCallback, factories().begin(), config, WTFMove(callback));
}

void MediaEngineConfigurationFactory::enableMock()
{
    mockEnabled() = true;
}

void MediaEngineConfigurationFactory::disableMock()
{
    mockEnabled() = false;
}

}

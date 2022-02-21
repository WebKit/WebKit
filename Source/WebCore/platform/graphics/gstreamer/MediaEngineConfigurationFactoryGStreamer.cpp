/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "MediaEngineConfigurationFactoryGStreamer.h"

#if USE(GSTREAMER)

#include "GStreamerRegistryScanner.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"
#include "MediaPlayer.h"
#include <wtf/Function.h>

#if ENABLE(MEDIA_SOURCE)
#include "GStreamerRegistryScannerMSE.h"
#endif

namespace WebCore {

void createMediaPlayerDecodingConfigurationGStreamer(MediaDecodingConfiguration&& configuration, Function<void(MediaCapabilitiesDecodingInfo&&)>&& callback)
{
    bool isMediaSource = configuration.type == MediaDecodingType::MediaSource;
#if ENABLE(MEDIA_SOURCE)
    auto& scanner = isMediaSource ? GStreamerRegistryScannerMSE::singleton() : GStreamerRegistryScanner::singleton();
#else
    if (isMediaSource) {
        callback({{ }, WTFMove(configuration)});
        return;
    }
    auto& scanner = GStreamerRegistryScanner::singleton();
#endif
    auto lookupResult = scanner.isDecodingSupported(configuration);
    MediaCapabilitiesDecodingInfo info;
    info.supported = lookupResult.isSupported;
    info.powerEfficient = lookupResult.isUsingHardware;
    info.supportedConfiguration = WTFMove(configuration);

    callback(WTFMove(info));
}

void createMediaPlayerEncodingConfigurationGStreamer(MediaEncodingConfiguration&& configuration, Function<void(MediaCapabilitiesEncodingInfo&&)>&& callback)
{
    auto& scanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = scanner.isEncodingSupported(configuration);
    MediaCapabilitiesEncodingInfo info;
    info.supported = lookupResult.isSupported;
    info.powerEfficient = lookupResult.isUsingHardware;
    info.supportedConfiguration = WTFMove(configuration);

    callback(WTFMove(info));
}
}
#endif

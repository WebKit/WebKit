/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "MediaCapabilities.h"

#include "ContentType.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaCapabilitiesDecodingInfo.h"
#include "JSMediaCapabilitiesEncodingInfo.h"
#include "Logging.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaCapabilitiesLogging.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"
#include "MediaEngineConfigurationFactory.h"
#include "Settings.h"
#include <wtf/HashSet.h>
#include <wtf/Logger.h>

namespace WebCore {

static const HashSet<String>& bucketMIMETypes()
{
    // A "bucket" MIME types is one whose container type does not uniquely specify a codec.
    // See: https://tools.ietf.org/html/rfc6381
    static NeverDestroyed<HashSet<String>> bucketMIMETypes = HashSet<String>({
        "audio/3gpp",
        "video/3gpp",
        "audio/3gpp2",
        "video/3gpp2",
        "audio/mp4",
        "video/mp4",
        "application/mp4",
        "video/quicktime",
        "application/mp21",
        "audio/vnd.apple.mpegurl",
        "video/vnd.apple.mpegurl",
        "audio/ogg",
        "video/ogg",
        "video/webm",
        "audio/webm",
    });
    return bucketMIMETypes;
}

static bool isValidMediaMIMEType(const ContentType& contentType)
{
    // 2.1.4. MIME types
    // https://wicg.github.io/media-capabilities/#valid-media-mime-type
    // A valid media MIME type is a string that is a valid MIME type per [mimesniff]. If the MIME type does
    // not imply a codec, the string MUST also have one and only one parameter that is named codecs with a
    // value describing a single media codec. Otherwise, it MUST contain no parameters.
    if (contentType.isEmpty())
        return false;

    auto codecs = contentType.codecs();

    // FIXME: The spec requires that the "codecs" parameter is the only parameter present.
    if (bucketMIMETypes().contains(contentType.containerType()))
        return codecs.size() == 1;
    return !codecs.size();
}

static bool isValidVideoMIMEType(const ContentType& contentType)
{
    // 2.1.4 MIME Types
    // https://wicg.github.io/media-capabilities/#valid-video-mime-type
    // A valid video MIME type is a string that is a valid media MIME type and for which the type per [RFC7231]
    // is either video or application.
    if (!isValidMediaMIMEType(contentType))
        return false;

    auto containerType = contentType.containerType();
    if (!startsWithLettersIgnoringASCIICase(containerType, "video/") && !startsWithLettersIgnoringASCIICase(containerType, "application/"))
        return false;

    return true;
}

static bool isValidAudioMIMEType(const ContentType& contentType)
{
    // 2.1.4 MIME Types
    // https://wicg.github.io/media-capabilities/#valid-audio-mime-type
    // A valid audio MIME type is a string that is a valid media MIME type and for which the type per [RFC7231]
    // is either audio or application.
    if (!isValidMediaMIMEType(contentType))
        return false;

    auto containerType = contentType.containerType();
    if (!startsWithLettersIgnoringASCIICase(containerType, "audio/") && !startsWithLettersIgnoringASCIICase(containerType, "application/"))
        return false;

    return true;
}

static bool isValidVideoConfiguration(const VideoConfiguration& configuration)
{
    // 2.1.5. VideoConfiguration
    // https://wicg.github.io/media-capabilities/#valid-video-configuration
    // 1. If configuration’s contentType is not a valid video MIME type, return false and abort these steps.
    if (!isValidVideoMIMEType(ContentType(configuration.contentType)))
        return false;

    // 2. If none of the following is true, return false and abort these steps:
    //   o. Applying the rules for parsing floating-point number values to configuration’s framerate
    //      results in a number that is finite and greater than 0.
    if (!std::isfinite(configuration.framerate) || configuration.framerate <= 0)
        return false;

    // 3. Return true.
    return true;
}

static bool isValidAudioConfiguration(const AudioConfiguration& configuration)
{
    // 2.1.6. AudioConfiguration
    // https://wicg.github.io/media-capabilities/#audioconfiguration
    // 1. If configuration’s contentType is not a valid audio MIME type, return false and abort these steps.
    if (!isValidAudioMIMEType(ContentType(configuration.contentType)))
        return false;

    // 2. Return true.
    return true;
}

static bool isValidMediaConfiguration(const MediaConfiguration& configuration)
{
    // 2.1.1. MediaConfiguration
    // https://wicg.github.io/media-capabilities/#mediaconfiguration
    // For a MediaConfiguration to be a valid MediaConfiguration, audio or video MUST be present.
    if (!configuration.video && !configuration.audio)
        return false;

    if (configuration.video && !isValidVideoConfiguration(configuration.video.value()))
        return false;

    if (configuration.audio && !isValidAudioConfiguration(configuration.audio.value()))
        return false;

    return true;
}

void MediaCapabilities::decodingInfo(Document& document, MediaDecodingConfiguration&& configuration, Ref<DeferredPromise>&& promise)
{
    // 2.4 Media Capabilities Interface
    // https://wicg.github.io/media-capabilities/#media-capabilities-interface

    auto identifier = WTF::Logger::LogSiteIdentifier("MediaCapabilities", __func__, this);
    Ref<Logger> logger = document.logger();

    // 1. If configuration is not a valid MediaConfiguration, return a Promise rejected with a TypeError.
    // 2. If configuration.video is present and is not a valid video configuration, return a Promise rejected with a TypeError.
    // 3. If configuration.audio is present and is not a valid audio configuration, return a Promise rejected with a TypeError.
    if (!isValidMediaConfiguration(configuration)) {
#if !RELEASE_LOG_DISABLED
        logger->info(LogMedia, identifier, " - Rejected. configuration: ", configuration);
#endif
        promise->reject(TypeError);
        return;
    }

    if (!document.settings().mediaCapabilitiesExtensionsEnabled() && configuration.video)
        configuration.video.value().alphaChannel.reset();

    // 4. Let p be a new promise.
    // 5. In parallel, run the create a MediaCapabilitiesInfo algorithm with configuration and resolve p with its result.
    // 6. Return p.
    m_taskQueue.enqueueTask([configuration = WTFMove(configuration), promise = WTFMove(promise), logger = WTFMove(logger), identifier = WTFMove(identifier)] () mutable {

        // 2.2.3 If configuration is of type MediaDecodingConfiguration, run the following substeps:
        MediaEngineConfigurationFactory::DecodingConfigurationCallback callback = [promise = WTFMove(promise), logger = WTFMove(logger), identifier = WTFMove(identifier)] (auto info) mutable {
            // 2.2.3.1. If the user agent is able to decode the media represented by
            // configuration, set supported to true. Otherwise set it to false.
            // 2.2.3.2. If the user agent is able to decode the media represented by
            // configuration at a pace that allows a smooth playback, set smooth to
            // true. Otherwise set it to false.
            // 2.2.3.3. If the user agent is able to decode the media represented by
            // configuration in a power efficient manner, set powerEfficient to
            // true. Otherwise set it to false. The user agent SHOULD NOT take into
            // consideration the current power source in order to determine the
            // decoding power efficiency unless the device’s power source has side
            // effects such as enabling different decoding modules.
#if !RELEASE_LOG_DISABLED
            logger->info(LogMedia, identifier, "::callback() - Resolved. info: ", info);
#endif
            promise->resolve<IDLDictionary<MediaCapabilitiesDecodingInfo>>(WTFMove(info));
        };

        MediaEngineConfigurationFactory::createDecodingConfiguration(WTFMove(configuration), WTFMove(callback));
    });
}

void MediaCapabilities::encodingInfo(MediaEncodingConfiguration&& configuration, Ref<DeferredPromise>&& promise)
{
    // 2.4 Media Capabilities Interface
    // https://wicg.github.io/media-capabilities/#media-capabilities-interface

    // 1. If configuration is not a valid MediaConfiguration, return a Promise rejected with a TypeError.
    // 2. If configuration.video is present and is not a valid video configuration, return a Promise rejected with a TypeError.
    // 3. If configuration.audio is present and is not a valid audio configuration, return a Promise rejected with a TypeError.
    if (!isValidMediaConfiguration(configuration)) {
        promise->reject(TypeError);
        return;
    }

    // 4. Let p be a new promise.
    // 5. In parallel, run the create a MediaCapabilitiesInfo algorithm with configuration and resolve p with its result.
    // 6. Return p.
    m_taskQueue.enqueueTask([configuration = WTFMove(configuration), promise = WTFMove(promise)] () mutable {

        // 2.2.4. If configuration is of type MediaEncodingConfiguration, run the following substeps:
        MediaEngineConfigurationFactory::EncodingConfigurationCallback callback = [promise = WTFMove(promise)] (auto info) mutable {
            // 2.2.4.1. If the user agent is able to encode the media
            // represented by configuration, set supported to true. Otherwise
            // set it to false.
            // 2.2.4.2. If the user agent is able to encode the media
            // represented by configuration at a pace that allows encoding
            // frames at the same pace as they are sent to the encoder, set
            // smooth to true. Otherwise set it to false.
            // 2.2.4.3. If the user agent is able to encode the media
            // represented by configuration in a power efficient manner, set
            // powerEfficient to true. Otherwise set it to false. The user agent
            // SHOULD NOT take into consideration the current power source in
            // order to determine the encoding power efficiency unless the
            // device’s power source has side effects such as enabling different
            // encoding modules.
            promise->resolve<IDLDictionary<MediaCapabilitiesEncodingInfo>>(WTFMove(info));
        };

        MediaEngineConfigurationFactory::createEncodingConfiguration(WTFMove(configuration), WTFMove(callback));

    });
}

}

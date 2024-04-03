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
#include "EventLoop.h"
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
#include "Page.h"
#include "Settings.h"
#include "WebRTCProvider.h"
#include <wtf/Logger.h>
#include <wtf/SortedArrayMap.h>

namespace WebCore {

static bool isValidMediaMIMEType(const ContentType& contentType)
{
    // A "bucket" MIME types is one whose container type does not uniquely specify a codec.
    // See: https://tools.ietf.org/html/rfc6381
    static constexpr ComparableASCIILiteral bucketMIMETypeArray[] = {
        "application/mp21",
        "application/mp4",
        "audio/3gpp",
        "audio/3gpp2",
        "audio/mp4",
        "audio/ogg",
        "audio/vnd.apple.mpegurl",
        "audio/webm",
        "video/3gpp",
        "video/3gpp2",
        "video/mp4",
        "video/ogg",
        "video/quicktime",
        "video/vnd.apple.mpegurl",
        "video/webm",
    };
    static constexpr SortedArraySet bucketMIMETypes { bucketMIMETypeArray };

    // 2.1.4. MIME types
    // https://wicg.github.io/media-capabilities/#valid-media-mime-type
    // A valid media MIME type is a string that is a valid MIME type per [mimesniff]. If the MIME type does
    // not imply a codec, the string MUST also have one and only one parameter that is named codecs with a
    // value describing a single media codec. Otherwise, it MUST contain no parameters.
    if (contentType.isEmpty())
        return false;

    auto codecs = contentType.codecs();

    // FIXME: The spec requires that the "codecs" parameter is the only parameter present.
    if (bucketMIMETypes.contains(contentType.containerType()))
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
    if (!startsWithLettersIgnoringASCIICase(containerType, "video/"_s) && !startsWithLettersIgnoringASCIICase(containerType, "application/"_s))
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
    if (!startsWithLettersIgnoringASCIICase(containerType, "audio/"_s) && !startsWithLettersIgnoringASCIICase(containerType, "application/"_s))
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

static void gatherDecodingInfo(Document& document, MediaDecodingConfiguration&& configuration, MediaEngineConfigurationFactory::DecodingConfigurationCallback&& callback)
{
    RELEASE_LOG_INFO(Media, "Gathering decoding MediaCapabilities");
    MediaEngineConfigurationFactory::DecodingConfigurationCallback decodingCallback = [callback = WTFMove(callback)](MediaCapabilitiesDecodingInfo&& result) mutable {
        RELEASE_LOG_INFO(Media, "Finished gathering decoding MediaCapabilities");
        callback(WTFMove(result));
    };

    if (!document.settings().mediaCapabilitiesExtensionsEnabled() && configuration.video)
        configuration.video.value().alphaChannel.reset();

    configuration.allowedMediaContainerTypes = document.settings().allowedMediaContainerTypes();
    configuration.allowedMediaCodecTypes = document.settings().allowedMediaCodecTypes();

#if ENABLE(VP9)
    configuration.canExposeVP9 = document.settings().vp9DecoderEnabled();
#endif

#if ENABLE(WEB_RTC)
    if (configuration.type == MediaDecodingType::WebRTC) {
        if (auto* page = document.page())
            page->webRTCProvider().createDecodingConfiguration(WTFMove(configuration), WTFMove(decodingCallback));
        return;
    }
#endif
    MediaEngineConfigurationFactory::createDecodingConfiguration(WTFMove(configuration), WTFMove(decodingCallback));
}

static void gatherEncodingInfo(Document& document, MediaEncodingConfiguration&& configuration, MediaEngineConfigurationFactory::EncodingConfigurationCallback&& callback)
{
    RELEASE_LOG_INFO(Media, "Gathering encoding MediaCapabilities");
    MediaEngineConfigurationFactory::EncodingConfigurationCallback encodingCallback = [callback = WTFMove(callback)](auto&& result) mutable {
        RELEASE_LOG_INFO(Media, "Finished gathering encoding MediaCapabilities");
        callback(WTFMove(result));
    };

#if ENABLE(WEB_RTC)
    if (configuration.type == MediaEncodingType::WebRTC) {
        if (auto* page = document.page())
            page->webRTCProvider().createEncodingConfiguration(WTFMove(configuration), WTFMove(encodingCallback));
        return;
    }
#else
    UNUSED_PARAM(document);
#endif
    MediaEngineConfigurationFactory::createEncodingConfiguration(WTFMove(configuration), WTFMove(encodingCallback));
}

void MediaCapabilities::decodingInfo(ScriptExecutionContext& context, MediaDecodingConfiguration&& configuration, Ref<DeferredPromise>&& promise)
{
    // 2.4 Media Capabilities Interface
    // https://wicg.github.io/media-capabilities/#media-capabilities-interface

    // 1. If configuration is not a valid MediaConfiguration, return a Promise rejected with a TypeError.
    // 2. If configuration.video is present and is not a valid video configuration, return a Promise rejected with a TypeError.
    // 2.2.3 If configuration is of type MediaDecodingConfiguration, run the following substeps:
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
    // 3. If configuration.audio is present and is not a valid audio configuration, return a Promise rejected with a TypeError.
    if (!isValidMediaConfiguration(configuration)) {
        RELEASE_LOG_INFO(Media, "Invalid decoding media configuration");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 4. Let p be a new promise.
    // 5. In parallel, run the create a MediaCapabilitiesInfo algorithm with configuration and resolve p with its result.
    // 6. Return p.

    MediaEngineConfigurationFactory::DecodingConfigurationCallback callback = [promise = WTFMove(promise), context = Ref { context }](auto info) mutable {
        context->eventLoop().queueTask(TaskSource::MediaElement, [promise = WTFMove(promise), info = WTFMove(info)] () mutable {
            promise->resolve<IDLDictionary<MediaCapabilitiesDecodingInfo>>(WTFMove(info));
        });
    };

    if (RefPtr document = dynamicDowncast<Document>(context)) {
        gatherDecodingInfo(*document, WTFMove(configuration), WTFMove(callback));
        return;
    }

    m_decodingTasks.add(++m_nextTaskIdentifier, WTFMove(callback));
    context.postTaskToResponsibleDocument([configuration = WTFMove(configuration).isolatedCopy(), contextIdentifier = context.identifier(), weakThis = WeakPtr { this }, taskIdentifier = m_nextTaskIdentifier](auto& document) mutable {
        gatherDecodingInfo(document, WTFMove(configuration), [contextIdentifier, weakThis = WTFMove(weakThis), taskIdentifier](MediaCapabilitiesDecodingInfo&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis = WTFMove(weakThis), taskIdentifier, result = WTFMove(result).isolatedCopy()](auto&) mutable {
                if (!weakThis)
                    return;
                if (auto callback = weakThis->m_decodingTasks.take(taskIdentifier))
                    callback(WTFMove(result));
            });
        });
    });
}

void MediaCapabilities::encodingInfo(ScriptExecutionContext& context, MediaEncodingConfiguration&& configuration, Ref<DeferredPromise>&& promise)
{
    // 2.4 Media Capabilities Interface
    // https://wicg.github.io/media-capabilities/#media-capabilities-interface

    // 1. If configuration is not a valid MediaConfiguration, return a Promise rejected with a TypeError.
    // 2. If configuration.video is present and is not a valid video configuration, return a Promise rejected with a TypeError.
    // 3. If configuration.audio is present and is not a valid audio configuration, return a Promise rejected with a TypeError.
    // 2.2.4. If configuration is of type MediaEncodingConfiguration, run the following substeps:
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
    if (!isValidMediaConfiguration(configuration)) {
        RELEASE_LOG_INFO(Media, "Invalid encoding media configuration");
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    // 4. Let p be a new promise.
    // 5. In parallel, run the create a MediaCapabilitiesInfo algorithm with configuration and resolve p with its result.
    // 6. Return p.

    MediaEngineConfigurationFactory::EncodingConfigurationCallback callback = [promise = WTFMove(promise), context = Ref { context }](auto info) mutable {
        context->eventLoop().queueTask(TaskSource::MediaElement, [promise = WTFMove(promise), info = WTFMove(info)] () mutable {
            promise->resolve<IDLDictionary<MediaCapabilitiesEncodingInfo>>(WTFMove(info));
        });
    };

    if (RefPtr document = dynamicDowncast<Document>(context)) {
        gatherEncodingInfo(*document, WTFMove(configuration), WTFMove(callback));
        return;
    }

    m_encodingTasks.add(++m_nextTaskIdentifier, WTFMove(callback));
    context.postTaskToResponsibleDocument([configuration = WTFMove(configuration).isolatedCopy(), contextIdentifier = context.identifier(), weakThis = WeakPtr { this }, taskIdentifier = m_nextTaskIdentifier](auto& document) mutable {
        gatherEncodingInfo(document, WTFMove(configuration), [contextIdentifier, weakThis = WTFMove(weakThis), taskIdentifier](auto&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis = WTFMove(weakThis), taskIdentifier, result = WTFMove(result).isolatedCopy()](auto&) mutable {
                if (!weakThis)
                    return;
                if (auto callback = weakThis->m_encodingTasks.take(taskIdentifier))
                    callback(WTFMove(result));
            });
        });
    });
}

}

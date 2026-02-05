/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "PlatformMediaCapabilitiesLogging.h"

#include "PlatformMediaCapabilitiesAudioConfiguration.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaCapabilitiesEncodingInfo.h"
#include "PlatformMediaCapabilitiesVideoConfiguration.h"
#include "PlatformMediaDecodingConfiguration.h"
#include "PlatformMediaDecodingType.h"
#include "PlatformMediaEncodingConfiguration.h"
#include "PlatformMediaEncodingType.h"
#include <wtf/JSONValues.h>

namespace WebCore {

static String convertEnumerationToString(PlatformMediaCapabilitiesColorGamut value)
{
    switch (value) {
    case PlatformMediaCapabilitiesColorGamut::SRGB:    return "srgb"_s;
    case PlatformMediaCapabilitiesColorGamut::P3:      return "p3"_s;
    case PlatformMediaCapabilitiesColorGamut::Rec2020: return "rec2020"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();

}

static String convertEnumerationToString(PlatformMediaCapabilitiesHdrMetadataType value)
{
    switch (value) {
    case PlatformMediaCapabilitiesHdrMetadataType::SmpteSt2086:   return "smpteSt2086"_s;
    case PlatformMediaCapabilitiesHdrMetadataType::SmpteSt209410: return "smpteSt2094-10"_s;
    case PlatformMediaCapabilitiesHdrMetadataType::SmpteSt209440: return "smpteSt2094-40"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static String convertEnumerationToString(PlatformMediaCapabilitiesTransferFunction value)
{
    switch (value) {
    case PlatformMediaCapabilitiesTransferFunction::SRGB:    return "srgb"_s;
    case PlatformMediaCapabilitiesTransferFunction::PQ:      return "pq"_s;
    case PlatformMediaCapabilitiesTransferFunction::HLG:     return "hlg"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static String convertEnumerationToString(PlatformMediaDecodingType value)
{
    switch (value) {
    case PlatformMediaDecodingType::File:           return "file"_s;
    case PlatformMediaDecodingType::MediaSource:    return "media-source"_s;
    case PlatformMediaDecodingType::WebRTC:         return "webrtc"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static String convertEnumerationToString(PlatformMediaEncodingType value)
{
    switch (value) {
    case PlatformMediaEncodingType::Record: return "record"_s;
    case PlatformMediaEncodingType::WebRTC: return "webrtc"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    auto object = JSON::Object::create();
    object->setString("contentType"_s, configuration.contentType);
    object->setInteger("width"_s, configuration.width);
    object->setInteger("height"_s, configuration.height);
    object->setInteger("bitrate"_s, static_cast<int>(configuration.bitrate));
    object->setDouble("framerate"_s, configuration.framerate);
    if (configuration.alphaChannel)
        object->setBoolean("alphaChannel"_s, configuration.alphaChannel.value());
    if (configuration.colorGamut)
        object->setString("colorGamut"_s, convertEnumerationToString(configuration.colorGamut.value()));
    if (configuration.hdrMetadataType)
        object->setString("hdrMetadataType"_s, convertEnumerationToString(configuration.hdrMetadataType.value()));
    if (configuration.transferFunction)
        object->setString("transferFunction"_s, convertEnumerationToString(configuration.transferFunction.value()));
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaCapabilitiesAudioConfiguration& configuration)
{
    auto object = JSON::Object::create();
    object->setString("contentType"_s, configuration.contentType);
    if (!configuration.channels.isNull())
        object->setString("channels"_s, configuration.channels);
    if (configuration.bitrate)
        object->setInteger("bitrate"_s, static_cast<int>(configuration.bitrate.value()));
    if (configuration.samplerate)
        object->setDouble("samplerate"_s, configuration.samplerate.value());
    if (configuration.spatialRendering)
        object->setBoolean("spatialRendering"_s, configuration.spatialRendering.value());
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaConfiguration& configuration)
{
    auto object = JSON::Object::create();
    if (configuration.video)
        object->setValue("video"_s, toJSONObject(configuration.video.value()));
    if (configuration.audio)
        object->setValue("audio"_s, toJSONObject(configuration.audio.value()));
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaDecodingConfiguration& configuration)
{
    auto object = toJSONObject(static_cast<const PlatformMediaConfiguration&>(configuration));
    object->setString("type"_s, convertEnumerationToString(configuration.type));
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaEncodingConfiguration& configuration)
{
    auto object = toJSONObject(static_cast<const PlatformMediaConfiguration&>(configuration));
    object->setString("type"_s, convertEnumerationToString(configuration.type));
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaCapabilitiesInfo& info)
{
    auto object = JSON::Object::create();
    object->setBoolean("supported"_s, info.supported);
    object->setBoolean("smooth"_s, info.smooth);
    object->setBoolean("powerEfficient"_s, info.powerEfficient);
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaCapabilitiesDecodingInfo& info)
{
    auto object = toJSONObject(static_cast<const PlatformMediaCapabilitiesInfo&>(info));
    object->setValue("configuration"_s, toJSONObject(info.configuration));
    return object;
}

static Ref<JSON::Object> toJSONObject(const PlatformMediaCapabilitiesEncodingInfo& info)
{
    auto object = toJSONObject(static_cast<const PlatformMediaCapabilitiesInfo&>(info));
    object->setValue("configuration"_s, toJSONObject(info.configuration));
    return object;
}

static String toJSONString(const PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const PlatformMediaCapabilitiesAudioConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const PlatformMediaConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const PlatformMediaDecodingConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const PlatformMediaEncodingConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const PlatformMediaCapabilitiesInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

static String toJSONString(const PlatformMediaCapabilitiesDecodingInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

static String toJSONString(const PlatformMediaCapabilitiesEncodingInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

} // namespace WebCore

namespace WTF {

String LogArgument<WebCore::PlatformMediaCapabilitiesVideoConfiguration>::toString(const WebCore::PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesAudioConfiguration>::toString(const WebCore::PlatformMediaCapabilitiesAudioConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::PlatformMediaConfiguration>::toString(const WebCore::PlatformMediaConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::PlatformMediaDecodingConfiguration>::toString(const WebCore::PlatformMediaDecodingConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::PlatformMediaEncodingConfiguration>::toString(const WebCore::PlatformMediaEncodingConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesInfo>::toString(const WebCore::PlatformMediaCapabilitiesInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesDecodingInfo>::toString(const WebCore::PlatformMediaCapabilitiesDecodingInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesEncodingInfo>::toString(const WebCore::PlatformMediaCapabilitiesEncodingInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesColorGamut>::toString(const WebCore::PlatformMediaCapabilitiesColorGamut& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesHdrMetadataType>::toString(const WebCore::PlatformMediaCapabilitiesHdrMetadataType& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::PlatformMediaCapabilitiesTransferFunction>::toString(const WebCore::PlatformMediaCapabilitiesTransferFunction& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::PlatformMediaDecodingType>::toString(const WebCore::PlatformMediaDecodingType& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::PlatformMediaEncodingType>::toString(const WebCore::PlatformMediaEncodingType& type)
{
    return convertEnumerationToString(type);
}

}

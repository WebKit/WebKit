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
#include "MediaCapabilitiesLogging.h"

#include "AudioConfiguration.h"
#include "JSColorGamut.h"
#include "JSHdrMetadataType.h"
#include "JSMediaDecodingType.h"
#include "JSMediaEncodingType.h"
#include "JSTransferFunction.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaDecodingType.h"
#include "MediaEncodingConfiguration.h"
#include "MediaEncodingType.h"
#include "VideoConfiguration.h"
#include <wtf/JSONValues.h>

namespace WebCore {

static Ref<JSON::Object> toJSONObject(const VideoConfiguration& configuration)
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

static Ref<JSON::Object> toJSONObject(const AudioConfiguration& configuration)
{
    auto object = JSON::Object::create();
    object->setString("contentType"_s, configuration.contentType);
    object->setString("channels"_s, configuration.channels);
    object->setInteger("bitrate"_s, static_cast<int>(configuration.bitrate));
    object->setDouble("samplerate"_s, configuration.samplerate);
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaConfiguration& configuration)
{
    auto object = JSON::Object::create();
    if (configuration.video)
        object->setValue("video"_s, toJSONObject(configuration.video.value()));
    if (configuration.audio)
        object->setValue("audio"_s, toJSONObject(configuration.audio.value()));
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaDecodingConfiguration& configuration)
{
    auto object = toJSONObject(static_cast<const MediaConfiguration&>(configuration));
    object->setString("type"_s, convertEnumerationToString(configuration.type));
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaEncodingConfiguration& configuration)
{
    auto object = toJSONObject(static_cast<const MediaConfiguration&>(configuration));
    object->setString("type"_s, convertEnumerationToString(configuration.type));
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaCapabilitiesInfo& info)
{
    auto object = JSON::Object::create();
    object->setBoolean("supported"_s, info.supported);
    object->setBoolean("smooth"_s, info.smooth);
    object->setBoolean("powerEfficient"_s, info.powerEfficient);
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaCapabilitiesDecodingInfo& info)
{
    auto object = toJSONObject(static_cast<const MediaCapabilitiesInfo&>(info));
    object->setValue("supportedConfiguration"_s, toJSONObject(info.supportedConfiguration));
    return object;
}

static Ref<JSON::Object> toJSONObject(const MediaCapabilitiesEncodingInfo& info)
{
    auto object = toJSONObject(static_cast<const MediaCapabilitiesInfo&>(info));
    object->setValue("supportedConfiguration"_s, toJSONObject(info.supportedConfiguration));
    return object;
}

static String toJSONString(const VideoConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const AudioConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const MediaConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const MediaDecodingConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const MediaEncodingConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const MediaCapabilitiesInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

static String toJSONString(const MediaCapabilitiesDecodingInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

static String toJSONString(const MediaCapabilitiesEncodingInfo& info)
{
    return toJSONObject(info)->toJSONString();
}

}

namespace WTF {

String LogArgument<WebCore::VideoConfiguration>::toString(const WebCore::VideoConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::AudioConfiguration>::toString(const WebCore::AudioConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::MediaConfiguration>::toString(const WebCore::MediaConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::MediaDecodingConfiguration>::toString(const WebCore::MediaDecodingConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::MediaEncodingConfiguration>::toString(const WebCore::MediaEncodingConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::MediaCapabilitiesInfo>::toString(const WebCore::MediaCapabilitiesInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::MediaCapabilitiesDecodingInfo>::toString(const WebCore::MediaCapabilitiesDecodingInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::MediaCapabilitiesEncodingInfo>::toString(const WebCore::MediaCapabilitiesEncodingInfo& info)
{
    return toJSONString(info);
}

String LogArgument<WebCore::ColorGamut>::toString(const WebCore::ColorGamut& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::HdrMetadataType>::toString(const WebCore::HdrMetadataType& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::TransferFunction>::toString(const WebCore::TransferFunction& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::MediaDecodingType>::toString(const WebCore::MediaDecodingType& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::MediaEncodingType>::toString(const WebCore::MediaEncodingType& type)
{
    return convertEnumerationToString(type);
}

}

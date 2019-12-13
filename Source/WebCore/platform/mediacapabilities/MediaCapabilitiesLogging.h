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

#pragma once

#include <wtf/text/WTFString.h>

namespace WebCore {

struct VideoConfiguration;
struct AudioConfiguration;
struct MediaConfiguration;
struct MediaDecodingConfiguration;
struct MediaEncodingConfiguration;
struct MediaCapabilitiesInfo;
struct MediaCapabilitiesDecodingInfo;
struct MediaCapabilitiesEncodingInfo;

enum class ColorGamut;
enum class HdrMetadataType;
enum class TransferFunction;
enum class MediaDecodingType;
enum class MediaEncodingType;

}

namespace WTF {

template<typename>
struct LogArgument;

template <>
struct LogArgument<WebCore::VideoConfiguration> {
    static String toString(const WebCore::VideoConfiguration&);
};

template <>
struct LogArgument<WebCore::AudioConfiguration> {
    static String toString(const WebCore::AudioConfiguration&);
};

template <>
struct LogArgument<WebCore::MediaConfiguration> {
    static String toString(const WebCore::MediaConfiguration&);
};

template <>
struct LogArgument<WebCore::MediaDecodingConfiguration> {
    static String toString(const WebCore::MediaDecodingConfiguration&);
};

template <>
struct LogArgument<WebCore::MediaEncodingConfiguration> {
    static String toString(const WebCore::MediaEncodingConfiguration&);
};

template <>
struct LogArgument<WebCore::MediaCapabilitiesInfo> {
    static String toString(const WebCore::MediaCapabilitiesInfo&);
};

template <>
struct LogArgument<WebCore::MediaCapabilitiesDecodingInfo> {
    static String toString(const WebCore::MediaCapabilitiesDecodingInfo&);
};

template <>
struct LogArgument<WebCore::MediaCapabilitiesEncodingInfo> {
    static String toString(const WebCore::MediaCapabilitiesEncodingInfo&);
};

template <>
struct LogArgument<WebCore::ColorGamut> {
    static String toString(const WebCore::ColorGamut&);
};

template <>
struct LogArgument<WebCore::HdrMetadataType> {
    static String toString(const WebCore::HdrMetadataType&);
};

template <>
struct LogArgument<WebCore::TransferFunction> {
    static String toString(const WebCore::TransferFunction&);
};

template <>
struct LogArgument<WebCore::MediaDecodingType> {
    static String toString(const WebCore::MediaDecodingType&);
};

template <>
struct LogArgument<WebCore::MediaEncodingType> {
    static String toString(const WebCore::MediaEncodingType&);
};

}

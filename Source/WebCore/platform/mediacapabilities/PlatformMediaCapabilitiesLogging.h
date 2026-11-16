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

struct PlatformMediaCapabilitiesVideoConfiguration;
struct PlatformMediaCapabilitiesAudioConfiguration;
struct PlatformMediaConfiguration;
struct PlatformMediaDecodingConfiguration;
struct PlatformMediaEncodingConfiguration;
struct PlatformMediaCapabilitiesInfo;
struct PlatformMediaCapabilitiesDecodingInfo;
struct PlatformMediaCapabilitiesEncodingInfo;

enum class PlatformMediaCapabilitiesColorGamut : uint8_t;
enum class PlatformMediaCapabilitiesHdrMetadataType : uint8_t;
enum class PlatformMediaCapabilitiesTransferFunction : uint8_t;
enum class PlatformMediaDecodingType : uint8_t;
enum class PlatformMediaEncodingType : bool;

}

namespace WTF {

template<typename>
struct LogArgument;

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesVideoConfiguration> {
    static String toString(const WebCore::PlatformMediaCapabilitiesVideoConfiguration&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesAudioConfiguration> {
    static String toString(const WebCore::PlatformMediaCapabilitiesAudioConfiguration&);
};

template<>
struct LogArgument<WebCore::PlatformMediaConfiguration> {
    static String toString(const WebCore::PlatformMediaConfiguration&);
};

template<>
struct LogArgument<WebCore::PlatformMediaDecodingConfiguration> {
    static String toString(const WebCore::PlatformMediaDecodingConfiguration&);
};

template<>
struct LogArgument<WebCore::PlatformMediaEncodingConfiguration> {
    static String toString(const WebCore::PlatformMediaEncodingConfiguration&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesInfo> {
    static String toString(const WebCore::PlatformMediaCapabilitiesInfo&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesDecodingInfo> {
    static String toString(const WebCore::PlatformMediaCapabilitiesDecodingInfo&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesEncodingInfo> {
    static String toString(const WebCore::PlatformMediaCapabilitiesEncodingInfo&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesColorGamut> {
    static String toString(const WebCore::PlatformMediaCapabilitiesColorGamut&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesHdrMetadataType> {
    static String toString(const WebCore::PlatformMediaCapabilitiesHdrMetadataType&);
};

template<>
struct LogArgument<WebCore::PlatformMediaCapabilitiesTransferFunction> {
    static String toString(const WebCore::PlatformMediaCapabilitiesTransferFunction&);
};

template<>
struct LogArgument<WebCore::PlatformMediaDecodingType> {
    static String toString(const WebCore::PlatformMediaDecodingType&);
};

template<>
struct LogArgument<WebCore::PlatformMediaEncodingType> {
    static String toString(const WebCore::PlatformMediaEncodingType&);
};

}

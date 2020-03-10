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

#if ENABLE(ENCRYPTED_MEDIA)

#include <wtf/text/WTFString.h>

namespace WebCore {

struct CDMKeySystemConfiguration;
struct CDMMediaCapability;
struct CDMRestrictions;

enum class CDMEncryptionScheme : uint8_t;
enum class CDMKeyStatus : uint8_t;
enum class CDMMessageType : uint8_t;
enum class CDMRequirement : uint8_t;
enum class CDMSessionType : uint8_t;

}

namespace WTF {

template<typename>
struct LogArgument;

template <>
struct LogArgument<WebCore::CDMKeySystemConfiguration> {
    static String toString(const WebCore::CDMKeySystemConfiguration&);
};

template <>
struct LogArgument<WebCore::CDMMediaCapability> {
    static String toString(const WebCore::CDMMediaCapability&);
};

template <>
struct LogArgument<WebCore::CDMRestrictions> {
    static String toString(const WebCore::CDMRestrictions&);
};

template <>
struct LogArgument<WebCore::CDMEncryptionScheme> {
    static String toString(const WebCore::CDMEncryptionScheme&);
};

template <>
struct LogArgument<WebCore::CDMKeyStatus> {
    static String toString(const WebCore::CDMKeyStatus&);
};

template <>
struct LogArgument<WebCore::CDMMessageType> {
    static String toString(const WebCore::CDMMessageType&);
};

template <>
struct LogArgument<WebCore::CDMRequirement> {
    static String toString(const WebCore::CDMRequirement&);
};

template <>
struct LogArgument<WebCore::CDMSessionType> {
    static String toString(const WebCore::CDMSessionType&);
};

}

#endif // ENABLE(ENCRYPTED_MEDIA)

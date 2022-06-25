/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <optional>
#include <pal/SessionID.h>
#include <wtf/MonotonicTime.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSDictionary;

namespace WebCore {

enum class NotificationDirection : uint8_t;

struct NotificationData {
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<NotificationData> decode(Decoder&);

    NotificationData isolatedCopy() const &;
    NotificationData isolatedCopy() &&;

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static std::optional<NotificationData> fromDictionary(NSDictionary *dictionaryRepresentation);
    WEBCORE_EXPORT NSDictionary *dictionaryRepresentation() const;
#endif

    String title;
    String body;
    String iconURL;
    String tag;
    String language;
    WebCore::NotificationDirection direction;
    String originString;
    URL serviceWorkerRegistrationURL;
    UUID notificationID;
    PAL::SessionID sourceSession;
    MonotonicTime creationTime;
    Vector<uint8_t> data;
};

template<class Encoder>
void NotificationData::encode(Encoder& encoder) const
{
    encoder << title << body << iconURL << tag << language << direction << originString << serviceWorkerRegistrationURL << notificationID << sourceSession << creationTime << data;
}

template<class Decoder>
std::optional<NotificationData> NotificationData::decode(Decoder& decoder)
{
    std::optional<String> title;
    decoder >> title;
    if (!title)
        return std::nullopt;

    std::optional<String> body;
    decoder >> body;
    if (!body)
        return std::nullopt;

    std::optional<String> iconURL;
    decoder >> iconURL;
    if (!iconURL)
        return std::nullopt;

    std::optional<String> tag;
    decoder >> tag;
    if (!tag)
        return std::nullopt;

    std::optional<String> language;
    decoder >> language;
    if (!language)
        return std::nullopt;

    std::optional<WebCore::NotificationDirection> direction;
    decoder >> direction;
    if (!direction)
        return std::nullopt;

    std::optional<String> originString;
    decoder >> originString;
    if (!originString)
        return std::nullopt;

    std::optional<URL> serviceWorkerRegistrationURL;
    decoder >> serviceWorkerRegistrationURL;
    if (!serviceWorkerRegistrationURL)
        return std::nullopt;

    std::optional<UUID> notificationID;
    decoder >> notificationID;
    if (!notificationID)
        return std::nullopt;

    std::optional<PAL::SessionID> sourceSession;
    decoder >> sourceSession;
    if (!sourceSession)
        return std::nullopt;

    std::optional<MonotonicTime> creationTime;
    decoder >> creationTime;
    if (!creationTime)
        return std::nullopt;

    std::optional<Vector<uint8_t>> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    return { {
        WTFMove(*title),
        WTFMove(*body),
        WTFMove(*iconURL),
        WTFMove(*tag),
        WTFMove(*language),
        WTFMove(*direction),
        WTFMove(*originString),
        WTFMove(*serviceWorkerRegistrationURL),
        WTFMove(*notificationID),
        WTFMove(*sourceSession),
        WTFMove(*creationTime),
        WTFMove(*data)
    } };
}

} // namespace WebCore

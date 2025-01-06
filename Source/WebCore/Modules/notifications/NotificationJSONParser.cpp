/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "NotificationJSONParser.h"

#if ENABLE(DECLARATIVE_WEB_PUSH)

#include "JSDOMConvertEnumeration.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

static constexpr int expectedDeclarativeWebPushValue = 8030;
static constexpr ASCIILiteral declarativeWebPushKey { "web_push"_s };
static constexpr ASCIILiteral notificationKey { "notification"_s };
static constexpr ASCIILiteral titleKey { "title"_s };
static constexpr ASCIILiteral dirKey { "dir"_s };
static constexpr ASCIILiteral langKey { "lang"_s };
static constexpr ASCIILiteral bodyKey { "body"_s };
static constexpr ASCIILiteral navigationURLKey { "navigate"_s };
static constexpr ASCIILiteral tagKey { "tag"_s };
static constexpr ASCIILiteral iconKey { "icon"_s };
static constexpr ASCIILiteral silentKey { "silent"_s };
static constexpr ASCIILiteral dataKey { "data"_s };
static constexpr ASCIILiteral appBadgeKey { "app_badge"_s };
static constexpr ASCIILiteral mutableKey { "mutable"_s };

static std::optional<unsigned long long> parseAppBadgeString(const String& badgeString)
{
    // A valid app badge string contains only ASCII digits and represents a value equal to or less than std::numeric_limits<uint64_t>::max()

    return readCharactersForParsing(badgeString, [&](auto buffer) -> std::optional<unsigned long long> {
        CheckedUint64 result;
        while (buffer.hasCharactersRemaining()) {
            result *= 10u;

            if (!isASCIIDigit(*buffer))
                return std::nullopt;
            result += (unsigned)(*buffer - '0');
            if (result.hasOverflowed())
                return std::nullopt;

            buffer.advance();
        }

        return result.value();
    });
}

static bool hasDeclarativeMessageJSONValues(const JSON::Object& outerObject)
{
    std::optional<int> declarativeWebPushValue;
    if (auto value = outerObject.getValue(declarativeWebPushKey)) {
        if (value->type() == JSON::Value::Type::Integer || value->type() == JSON::Value::Type::Double)
            declarativeWebPushValue = value->asInteger();
    }
    if (!declarativeWebPushValue)
        return false;

    return *declarativeWebPushValue == expectedDeclarativeWebPushValue;
}

bool NotificationJSONParser::hasDeclarativeMessageHeader(const String& json)
{
    auto value = JSON::Value::parseJSON(json);
    if (!value)
        return false;

    auto object = value->asObject();
    if (!object)
        return false;

    return hasDeclarativeMessageJSONValues(*object);
}

ExceptionOr<NotificationPayload> NotificationJSONParser::parseNotificationPayload(const JSON::Object& outerObject)
{
    if (!hasDeclarativeMessageJSONValues(outerObject))
        return Exception { ExceptionCode::SyntaxError, makeString("Push message doesn't have valid declarative web push JSON values"_s) };

    auto notificationObject = outerObject.getValue(notificationKey);
    if (!notificationObject)
        return Exception { ExceptionCode::SyntaxError, makeString("Push message does not have '"_s, notificationKey, "' value"_s) };

    if (notificationObject->type() != JSON::Value::Type::Object)
        return Exception { ExceptionCode::SyntaxError, makeString("Push message '"_s, notificationKey, "' value is not the expected object type"_s) };

    RefPtr protectedObject = notificationObject->asObject();
    RELEASE_ASSERT(protectedObject);

    URL navigationURL;
    if (auto value = protectedObject->getValue(navigationURLKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, navigationURLKey, "' member is specified but is not a string"_s) };

        navigationURL = URL { value->asString() };
    }

    if (!navigationURL.isValid())
        return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, navigationURLKey, "' member is specified but does not represent a valid URL"_s) };

    String title;
    if (auto value = protectedObject->getValue(titleKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, titleKey, "' member is specified but is not a string"_s) };
        title = value->asString();
    }

    if (title.isEmpty())
        return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, titleKey, "' member is missing or is an empty string"_s) };

    std::optional<unsigned long long> appBadge;
    if (auto value = protectedObject->getValue(appBadgeKey)) {
        if (value->type() == JSON::Value::Type::String) {
            appBadge = parseAppBadgeString(value->asString());
            if (!appBadge)
                return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, appBadgeKey, "' member is specified as a string that did not parse to a valid unsigned long long"_s) };
        } else {
            if (value->type() != JSON::Value::Type::Integer && value->type() != JSON::Value::Type::Double)
                return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, appBadgeKey, "' member is specified but is not a string or a number"_s) };
            auto appBadgeInt = value->asInteger();
            if (!appBadgeInt || *appBadgeInt < 0)
                return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, appBadgeKey, "' member is specified as an number but is not a valid unsigned long long"_s) };
            appBadge = (unsigned long long)(*appBadgeInt);
        }
    }

    auto optionsOrException = parseNotificationOptions(*protectedObject);
    if (optionsOrException.hasException())
        return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, optionsOrException.exception().message()) };

    std::optional<NotificationOptionsPayload> notificationOptions = optionsOrException.releaseReturnValue();

    bool isMutable = false;
    if (auto value = protectedObject->getValue(mutableKey)) {
        if (value->type() != JSON::Value::Type::Boolean)
            return Exception { ExceptionCode::SyntaxError, makeString("Push message with Notification disposition: '"_s, mutableKey, "' member is specified but is not a boolean"_s) };
        isMutable = *(value->asBoolean());
    }

    return NotificationPayload { WTFMove(navigationURL), WTFMove(title), WTFMove(appBadge), WTFMove(notificationOptions), isMutable };
}

ExceptionOr<NotificationOptionsPayload> NotificationJSONParser::parseNotificationOptions(const JSON::Object& object)
{
    NotificationDirection direction = NotificationDirection::Auto;
    if (auto value = object.getValue(dirKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', dirKey, "' member is specified but is not a string"_s) };
        auto directionString = object.getString(dirKey);
        auto parsedDirection = parseEnumerationFromString<NotificationDirection>(directionString);
        if (parsedDirection == std::nullopt)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', dirKey, "' member is specified but is not a valid NotificationDirection"_s) };
        direction = *parsedDirection;
    }

    String lang;
    if (auto value = object.getValue(langKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', langKey, "' member is specified but is not a string"_s) };
        lang = value->asString();
    }

    String body;
    if (auto value = object.getValue(bodyKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', bodyKey, "' member is specified but is not a string"_s) };
        body = value->asString();
    }

    String tag;
    if (auto value = object.getValue(tagKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', tagKey, "' member is specified but is not a string"_s) };
        tag = value->asString();
    }

    URL iconURL;
    if (auto value = object.getValue(iconKey)) {
        if (value->type() != JSON::Value::Type::String)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', iconKey, "' member is specified but is not a string"_s) };

        iconURL = URL { value->asString() };
        if (!iconURL.isValid())
            return Exception { ExceptionCode::SyntaxError, makeString('\'', iconKey, "' member is specified but does not represent a valid URL"_s) };
    }

    std::optional<bool> silent;
    if (auto value = object.getValue(silentKey)) {
        if (value->type() != JSON::Value::Type::Boolean)
            return Exception { ExceptionCode::SyntaxError, makeString('\'', silentKey, "' member is specified but is not a boolean"_s) };
        silent = value->asBoolean();
    }

    RefPtr<JSON::Value> dataValue = object.getValue(dataKey);
    String dataString = dataValue ? dataValue->toJSONString() : nullString();

    return NotificationOptionsPayload { direction, WTFMove(lang), WTFMove(body), WTFMove(tag), iconURL.string(), WTFMove(dataString), WTFMove(silent) };
}

} // namespace WebCore

#endif // ENABLE(DECLARATIVE_WEB_PUSH)

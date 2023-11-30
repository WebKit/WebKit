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
#include "NotificationPayload.h"

#include "NotificationData.h"
#include "NotificationJSONParser.h"

#if ENABLE(DECLARATIVE_WEB_PUSH)

#include "Logging.h"

namespace WebCore {

ExceptionOr<NotificationPayload> NotificationPayload::parseJSON(const String& json)
{
    auto value = JSON::Value::parseJSON(json);
    if (!value)
        return Exception { ExceptionCode::SyntaxError, "Push message with Notification disposition: does not contain valid JSON"_s };

    auto object = value->asObject();
    if (!object)
        return Exception { ExceptionCode::SyntaxError, "Push message with Notification disposition: top level JSON value is not an object"_s };

    return NotificationJSONParser::parseNotificationPayload(*object);
}

NotificationPayload NotificationPayload::fromNotificationData(const NotificationData& data)
{
    NotificationOptionsPayload options { data.direction, data.language, data.body, data.tag, data.iconURL, { }, data.silent };

    return { data.defaultActionURL, data.title, std::nullopt, WTFMove(options), false };
}

} // namespace WebCore

#endif // ENABLE(DECLARATIVE_WEB_PUSH)

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
#include "WebPushMessage.h"

#include <WebCore/NotificationData.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {

#if ENABLE(DECLARATIVE_WEB_PUSH)

WebCore::NotificationData WebPushMessage::notificationPayloadToCoreData() const
{
    RELEASE_ASSERT(notificationPayload);

    static NeverDestroyed<WebCore::ScriptExecutionContextIdentifier> sharedScriptIdentifier = WebCore::ScriptExecutionContextIdentifier::generate();

    String body, iconURL, tag, language;
    auto direction = WebCore::NotificationDirection::Auto;
    std::optional<bool> silent;

    Vector<uint8_t> dataJSON;
    if (notificationPayload->options) {
        body = notificationPayload->options->body;
        language = notificationPayload->options->lang;
        tag = notificationPayload->options->tag;
        iconURL = notificationPayload->options->icon;
        direction = notificationPayload->options->dir;
        silent = notificationPayload->options->silent;

        CString dataCString = notificationPayload->options->dataJSONString.utf8();
        dataJSON = { dataCString.dataAsUInt8Ptr(), dataCString.length() };
    }

    return {
        notificationPayload->defaultActionURL,
        notificationPayload->title,
        WTFMove(body),
        WTFMove(iconURL),
        WTFMove(tag),
        WTFMove(language),
        direction,
        WebCore::SecurityOriginData::fromURL(registrationURL).toString(),
        registrationURL,
        WTF::UUID::createVersion4(),
        sharedScriptIdentifier,
        PAL::SessionID::defaultSessionID(),
        MonotonicTime::now(),
        WTFMove(dataJSON),
        WTFMove(silent)
    };
}

#endif // ENABLE(DECLARATIVE_WEB_PUSH)

} // namespace WebKit

/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaKeySystemRequest.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaKeySystemAccess.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaKeySystemController.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "Settings.h"
#include "WindowEventLoop.h"

namespace WebCore {

Ref<MediaKeySystemRequest> MediaKeySystemRequest::create(Document& document, const String& keySystem, Ref<DeferredPromise>&& promise)
{
    auto result = adoptRef(*new MediaKeySystemRequest(document, keySystem, WTFMove(promise)));
    result->suspendIfNeeded();
    return result;
}

MediaKeySystemRequest::MediaKeySystemRequest(Document& document, const String& keySystem, Ref<DeferredPromise>&& promise)
    : ActiveDOMObject(document)
    , m_identifier(MediaKeySystemRequestIdentifier::generate())
    , m_keySystem(keySystem)
    , m_promise(WTFMove(promise))
{
}

MediaKeySystemRequest::~MediaKeySystemRequest()
{
    if (m_allowCompletionHandler)
        m_allowCompletionHandler(WTFMove(m_promise));
}

SecurityOrigin* MediaKeySystemRequest::topLevelDocumentOrigin() const
{
    RefPtr context = scriptExecutionContext();
    return context ? &context->topOrigin() : nullptr;
}

void MediaKeySystemRequest::start()
{
    RefPtr context = scriptExecutionContext();
    ASSERT(context);
    if (!context) {
        deny();
        return;
    }

    auto& document = downcast<Document>(*context);
    auto* controller = MediaKeySystemController::from(document.page());
    if (!controller) {
        deny();
        return;
    }

    controller->requestMediaKeySystem(*this);
}

void MediaKeySystemRequest::allow()
{
    if (!scriptExecutionContext())
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [this] {
        if (auto allowCompletionHandler = std::exchange(m_allowCompletionHandler, { }))
            allowCompletionHandler(WTFMove(m_promise));
    });
}

void MediaKeySystemRequest::deny(const String& message)
{
    if (!scriptExecutionContext())
        return;

    ExceptionCode code = ExceptionCode::NotSupportedError;
    if (!message.isEmpty())
        m_promise->reject(code, message);
    else
        m_promise->reject(code);
}

void MediaKeySystemRequest::stop()
{
    auto& document = downcast<Document>(*scriptExecutionContext());
    if (auto* controller = MediaKeySystemController::from(document.page()))
        controller->cancelMediaKeySystemRequest(*this);
}

const char* MediaKeySystemRequest::activeDOMObjectName() const
{
    return "MediaKeySystemRequest";
}

Document* MediaKeySystemRequest::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

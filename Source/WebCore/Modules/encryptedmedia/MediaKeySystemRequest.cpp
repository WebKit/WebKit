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

#include "ContextDestructionObserverInlines.h"
#include "DocumentPage.h"
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

Ref<MediaKeySystemRequest> MediaKeySystemRequest::create(Document& document, const String& keySystem, RefPtr<DeferredPromise>&& promise)
{
    auto result = adoptRef(*new MediaKeySystemRequest(document, keySystem, WTF::move(promise)));
    result->suspendIfNeeded();
    return result;
}

MediaKeySystemRequest::MediaKeySystemRequest(Document& document, const String& keySystem, RefPtr<DeferredPromise>&& promise)
    : ActiveDOMObject(document)
    , m_keySystem(keySystem)
    , m_promise(WTF::move(promise))
{
}

MediaKeySystemRequest::~MediaKeySystemRequest()
{
    if (m_allowCompletionHandler)
        m_allowCompletionHandler({ }, WTF::move(m_promise));
}

SecurityOrigin* MediaKeySystemRequest::topLevelDocumentOrigin() const
{
    RefPtr context = scriptExecutionContext();
    return context ? &context->topOrigin() : nullptr;
}

void MediaKeySystemRequest::start()
{
    RefPtr document = this->document();
    ASSERT(document);
    if (!document) {
        deny();
        return;
    }

    auto* controller = MediaKeySystemController::from(protect(document->page()).get());
    if (!controller) {
        deny();
        return;
    }

    controller->requestMediaKeySystem(*this);
}

void MediaKeySystemRequest::allow(String&& mediaKeysHashSalt)
{
    if (!scriptExecutionContext())
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [mediaKeysHashSalt = WTF::move(mediaKeysHashSalt)](auto& request) mutable {
        if (auto allowCompletionHandler = std::exchange(request.m_allowCompletionHandler, { }))
            allowCompletionHandler(WTF::move(mediaKeysHashSalt), WTF::move(request.m_promise));
    });
}

void MediaKeySystemRequest::deny(const String& message)
{
    if (!scriptExecutionContext())
        return;

    RefPtr promise = m_promise;
    if (!promise)
        return;

    ExceptionCode code = ExceptionCode::NotSupportedError;
    if (!message.isEmpty())
        promise->reject(code, message);
    else
        promise->reject(code);
}

void MediaKeySystemRequest::stop()
{
    Ref document = *this->document();
    if (auto* controller = MediaKeySystemController::from(protect(document->page()).get()))
        controller->cancelMediaKeySystemRequest(*this);
}

Document* MediaKeySystemRequest::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

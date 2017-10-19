/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "MediaDevicesEnumerationRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Document.h"
#include "MainFrame.h"
#include "SecurityOrigin.h"
#include "UserMediaController.h"

namespace WebCore {

Ref<MediaDevicesEnumerationRequest> MediaDevicesEnumerationRequest::create(Document& document, CompletionHandler&& completionHandler)
{
    return adoptRef(*new MediaDevicesEnumerationRequest(document, WTFMove(completionHandler)));
}

MediaDevicesEnumerationRequest::MediaDevicesEnumerationRequest(ScriptExecutionContext& context, CompletionHandler&& completionHandler)
    : ContextDestructionObserver(&context)
    , m_completionHandler(WTFMove(completionHandler))
{
}

MediaDevicesEnumerationRequest::~MediaDevicesEnumerationRequest() = default;

SecurityOrigin* MediaDevicesEnumerationRequest::userMediaDocumentOrigin() const
{
    if (!scriptExecutionContext())
        return nullptr;

    return scriptExecutionContext()->securityOrigin();
}

SecurityOrigin* MediaDevicesEnumerationRequest::topLevelDocumentOrigin() const
{
    if (!scriptExecutionContext())
        return nullptr;

    return &scriptExecutionContext()->topOrigin();
}

void MediaDevicesEnumerationRequest::contextDestroyed()
{
    // Calling cancel() may destroy ourselves.
    Ref<MediaDevicesEnumerationRequest> protectedThis(*this);

    cancel();
    ContextDestructionObserver::contextDestroyed();
}

void MediaDevicesEnumerationRequest::start()
{
    ASSERT(scriptExecutionContext());

    auto& document = downcast<Document>(*scriptExecutionContext());
    auto* controller = UserMediaController::from(document.page());
    if (!controller)
        return;

    Ref<MediaDevicesEnumerationRequest> protectedThis(*this);
    controller->enumerateMediaDevices(*this);
}

void MediaDevicesEnumerationRequest::cancel()
{
    m_completionHandler = nullptr;
}

void MediaDevicesEnumerationRequest::setDeviceInfo(const Vector<CaptureDevice>& deviceList, const String& deviceIdentifierHashSalt, bool originHasPersistentAccess)
{
    if (m_completionHandler)
        m_completionHandler(deviceList, deviceIdentifierHashSalt, originHasPersistentAccess);
    m_completionHandler = nullptr;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

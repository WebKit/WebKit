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
#include "MediaKeySystemController.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "Document.h"
#include "FeaturePolicy.h"
#include "HTMLIFrameElement.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediaKeySystemRequest.h"
#include "Page.h"

namespace WebCore {

ASCIILiteral MediaKeySystemController::supplementName()
{
    return "MediaKeySystemController"_s;
}

MediaKeySystemController* MediaKeySystemController::from(Page* page)
{
    return static_cast<MediaKeySystemController*>(Supplement<Page>::from(page, MediaKeySystemController::supplementName()));
}

MediaKeySystemController::MediaKeySystemController(MediaKeySystemClient& client)
    : m_client(client)
{
}

MediaKeySystemController::~MediaKeySystemController()
{
    if (m_client)
        m_client->pageDestroyed();
}

void provideMediaKeySystemTo(Page& page, MediaKeySystemClient& client)
{
    MediaKeySystemController::provideTo(&page, MediaKeySystemController::supplementName(), makeUnique<MediaKeySystemController>(client));
}

void MediaKeySystemController::logRequestMediaKeySystemDenial(Document& document)
{
    if (RefPtr window = document.domWindow())
        window->printErrorMessage(makeString("Not allowed to access MediaKeySystem."));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "UserMediaController.h"

#if ENABLE(MEDIA_STREAM)

#include "DOMWindow.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "HTMLIFrameElement.h"
#include "HTMLParserIdioms.h"
#include "SchemeRegistry.h"
#include "UserMediaRequest.h"

namespace WebCore {

const char* UserMediaController::supplementName()
{
    return "UserMediaController";
}

UserMediaController::UserMediaController(UserMediaClient* client)
    : m_client(client)
{
}

UserMediaController::~UserMediaController()
{
    m_client->pageDestroyed();
}

void provideUserMediaTo(Page* page, UserMediaClient* client)
{
    UserMediaController::provideTo(page, UserMediaController::supplementName(), std::make_unique<UserMediaController>(client));
}

static bool isSecure(DocumentLoader& documentLoader)
{
    auto& response = documentLoader.response();
    if (SecurityOrigin::isLocalHostOrLoopbackIPAddress(documentLoader.response().url().host()))
        return true;
    return SchemeRegistry::shouldTreatURLSchemeAsSecure(response.url().protocol().toStringWithoutCopying())
        && response.certificateInfo()
        && !response.certificateInfo()->containsNonRootSHA1SignedCertificate();
}

static UserMediaController::GetUserMediaAccess isAllowedToUse(Document& document, Document& topDocument, OptionSet<UserMediaController::CaptureType> types)
{
    if (&document == &topDocument)
        return UserMediaController::GetUserMediaAccess::CanCall;

    auto* parentDocument = document.parentDocument();
    if (!parentDocument)
        return UserMediaController::GetUserMediaAccess::BlockedByParent;

    if (document.securityOrigin().isSameSchemeHostPort(parentDocument->securityOrigin()))
        return UserMediaController::GetUserMediaAccess::CanCall;

    auto* element = document.ownerElement();
    ASSERT(element);
    if (!element)
        return UserMediaController::GetUserMediaAccess::BlockedByParent;

    if (!is<HTMLIFrameElement>(*element))
        return UserMediaController::GetUserMediaAccess::BlockedByParent;
    auto& allow = downcast<HTMLIFrameElement>(*element).allow();

    bool allowCameraAccess = false;
    bool allowMicrophoneAccess = false;
    bool allowDisplay = false;
    for (auto allowItem : StringView { allow }.split(';')) {
        auto item = allowItem.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
        if (!allowCameraAccess && item == "camera")
            allowCameraAccess = true;
        else if (!allowMicrophoneAccess && item == "microphone")
            allowMicrophoneAccess = true;
        else if (!allowDisplay && item == "display")
            allowDisplay = true;
    }
    if ((allowCameraAccess || !(types & UserMediaController::CaptureType::Camera)) && (allowMicrophoneAccess || !(types & UserMediaController::CaptureType::Microphone)) && (allowDisplay || !(types & UserMediaController::CaptureType::Display)))
        return UserMediaController::GetUserMediaAccess::CanCall;

    return UserMediaController::GetUserMediaAccess::BlockedByFeaturePolicy;
}

UserMediaController::GetUserMediaAccess UserMediaController::canCallGetUserMedia(Document& document, OptionSet<UserMediaController::CaptureType> types)
{
    ASSERT(!types.isEmpty());

    bool requiresSecureConnection = DeprecatedGlobalSettings::mediaCaptureRequiresSecureConnection();
    auto& documentLoader = *document.loader();
    if (requiresSecureConnection && !isSecure(documentLoader))
        return GetUserMediaAccess::InsecureDocument;

    auto& topDocument = document.topDocument();
    if (&document != &topDocument) {
        for (auto* ancestorDocument = &document; ancestorDocument != &topDocument; ancestorDocument = ancestorDocument->parentDocument()) {
            if (requiresSecureConnection && !isSecure(*ancestorDocument->loader()))
                return GetUserMediaAccess::InsecureParent;

            auto status = isAllowedToUse(*ancestorDocument, topDocument, types);
            if (status != GetUserMediaAccess::CanCall)
                return status;
        }
    }

    return GetUserMediaAccess::CanCall;
}

void UserMediaController::logGetUserMediaDenial(Document& document, GetUserMediaAccess access, BlockedCaller caller)
{
    auto& domWindow = *document.domWindow();
    const char* callerName;

    switch (caller) {
    case BlockedCaller::GetUserMedia:
        callerName = "getUserMedia";
        break;
    case BlockedCaller::GetDisplayMedia:
        callerName = "getDisplayMedia";
        break;
    case BlockedCaller::EnumerateDevices:
        callerName = "enumerateDevices";
        break;
    }

    switch (access) {
    case UserMediaController::GetUserMediaAccess::InsecureDocument:
        domWindow.printErrorMessage(makeString("Trying to call ", callerName, " from an insecure document."));
        break;
    case UserMediaController::GetUserMediaAccess::InsecureParent:
        domWindow.printErrorMessage(makeString("Trying to call ", callerName, " from a document with an insecure parent frame."));
        break;
    case UserMediaController::GetUserMediaAccess::BlockedByParent:
        domWindow.printErrorMessage(makeString("The top-level frame has prevented a document with a different security origin from calling ", callerName, "."));
        break;
    case GetUserMediaAccess::BlockedByFeaturePolicy:
        domWindow.printErrorMessage(makeString("Trying to call ", callerName, " from a frame without correct 'allow' attribute."));
        break;
    case UserMediaController::GetUserMediaAccess::CanCall:
        break;
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

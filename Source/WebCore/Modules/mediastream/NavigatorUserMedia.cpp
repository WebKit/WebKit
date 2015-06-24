/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "NavigatorUserMedia.h"

#if ENABLE(MEDIA_STREAM)

#include "Dictionary.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "MediaStream.h"
#include "Navigator.h"
#include "NavigatorUserMediaError.h"
#include "NavigatorUserMediaErrorCallback.h"
#include "NavigatorUserMediaSuccessCallback.h"
#include "Page.h"
#include "UserMediaController.h"
#include "UserMediaRequest.h"

namespace WebCore {

NavigatorUserMedia::NavigatorUserMedia()
{
}

NavigatorUserMedia::~NavigatorUserMedia()
{
}

void NavigatorUserMedia::webkitGetUserMedia(Navigator* navigator, const Dictionary& options, PassRefPtr<NavigatorUserMediaSuccessCallback> successCallback, PassRefPtr<NavigatorUserMediaErrorCallback> errorCallback, ExceptionCode& ec)
{
    // FIXME: Remove this test once IDL is updated to make errroCallback parameter mandatory.
    if (!successCallback || !errorCallback) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    // We do not need to protect the context (i.e. document) here as UserMediaRequest is observing context destruction and will check validity before resolving/rejecting promise.
    Document* document = navigator->frame()->document();

    ASSERT(errorCallback);
    ASSERT(successCallback);
    auto resolveCallback = [successCallback, document](const RefPtr<MediaStream>& stream) mutable {
        RefPtr<MediaStream> protectedStream = stream;
        document->postTask([successCallback, protectedStream](ScriptExecutionContext&) {
            successCallback->handleEvent(protectedStream.get());
        });
    };
    auto rejectCallback = [errorCallback, document](const RefPtr<NavigatorUserMediaError>& error) mutable {
        RefPtr<NavigatorUserMediaError> protectedError = error;
        document->postTask([errorCallback, protectedError](ScriptExecutionContext&) {
            errorCallback->handleEvent(protectedError.get());
        });
    };

    UserMediaRequest::start(navigator->frame()->document(), options, MediaDevices::Promise(WTF::move(resolveCallback), WTF::move(rejectCallback)), ec);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "NavigatorIntents.h"

#if ENABLE(WEB_INTENTS)

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "Intent.h"
#include "IntentRequest.h"
#include "IntentResultCallback.h"
#include "Navigator.h"
#include "ScriptController.h"

namespace WebCore {

NavigatorIntents::NavigatorIntents()
{
}

NavigatorIntents::~NavigatorIntents()
{
}

void NavigatorIntents::startActivity(Navigator* navigator,
                                     PassRefPtr<Intent> intent,
                                     PassRefPtr<IntentResultCallback> successCallback,
                                     PassRefPtr<IntentResultCallback> errorCallback,
                                     ExceptionCode& ec)
{
    if (!navigator->frame() || !intent) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (intent->action().isEmpty() || intent->type().isEmpty()) {
        ec = VALIDATION_ERR;
        return;
    }

    if (!ScriptController::processingUserGesture()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    RefPtr<IntentRequest> request = IntentRequest::create(navigator->frame()->document(),
                                                          intent, successCallback, errorCallback);
    navigator->frame()->loader()->client()->dispatchIntent(request);
}

} // namespace WebCore

#endif // ENABLE(WEB_INTENTS)

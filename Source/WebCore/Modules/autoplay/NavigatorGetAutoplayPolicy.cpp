/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "NavigatorGetAutoplayPolicy.h"

#include "AudioContext.h"
#include "AutoplayPolicy.h"
#include "AutoplayPolicyMediaType.h"
#include "HTMLMediaElement.h"
#include "MediaElementSession.h"

namespace WebCore {

AutoplayPolicy NavigatorGetAutoplayPolicy::getAutoplayPolicy(Navigator& navigator, AutoplayPolicyMediaType type)
{
    RefPtr document = navigator.document();
    if (!document)
        return AutoplayPolicy::Disallowed;

    RefPtr page = document->page();
    if (!page)
        return AutoplayPolicy::Disallowed;

    if (document->processingUserGestureForMedia())
        return AutoplayPolicy::Allowed;

    if (type == AutoplayPolicyMediaType::Mediaelement && page->requiresUserGestureForVideoPlayback())
        return AutoplayPolicy::Disallowed;

    if (page->requiresUserGestureForAudioPlayback())
        return AutoplayPolicy::AllowedMuted;

    return AutoplayPolicy::Allowed;
}

AutoplayPolicy NavigatorGetAutoplayPolicy::getAutoplayPolicy(Navigator&, const HTMLMediaElement& element)
{
    if (RefPtr session = element.mediaSessionIfExists())
        return session->getAutoplayPolicy();
    return AutoplayPolicy::Disallowed;
}

AutoplayPolicy NavigatorGetAutoplayPolicy::getAutoplayPolicy(Navigator&, const AudioContext& context)
{
    return context.getAutoplayPolicy();
}

}

/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WKMediaSessionFocusManager.h"

#include "WKAPICast.h"
#include "WebMediaSessionFocusManager.h"

using namespace WebKit;

WKTypeID WKMediaSessionFocusManagerGetTypeID()
{
#if ENABLE(MEDIA_SESSION)
    return toAPI(WebMediaSessionFocusManager::APIType);
#else
    return toAPI(API::Object::Type::Null);
#endif
}

void WKMediaSessionFocusManagerSetClient(WKMediaSessionFocusManagerRef manager, const WKMediaSessionFocusManagerClientBase* client)
{
#if ENABLE(MEDIA_SESSION)
    toImpl(manager)->initializeClient(client);
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(client);
#endif
}

bool WKMediaSessionFocusManagerValueForPlaybackAttribute(WKMediaSessionFocusManagerRef manager, WKMediaSessionFocusManagerPlaybackAttribute attribute)
{
#if ENABLE(MEDIA_SESSION)
    return toImpl(manager)->valueForPlaybackAttribute(attribute);
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(attribute);
    return false;
#endif
}

void WKMediaSessionFocusManagerSetVolumeOfFocusedMediaElement(WKMediaSessionFocusManagerRef manager, double volume)
{
#if ENABLE(MEDIA_SESSION)
    toImpl(manager)->setVolumeOfFocusedMediaElement(volume);
#else
    UNUSED_PARAM(manager);
    UNUSED_PARAM(volume);
#endif
}

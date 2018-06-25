/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WKMockMediaDevice.h"

#include "WKAPICast.h"
#include "WKString.h"
#include "WebProcessPool.h"
#include <WebCore/MockMediaDevice.h>

using namespace WebKit;

void WKAddMockMediaDevice(WKContextRef context, WKStringRef persistentId, WKStringRef label, WKStringRef type)
{
#if ENABLE(MEDIA_STREAM)
    String typeString = WebKit::toImpl(type)->string();
    Variant<WebCore::MockMicrophoneProperties, WebCore::MockCameraProperties, WebCore::MockDisplayProperties> properties;
    if (typeString == "camera")
        properties = WebCore::MockCameraProperties { };
    else if (typeString == "screen")
        properties = WebCore::MockDisplayProperties { };
    else if (typeString != "microphone")
        return;

    toImpl(context)->addMockMediaDevice({ WebKit::toImpl(persistentId)->string(), WebKit::toImpl(label)->string(), WTFMove(properties) });
#endif
}

void WKClearMockMediaDevices(WKContextRef context)
{
    toImpl(context)->clearMockMediaDevices();
}

void WKRemoveMockMediaDevice(WKContextRef context, WKStringRef persistentId)
{
    toImpl(context)->removeMockMediaDevice(WebKit::toImpl(persistentId)->string());
}

void WKResetMockMediaDevices(WKContextRef context)
{
    toImpl(context)->resetMockMediaDevices();
}

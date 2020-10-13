/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebProcessProxy.h"

#if PLATFORM(IOS_FAMILY)

#import "AccessibilitySupportSPI.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKMouseDeviceObserver.h"
#import "WKStylusDeviceObserver.h"
#import "WebProcessMessages.h"

namespace WebKit {

void WebProcessProxy::platformInitialize()
{
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)
    [[WKMouseDeviceObserver sharedInstance] start];
#endif
#if HAVE(PENCILKIT_TEXT_INPUT)
    [[WKStylusDeviceObserver sharedInstance] start];
#endif
}

void WebProcessProxy::platformDestroy()
{
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)
    [[WKMouseDeviceObserver sharedInstance] stop];
#endif
#if HAVE(PENCILKIT_TEXT_INPUT)
    [[WKStylusDeviceObserver sharedInstance] stop];
#endif
}

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) && PLATFORM(IOS)

void WebProcessProxy::notifyHasMouseDeviceChanged()
{
    bool hasMouseDevice = [[WKMouseDeviceObserver sharedInstance] hasMouseDevice];
    for (auto* webProcessProxy : WebProcessProxy::allProcesses().values())
        webProcessProxy->send(Messages::WebProcess::SetHasMouseDevice(hasMouseDevice), 0);
}

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
#if ENABLE(FULL_KEYBOARD_ACCESS)
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
#else
    return NO;
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)

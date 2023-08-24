/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIWindows.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIWindowsEvent.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebProcess.h"
#import "_WKWebExtensionUtilities.h"
#import <wtf/DateMath.h>

namespace WebKit {

bool WebExtensionAPIWindows::isPropertyAllowed(String name, WebPage*)
{
    // FIXME: Implement.

    return true;
}

void WebExtensionAPIWindows::createWindow(NSDictionary *data, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/create

    // FIXME: Implement.
}

void WebExtensionAPIWindows::get(double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/get

    // FIXME: Implement.
}

void WebExtensionAPIWindows::getCurrent(NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getCurrent

    // FIXME: Implement.
}

void WebExtensionAPIWindows::getLastFocused(NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getLastFocused

    // FIXME: Implement.
}

void WebExtensionAPIWindows::getAll(NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getAll

    // FIXME: Implement.
}

void WebExtensionAPIWindows::update(double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/update

    // FIXME: Implement.
}

void WebExtensionAPIWindows::remove(double windowID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/remove

    // FIXME: Implement.
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onCreated

    if (!m_onCreated)
        m_onCreated = WebExtensionAPIWindowsEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WindowsOnCreated);

    return *m_onCreated;
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIWindowsEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WindowsOnRemoved);

    return *m_onRemoved;
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onFocusChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onFocusChanged

    if (!m_onFocusChanged)
        m_onFocusChanged = WebExtensionAPIWindowsEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::WindowsOnFocusChanged);

    return *m_onFocusChanged;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

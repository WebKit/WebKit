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
#import "WebExtensionAPITabs.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionTabParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

namespace WebKit {

NSDictionary *toWebAPI(const WebExtensionTabParameters& parameters)
{
    // FIXME: Implemnet.

    return @{ };
}

bool WebExtensionAPITabs::isPropertyAllowed(String name, WebPage*)
{
    // FIXME: Implement.

    return true;
}

void WebExtensionAPITabs::createTab(NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create

    // FIXME: Implement.
}

void WebExtensionAPITabs::query(NSDictionary *queryInfo, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/query

    // FIXME: Implement.
}

void WebExtensionAPITabs::get(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/get

    // FIXME: Implement.
}

void WebExtensionAPITabs::getCurrent(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getCurrent

    // FIXME: Implement.
}

void WebExtensionAPITabs::getSelected(double windowID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getSelected

    // FIXME: Implement.
}

void WebExtensionAPITabs::duplicate(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/duplicate

    // FIXME: Implement.
}

void WebExtensionAPITabs::update(double tabID, NSDictionary *updateProperties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/update

    // FIXME: Implement.
}

void WebExtensionAPITabs::remove(NSObject *tabIDs, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove

    // FIXME: Implement.
}

void WebExtensionAPITabs::reload(double tabID, NSDictionary *reloadProperties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/reload

    // FIXME: Implement.
}

void WebExtensionAPITabs::goBack(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goBack

    // FIXME: Implement.
}

void WebExtensionAPITabs::goForward(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goForward

    // FIXME: Implement.
}

void WebExtensionAPITabs::getZoom(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getZoom

    // FIXME: Implement.
}

void WebExtensionAPITabs::setZoom(double tabID, double zoomFactor, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/setZoom

    // FIXME: Implement.
}

void WebExtensionAPITabs::detectLanguage(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/detectLanguage

    // FIXME: Implement.
}

void WebExtensionAPITabs::toggleReaderMode(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/toggleReaderMode

    // FIXME: Implement.
}

WebExtensionAPIEvent& WebExtensionAPITabs::onActivated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    if (!m_onActivated)
        m_onActivated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnActivated);

    return *m_onActivated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onAttached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onAttached

    if (!m_onAttached)
        m_onAttached = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnAttached);

    return *m_onAttached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    if (!m_onCreated)
        m_onCreated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnCreated);

    return *m_onCreated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onDetached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onDetached

    if (!m_onDetached)
        m_onDetached = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnDetached);

    return *m_onDetached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onHighlighted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onHighlighted

    if (!m_onHighlighted)
        m_onHighlighted = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnHighlighted);

    return *m_onHighlighted;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onMoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    if (!m_onMoved)
        m_onMoved = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnMoved);

    return *m_onMoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnRemoved);

    return *m_onRemoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onReplaced()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    if (!m_onReplaced)
        m_onReplaced = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnReplaced);

    return *m_onReplaced;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onUpdated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    if (!m_onUpdated)
        m_onUpdated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnUpdated);

    return *m_onUpdated;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

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
#import "WebExtensionAPINotifications.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIKeys.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionNotificationParameters.h"
#import "WebExtensionUtilities.h"
#import "WebFrame.h"
#import "WebProcess.h"
#import <WebCore/LocalFrameInlines.h>
#import <wtf/UUID.h>

namespace WebKit {

#if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)

enum class ForUpdate : bool { No, Yes };

static bool parseNotificationOptions(NSDictionary *options, ForUpdate forUpdate, WebExtensionNotificationParameters& parameters, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        messageKey,
        titleKey,
        typeKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        typeKey: NSString.class,
        titleKey: NSString.class,
        messageKey: NSString.class,
        contextMessageKey: NSString.class,
        iconURLKey: NSString.class,
        buttonsKey: @[ NSDictionary.class ],
    };

    if (!validateDictionary(options, @"options", forUpdate == ForUpdate::Yes ? nil : requiredKeys, types, outExceptionString))
        return false;

    if (NSString *title = objectForKey<NSString>(options, titleKey))
        parameters.title = title;

    if (NSString *message = objectForKey<NSString>(options, messageKey))
        parameters.message = message;

    if (NSString *contextMessage = objectForKey<NSString>(options, contextMessageKey))
        parameters.contextMessage = contextMessage;

    if (NSArray *buttons = objectForKey<NSArray>(options, buttonsKey)) {
        static NSArray<NSString *> *buttonRequiredKeys = @[ titleKey ];
        static NSDictionary<NSString *, id> *buttonTypes = @{
            titleKey: NSString.class,
        };

        Vector<WebExtensionNotificationButton> parsedButtons;

        for (NSDictionary *button in buttons) {
            if (!validateDictionary(button, @"button", buttonRequiredKeys, buttonTypes, outExceptionString))
                return false;

            WebExtensionNotificationButton parsedButton;

            if (NSString *buttonTitle = objectForKey<NSString>(button, titleKey))
                parsedButton.title = buttonTitle;

            parsedButtons.append(WTF::move(parsedButton));
        }

        parameters.buttons = WTF::move(parsedButtons);
    }

    return true;
}

void WebExtensionAPINotifications::createNotification(const String& identifier, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/create

    WebExtensionNotificationParameters parameters;
    if (!parseNotificationOptions(options, ForUpdate::No, parameters, outExceptionString))
        return;

    parameters.identifier = !identifier.isEmpty() ? identifier : createVersion4UUIDString();

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::NotificationsCreate(parameters), [protectedThis = Ref { *this }, callback = WTF::move(callback), identifier = parameters.identifier](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), identifier));
    }, extensionContext().identifier());
}

void WebExtensionAPINotifications::update(const String& identifier, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/update

    WebExtensionNotificationParameters parameters;
    if (!parseNotificationOptions(options, ForUpdate::Yes, parameters, outExceptionString))
        return;

    parameters.identifier = identifier;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::NotificationsUpdate(identifier, parameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<bool, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(JSValueMakeBoolean(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPINotifications::clear(const String& identifier, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/clear

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::NotificationsClear(identifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<bool, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(JSValueMakeBoolean(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPINotifications::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::NotificationsGetAll(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Vector<String> identifiers) {
        NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:identifiers.size()];
        for (auto& identifier : identifiers)
            result[identifier.createNSString().get()] = @YES;

        callback->call(toJSValueRef(callback->globalContext(), result));
    }, extensionContext().identifier());
}

void WebExtensionAPINotifications::getPermissionLevel(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.chrome.com/docs/extensions/reference/api/notifications#method-getPermissionLevel

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::NotificationsGetPermissionLevel(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

#endif

WebExtensionAPIEvent& WebExtensionAPINotifications::onClicked()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onClicked

    if (!m_onClicked)
        lazyInitialize(m_onClicked, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::NotificationsOnClicked));

    return *m_onClicked;
}

WebExtensionAPIEvent& WebExtensionAPINotifications::onButtonClicked()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onButtonClicked

    if (!m_onButtonClicked)
        lazyInitialize(m_onButtonClicked, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::NotificationsOnButtonClicked));

    return *m_onButtonClicked;
}

WebExtensionAPIEvent& WebExtensionAPINotifications::onClosed()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onClosed

    if (!m_onClosed)
        lazyInitialize(m_onClosed, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::NotificationsOnClosed));

    return *m_onClosed;
}

#if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)

void WebExtensionContextProxy::dispatchNotificationsClickedEvent(const String& identifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onClicked

    RetainPtr nsIdentifier = identifier.createNSString();
    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        RefPtr coreFrame = frame.coreLocalFrame();
        WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, protect(coreFrame ? coreFrame->document() : nullptr));
        namespaceObject.notifications().onClicked().invokeListenersWithArgument(nsIdentifier.get());
    });
}

void WebExtensionContextProxy::dispatchNotificationsButtonClickedEvent(const String& identifier, uint64_t buttonIndex)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onButtonClicked

    RetainPtr nsIdentifier = identifier.createNSString();
    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        RefPtr coreFrame = frame.coreLocalFrame();
        WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, protect(coreFrame ? coreFrame->document() : nullptr));
        namespaceObject.notifications().onButtonClicked().invokeListenersWithArgument(nsIdentifier.get(), @(buttonIndex));
    });
}

void WebExtensionContextProxy::dispatchNotificationsClosedEvent(const String& identifier, bool byUser)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications/onClosed

    RetainPtr nsIdentifier = identifier.createNSString();
    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.notifications().onClosed().invokeListenersWithArgument(nsIdentifier.get(), @(byUser));
    });
}

#endif

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

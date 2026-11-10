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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)

#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionController.h"
#import "WebExtensionPermission.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionNotificationInternal.h"
#import <wtf/BlockPtr.h>
#import <wtf/Borrow.h>

namespace WebKit {

static _WKWebExtensionNotification *createNotificationObject(WKWebExtensionContext *context, const WebExtensionNotificationParameters& parameters)
{
    NSMutableArray<_WKWebExtensionNotificationButton *> *buttons = [NSMutableArray array];
    if (parameters.buttons) {
        for (auto& button : borrow(*parameters.buttons))
            [buttons addObject:[[_WKWebExtensionNotificationButton alloc] initWithTitle:createNSString(button.title).get()]];
    }

    NSString *subtitle = !parameters.contextMessage ? nil : createNSString(parameters.contextMessage).get();

    return [[_WKWebExtensionNotification alloc] initWithIdentifier:createNSString(parameters.identifier).get() webExtensionContext:context title:createNSString(parameters.title).get() subtitle:subtitle body:createNSString(parameters.message).get() buttons:buttons];
}

static void mergeNotificationParameters(WebExtensionNotificationParameters& base, const WebExtensionNotificationParameters& update)
{
    if (update.title)
        base.title = update.title;
    if (update.message)
        base.message = update.message;
    if (update.contextMessage)
        base.contextMessage = update.contextMessage;
    if (update.buttons)
        base.buttons = update.buttons;
}

bool WebExtensionContext::isNotificationsMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::notifications());
}

void WebExtensionContext::notificationsCreate(const WebExtensionNotificationParameters& parameters, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"notifications.create()";

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"the extension is not loaded").get()));
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:presentNotification:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    auto *notification = createNotificationObject(wrapper(), parameters);

    [controllerDelegate _webExtensionController:controller->wrapper() presentNotification:notification forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, parameters, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        m_notifications.set(parameters.identifier, parameters);
        completionHandler({ });
    }).get()];
}

void WebExtensionContext::notificationsUpdate(StringImpl* identifier, const WebExtensionNotificationParameters& parameters, CompletionHandler<void(std::expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"notifications.update()";

    auto entry = m_notifications.find(identifier);
    if (entry == m_notifications.end()) {
        completionHandler(false);
        return;
    }

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"the extension is not loaded").get()));
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:updateNotification:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    WebExtensionNotificationParameters merged = entry->value;
    mergeNotificationParameters(merged, parameters);

    auto *notification = createNotificationObject(wrapper(), merged);

    [controllerDelegate _webExtensionController:controller->wrapper() updateNotification:notification forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, identifier = RefPtr { identifier }, merged = WTF::move(merged), completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        m_notifications.set(identifier, WTF::move(merged));
        completionHandler(true);
    }).get()];
}

void WebExtensionContext::notificationsClear(StringImpl* identifier, CompletionHandler<void(std::expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"notifications.clear()";

    auto entry = m_notifications.find(identifier);
    if (entry == m_notifications.end()) {
        completionHandler(false);
        return;
    }

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"the extension is not loaded").get()));
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:clearNotification:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    auto *notification = createNotificationObject(wrapper(), entry->value);

    [controllerDelegate _webExtensionController:controller->wrapper() clearNotification:notification forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, identifier = RefPtr { identifier }, completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        m_notifications.remove(identifier);
        completionHandler(true);
    }).get()];
}

void WebExtensionContext::notificationsGetAll(CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    completionHandler(copyToVector(m_notifications.keys()));
}

void WebExtensionContext::notificationsGetPermissionLevel(CompletionHandler<void(std::expected<RefPtr<StringImpl>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"notifications.getPermissionLevel()";

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"the extension is not loaded").get()));
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:mayPresentNotificationsForExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"it is not implemented").get()));
        return;
    }

    [controllerDelegate _webExtensionController:controller->wrapper() mayPresentNotificationsForExtensionContext:wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](BOOL mayPresent, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(StringImpl::create(apiName).get(), nullString(), StringImpl::create(error.localizedDescription).get()));
            return;
        }

        completionHandler(StringImpl::create(mayPresent ? "granted"_s : "denied"_s));
    }).get()];
}

void WebExtensionContext::fireNotificationsClickedEventIfNeeded(StringImpl* identifier)
{
    if (!m_notifications.contains(identifier))
        return;

    constexpr auto type = WebExtensionEventListenerType::NotificationsOnClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, identifier = RefPtr { identifier }, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchNotificationsClickedEvent(identifier));
    });
}

void WebExtensionContext::fireNotificationsButtonClickedEventIfNeeded(StringImpl* identifier, size_t buttonIndex)
{
    auto entry = m_notifications.find(identifier);
    if (entry == m_notifications.end())
        return;

    auto& buttons = entry->value.buttons;
    if (!buttons || buttonIndex >= buttons->size())
        return;

    constexpr auto type = WebExtensionEventListenerType::NotificationsOnButtonClicked;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, identifier = RefPtr { identifier }, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchNotificationsButtonClickedEvent(identifier, buttonIndex));
    });
}

void WebExtensionContext::fireNotificationsClosedEventIfNeeded(StringImpl* identifier, UserTriggered userTriggered)
{
    if (!m_notifications.remove(identifier))
        return;

    constexpr auto type = WebExtensionEventListenerType::NotificationsOnClosed;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, identifier = RefPtr { identifier }, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchNotificationsClosedEvent(identifier, userTriggered == UserTriggered::Yes));
    });
}

}

#endif

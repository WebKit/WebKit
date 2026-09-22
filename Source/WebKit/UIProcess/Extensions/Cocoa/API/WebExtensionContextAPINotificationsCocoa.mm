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
#import "WebExtensionController.h"
#import "WebExtensionPermission.h"
#import "_WKWebExtensionNotificationInternal.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

static _WKWebExtensionNotification *createNotificationObject(WKWebExtensionContext *context, const WebExtensionNotificationParameters& parameters)
{
    NSMutableArray<_WKWebExtensionNotificationButton *> *buttons = [NSMutableArray array];
    if (parameters.buttons) {
        for (auto& button : *parameters.buttons)
            [buttons addObject:[[_WKWebExtensionNotificationButton alloc] initWithTitle:button.title.createNSString().get()]];
    }

    NSString *subtitle = parameters.contextMessage.isNull() ? nil : parameters.contextMessage.createNSString().get();

    return [[_WKWebExtensionNotification alloc] initWithIdentifier:parameters.identifier.createNSString().get() webExtensionContext:context title:parameters.title.createNSString().get() subtitle:subtitle body:parameters.message.createNSString().get() buttons:buttons];
}

bool WebExtensionContext::isNotificationsMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::notifications());
}

void WebExtensionContext::notificationsCreate(const WebExtensionNotificationParameters& parameters, CompletionHandler<void()>&& completionHandler)
{
    m_notifications.set(parameters.identifier, parameters);

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler();
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:presentNotification:forExtensionContext:completionHandler:)]) {
        completionHandler();
        return;
    }

    auto *controllerWrapper = controller->wrapper();
    auto *contextWrapper = wrapper();
    if (!(controllerWrapper && contextWrapper)) {
        completionHandler();
        return;
    }

    auto *notification = createNotificationObject(contextWrapper, parameters);

    [controllerDelegate _webExtensionController:controllerWrapper presentNotification:notification forExtensionContext:contextWrapper completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSError *) mutable {
        completionHandler();
    }).get()];
}

}

#endif

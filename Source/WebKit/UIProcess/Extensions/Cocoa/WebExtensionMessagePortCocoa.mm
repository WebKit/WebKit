/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WebExtensionMessagePort.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WKWebExtensionMessagePortInternal.h"
#import "WebExtensionContext.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

NSError *toAPI(WebExtensionMessagePort::Error error)
{
    if (!error)
        return nil;

    WKWebExtensionMessagePortError errorCode;
    NSString *message;

    switch (error.value().first) {
    case WebKit::WebExtensionMessagePort::ErrorType::Unknown:
        errorCode = WKWebExtensionMessagePortErrorUnknown;
        message = (NSString *)error.value().second.value_or("An unknown error occurred."_s);
        break;

    case WebKit::WebExtensionMessagePort::ErrorType::NotConnected:
        errorCode = WKWebExtensionMessagePortErrorNotConnected;
        message = (NSString *)error.value().second.value_or("Message port is not connected and cannot send messages."_s);
        break;

    case WebKit::WebExtensionMessagePort::ErrorType::MessageInvalid:
        errorCode = WKWebExtensionMessagePortErrorMessageInvalid;
        message = (NSString *)error.value().second.value_or("Message is not JSON-serializable."_s);
        break;
    }

    return [NSError errorWithDomain:WKWebExtensionMessagePortErrorDomain code:errorCode userInfo:@{ NSDebugDescriptionErrorKey: message }];
}

WebExtensionMessagePort::Error toWebExtensionMessagePortError(NSError *error)
{
    if (!error)
        return std::nullopt;
    return { { WebExtensionMessagePort::ErrorType::Unknown, error.localizedDescription } };
}

WebExtensionMessagePort::WebExtensionMessagePort(WebExtensionContext& extensionContext, String applicationIdentifier, WebExtensionPortChannelIdentifier channelIdentifier)
    : m_extensionContext(extensionContext)
    , m_applicationIdentifier(applicationIdentifier)
    , m_channelIdentifier(channelIdentifier)
{
}

bool WebExtensionMessagePort::operator==(const WebExtensionMessagePort& other) const
{
    return this == &other || (m_extensionContext == other.m_extensionContext && m_applicationIdentifier == other.m_applicationIdentifier && m_channelIdentifier == other.m_channelIdentifier);
}

WebExtensionContext* WebExtensionMessagePort::extensionContext() const
{
    return m_extensionContext.get();
}

bool WebExtensionMessagePort::isDisconnected() const
{
    return !m_extensionContext;
}

void WebExtensionMessagePort::disconnect(Error error)
{
    remove();
}

void WebExtensionMessagePort::reportDisconnection(Error error)
{
    ASSERT(!isDisconnected());

    remove();

    if (auto disconnectHandler = wrapper().disconnectHandler)
        disconnectHandler(toAPI(error));
}

void WebExtensionMessagePort::remove()
{
    if (isDisconnected())
        return;

    Ref protectedThis { *this };

    m_extensionContext->removeNativePort(*this);
    m_extensionContext->firePortDisconnectEventIfNeeded(WebExtensionContentWorldType::Native, WebExtensionContentWorldType::Main, m_channelIdentifier);
    m_extensionContext = nullptr;
}

void WebExtensionMessagePort::sendMessage(id message, CompletionHandler<void(Error error)>&& completionHandler)
{
    if (isDisconnected()) {
        if (completionHandler)
            completionHandler({ { ErrorType::NotConnected, std::nullopt } });
        return;
    }

    if (message && !isValidJSONObject(message, JSONOptions::FragmentsAllowed)) {
        if (completionHandler)
            completionHandler({ { ErrorType::MessageInvalid, std::nullopt } });
        return;
    }

    m_extensionContext->portPostMessage(WebExtensionContentWorldType::Native, WebExtensionContentWorldType::Main, std::nullopt, m_channelIdentifier, encodeJSONString(message, JSONOptions::FragmentsAllowed) );

    if (completionHandler)
        completionHandler(std::nullopt);
}

void WebExtensionMessagePort::receiveMessage(id message, Error error)
{
    ASSERT(!isDisconnected());

    if (auto messageHandler = wrapper().messageHandler)
        messageHandler(message, toAPI(error));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

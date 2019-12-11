/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WebAuthenticationPanelClient.h"
#import "_WKWebAuthenticationPanelInternal.h"
#import <WebCore/WebAuthenticationConstants.h>

#import <wtf/RetainPtr.h>

@implementation _WKWebAuthenticationPanel {
#if ENABLE(WEB_AUTHN)
    WeakPtr<WebKit::WebAuthenticationPanelClient> _client;
    RetainPtr<NSMutableArray> _transports;
#endif
}

#if ENABLE(WEB_AUTHN)

- (void)dealloc
{
    _panel->~WebAuthenticationPanel();

    [super dealloc];
}

- (id <_WKWebAuthenticationPanelDelegate>)delegate
{
    if (!_client)
        return nil;
    return _client->delegate().autorelease();
}

- (void)setDelegate:(id<_WKWebAuthenticationPanelDelegate>)delegate
{
    auto client = WTF::makeUniqueRef<WebKit::WebAuthenticationPanelClient>(self, delegate);
    _client = makeWeakPtr(client.get());
    _panel->setClient(WTFMove(client));
}


- (NSString *)relyingPartyID
{
    return _panel->rpId();
}

static _WKWebAuthenticationTransport wkWebAuthenticationTransport(WebCore::AuthenticatorTransport transport)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Usb:
        return _WKWebAuthenticationTransportUSB;
    case WebCore::AuthenticatorTransport::Nfc:
        return _WKWebAuthenticationTransportNFC;
    default:
        ASSERT_NOT_REACHED();
        return _WKWebAuthenticationTransportUSB;
    }
}

- (NSArray *)transports
{
    if (_transports)
        return _transports.get();

    auto& transports = _panel->transports();
    _transports = [[NSMutableArray alloc] initWithCapacity:transports.size()];
    for (auto& transport : transports)
        [_transports addObject:adoptNS([[NSNumber alloc] initWithInt:wkWebAuthenticationTransport(transport)]).get()];
    return _transports.get();
}

static _WKWebAuthenticationType wkWebAuthenticationType(WebCore::ClientDataType type)
{
    switch (type) {
    case WebCore::ClientDataType::Create:
        return _WKWebAuthenticationTypeCreate;
    case WebCore::ClientDataType::Get:
        return _WKWebAuthenticationTypeGet;
    default:
        ASSERT_NOT_REACHED();
        return _WKWebAuthenticationTypeCreate;
    }
}

- (_WKWebAuthenticationType)type
{
    return wkWebAuthenticationType(_panel->clientDataType());
}
#else // ENABLE(WEB_AUTHN)
- (id <_WKWebAuthenticationPanelDelegate>)delegate
{
    return nil;
}
- (void)setDelegate:(id<_WKWebAuthenticationPanelDelegate>)delegate
{
}
#endif // ENABLE(WEB_AUTHN)

- (void)cancel
{
#if ENABLE(WEB_AUTHN)
    _panel->cancel();
#endif
}

#if ENABLE(WEB_AUTHN)
#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_panel;
}
#endif

@end

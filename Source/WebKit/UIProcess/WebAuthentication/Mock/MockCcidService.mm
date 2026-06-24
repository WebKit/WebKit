/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "MockCcidService.h"

#if ENABLE(WEB_AUTHN)

#import <CryptoTokenKit/TKSmartCard.h>
#include <WebCore/FidoConstants.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/cocoa/VectorCocoa.h>

@interface _WKMockTKSmartCard : TKSmartCard
- (instancetype)initWithService:(WeakPtr<WebKit::MockCcidService>&&)service;
@end

@implementation _WKMockTKSmartCard {
    WeakPtr<WebKit::MockCcidService> m_service;
}

- (instancetype)initWithService:(WeakPtr<WebKit::MockCcidService>&&)service
{
    if (!(self = [super init]))
        return nil;

    m_service = WTF::move(service);

    return self;
}

- (void)beginSessionWithReply:(void(^)(BOOL success, NSError * error))reply
{
    reply(TRUE, nil);
}

- (void)endSession
{
}

- (void)transmitRequest:(NSData *)request reply:(void(^)(NSData * response, NSError * error))reply
{
    reply(Ref { *m_service }->nextReply(request).get(), nil);
}

@end

namespace WebKit {
using namespace fido;

static RetainPtr<NSData> dataFromBase64(const String& base64String)
{
    if (base64String.isEmpty())
        return nil;
    return adoptNS([[NSData alloc] initWithBase64EncodedString:base64String.createNSString().get() options:NSDataBase64DecodingIgnoreUnknownCharacters]);
}

Ref<MockCcidService> MockCcidService::create(AuthenticatorTransportServiceObserver& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
{
    return adoptRef(*new MockCcidService(observer, configuration));
}

MockCcidService::MockCcidService(AuthenticatorTransportServiceObserver& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
    : CcidService(observer)
    , m_configuration(configuration)
{
}

void MockCcidService::platformStartDiscovery()
{
    if (!!m_configuration.ccid) {
        auto card = adoptNS([[_WKMockTKSmartCard alloc] initWithService:this]);
        onValidCard(WTF::move(card), nullptr);
        return;
    }
    LOG_ERROR("No ccid authenticators is available.");
}

RetainPtr<NSData> MockCcidService::nextReply(NSData *request)
{
    // Command-aware replies let a test model a legacy U2F-only key: the applet selection command can be
    // answered with a non-match, forcing the connection to fall back to the U2F_VERSION command.
    auto requestVector = makeVector(request);
    if (!m_configuration.ccid->appletSelectionResponseBase64.isEmpty() && equalSpans(requestVector.span(), std::span { kCtapNfcAppletSelectionCommand }))
        return dataFromBase64(m_configuration.ccid->appletSelectionResponseBase64);
    if (!m_configuration.ccid->u2fVersionResponseBase64.isEmpty() && equalSpans(requestVector.span(), std::span { kCtapNfcU2fVersionCommand }))
        return dataFromBase64(m_configuration.ccid->u2fVersionResponseBase64);

    if (m_configuration.ccid->payloadBase64.isEmpty())
        return nil;

    auto result = dataFromBase64(m_configuration.ccid->payloadBase64[0]);
    m_configuration.ccid->payloadBase64.removeAt(0);
    return result;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

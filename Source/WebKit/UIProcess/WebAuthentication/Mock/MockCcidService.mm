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
#include <wtf/RunLoop.h>

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

    m_service = WTFMove(service);

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
    reply(m_service->nextReply().get(), nil);
}

@end

namespace WebKit {

MockCcidService::MockCcidService(Observer& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
    : CcidService(observer)
    , m_configuration(configuration)
{
}

void MockCcidService::platformStartDiscovery()
{
    if (!!m_configuration.ccid) {
        auto card = adoptNS([[_WKMockTKSmartCard alloc] initWithService:this]);
        onValidCard(WTFMove(card));
        return;
    }
    LOG_ERROR("No ccid authenticators is available.");
}

RetainPtr<NSData> MockCcidService::nextReply()
{
    if (m_configuration.ccid->payloadBase64.isEmpty())
        return nil;

    auto result = adoptNS([[NSData alloc] initWithBase64EncodedString:m_configuration.ccid->payloadBase64[0] options:NSDataBase64DecodingIgnoreUnknownCharacters]);
    m_configuration.ccid->payloadBase64.remove(0);
    return result;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

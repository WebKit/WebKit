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

#import "config.h"
#import "NetworkDigitalCredentialsPresentment.h"

#if ENABLE(WEB_AUTHN)

#import "DigitalCredentialsPresentment.h"
#import <WebKit/WKIdentityDocumentPresentmentRawRequest.h>
#import <WebKit/WKIdentityDocumentPresentmentResponse.h>
#import <WebKit/WKIdentityDocumentSystemPresentmentController.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

#import "WebKitSwiftSoftLink.h"

@protocol WKDigitalCredentialsPresentmentPerforming <NSObject>
- (void)performRawRequests:(NSArray<WKIdentityDocumentPresentmentRawRequest *> *)rawRequests origin:(NSURL *)origin completionHandler:(void (^)(WKIdentityDocumentPresentmentResponse *response, NSError *error))completionHandler;
- (void)cancelRequest;
@end

#if HAVE(IDENTITY_DOCUMENT_SERVICES_SYSTEM_PRESENTMENT)
@interface WKIdentityDocumentSystemPresentmentController () <WKDigitalCredentialsPresentmentPerforming>
@end
#endif

@interface WKMockPlatformWalletController : NSObject <WKDigitalCredentialsPresentmentPerforming>
@end

@implementation WKMockPlatformWalletController {
    BlockPtr<void(WKIdentityDocumentPresentmentResponse *, NSError *)> _completionHandler;
}

- (void)performRawRequests:(NSArray<WKIdentityDocumentPresentmentRawRequest *> *)rawRequests origin:(NSURL *)origin completionHandler:(void (^)(WKIdentityDocumentPresentmentResponse *, NSError *))completionHandler
{
    static uint64_t presentationCount;
    _completionHandler = makeBlockPtr(completionHandler);
    RetainPtr json = [NSString stringWithFormat:@"{\"mockPlatformWallet\":true,\"origin\":\"%@\",\"presentation\":%llu}", origin.absoluteString, ++presentationCount];
    RetainPtr response = adoptNS([WebKit::allocWKIdentityDocumentPresentmentResponseInstance() initWithProtocolString:@"org.iso.mdoc" responseData:[json dataUsingEncoding:NSUTF8StringEncoding]]);
    callOnMainRunLoop([protectedSelf = RetainPtr { self }, response = WTF::move(response)] {
        if (auto completionHandler = std::exchange(protectedSelf->_completionHandler, nullptr))
            completionHandler(response.get(), nil);
    });
}

- (void)cancelRequest
{
    if (auto completionHandler = std::exchange(_completionHandler, nullptr))
        completionHandler(nil, adoptNS([[NSError alloc] initWithDomain:NSCocoaErrorDomain code:NSUserCancelledError userInfo:nil]).get());
}

@end

namespace WebKit {

struct ActiveRequest {
    DigitalCredentialRequestIdentifier identifier;
    RetainPtr<id<WKDigitalCredentialsPresentmentPerforming>> request;
};

static HashMap<WebPageProxyIdentifier, ActiveRequest>& activeRequests()
{
    static NeverDestroyed<HashMap<WebPageProxyIdentifier, ActiveRequest>> requests;
    return requests;
}

static RetainPtr<id<WKDigitalCredentialsPresentmentPerforming>> createPresentmentController(UseMockPlatformWallet useMockPlatformWallet)
{
    if (useMockPlatformWallet == UseMockPlatformWallet::Yes)
        return adoptNS([[WKMockPlatformWalletController alloc] init]);
#if HAVE(IDENTITY_DOCUMENT_SERVICES_SYSTEM_PRESENTMENT)
    return adoptNS([allocWKIdentityDocumentSystemPresentmentControllerInstance() init]);
#else
    return nil;
#endif
}

void presentDigitalCredentialRequestFromNetworkProcess(WebPageProxyIdentifier webPageProxyID, DigitalCredentialRequestIdentifier requestIdentifier, const WebCore::SecurityOriginData& topOrigin, const WebCore::DigitalCredentialsRawRequests& rawRequests, UseMockPlatformWallet useMockPlatformWallet, CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    RetainPtr origin = topOrigin.toURL().createNSURL();
    if (!origin)
        return completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotAllowedError, "Digital credentials request is not allowed."_s }));

    RetainPtr request = createPresentmentController(useMockPlatformWallet);
    if (!request)
        return completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotSupportedError, "Digital credentials are not available in this app."_s }));

    RetainPtr presentmentRawRequests = createPresentmentRawRequests(rawRequests);
    if (![presentmentRawRequests count])
        return completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::TypeError, "No supported credential requests to present."_s }));

    if (auto previousRequest = activeRequests().find(webPageProxyID); previousRequest != activeRequests().end())
        [previousRequest->value.request cancelRequest];
    activeRequests().set(webPageProxyID, ActiveRequest { requestIdentifier, request });

    [request performRawRequests:presentmentRawRequests.get() origin:origin.get() completionHandler:makeBlockPtr([webPageProxyID, request, completionHandler = WTF::move(completionHandler)](WKIdentityDocumentPresentmentResponse *response, NSError *error) mutable {
        callOnMainRunLoop([webPageProxyID, request = WTF::move(request), response = RetainPtr { response }, error = RetainPtr { error }, completionHandler = WTF::move(completionHandler)] mutable {
            auto activeRequest = activeRequests().find(webPageProxyID);
            if (activeRequest != activeRequests().end() && activeRequest->value.request == request)
                activeRequests().remove(activeRequest);
            completionHandler(responseDataFromPresentment(response.get(), error.get()));
        });
    }).get()];
}

void cancelDigitalCredentialRequestFromNetworkProcess(WebPageProxyIdentifier webPageProxyID, DigitalCredentialRequestIdentifier requestIdentifier)
{
    auto activeRequest = activeRequests().find(webPageProxyID);
    if (activeRequest == activeRequests().end() || activeRequest->value.identifier != requestIdentifier)
        return;
    RetainPtr request = WTF::move(activeRequest->value.request);
    activeRequests().remove(activeRequest);
    [request cancelRequest];
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

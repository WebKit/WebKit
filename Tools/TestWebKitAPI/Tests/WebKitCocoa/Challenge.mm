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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKErrorRecoveryAttempting.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/Platform.h>
#import <wtf/RetainPtr.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

static bool navigationFinished;

static RetainPtr<SecCertificateRef> testCertificate()
{
    auto certificateBytes = TestWebKitAPI::TCPServer::testCertificate();
    return adoptCF(SecCertificateCreateWithData(nullptr, (__bridge CFDataRef)[NSData dataWithBytes:certificateBytes.data() length:certificateBytes.size()]));
}

RetainPtr<SecIdentityRef> testIdentity()
{
    auto privateKeyBytes = TestWebKitAPI::TCPServer::testPrivateKey();
    NSData *derEncodedPrivateKey = [NSData dataWithBytes:privateKeyBytes.data() length:privateKeyBytes.size()];
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @4096,
    };
    const NSUInteger pemEncodedPrivateKeyHeaderLength = 26;
    CFErrorRef error = nullptr;
    auto privateKey = adoptCF(SecKeyCreateWithData((__bridge CFDataRef)[derEncodedPrivateKey subdataWithRange:NSMakeRange(pemEncodedPrivateKeyHeaderLength, derEncodedPrivateKey.length - pemEncodedPrivateKeyHeaderLength)], (__bridge CFDictionaryRef)options, &error));
    EXPECT_NULL(error);
    EXPECT_NOT_NULL(privateKey.get());

    return adoptCF(SecIdentityCreate(kCFAllocatorDefault, testCertificate().get(), privateKey.get()));
}

static RetainPtr<NSURLCredential> credentialWithIdentity()
{
    auto identity = testIdentity();
    EXPECT_NOT_NULL(identity);
    
    return [NSURLCredential credentialWithIdentity:identity.get() certificates:@[(id)testCertificate().get()] persistence:NSURLCredentialPersistenceNone];
}

@interface ChallengeDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ChallengeDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    NSURLProtectionSpace *protectionSpace = challenge.protectionSpace;
    EXPECT_NULL(challenge.proposedCredential);
    EXPECT_EQ(challenge.previousFailureCount, 0);
    EXPECT_TRUE([protectionSpace.realm isEqualToString:@"testrealm"]);
    EXPECT_FALSE(protectionSpace.receivesCredentialSecurely);
    EXPECT_FALSE(protectionSpace.isProxy);
    EXPECT_TRUE([protectionSpace.host isEqualToString:@"127.0.0.1"]);
    EXPECT_NULL(protectionSpace.proxyType);
    EXPECT_TRUE([protectionSpace.protocol isEqualToString:NSURLProtectionSpaceHTTP]);
    EXPECT_TRUE([protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodHTTPBasic]);
    EXPECT_EQ([(NSHTTPURLResponse *)challenge.failureResponse statusCode], 401);

    completionHandler(NSURLSessionAuthChallengeUseCredential, credentialWithIdentity().get());
}

@end

TEST(Challenge, SecIdentity)
{
    using namespace TestWebKitAPI;
    TCPServer server(TCPServer::respondWithChallengeThenOK);

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([ChallengeDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    // Make sure no credential left by previous tests.
    NSURLProtectionSpace *protectionSpace = [[[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:server.port() protocol:NSURLProtectionSpaceHTTP realm:@"testrealm" authenticationMethod:NSURLAuthenticationMethodHTTPBasic] autorelease];
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    Util::run(&navigationFinished);
}

@interface ClientCertificateDelegate : NSObject <WKNavigationDelegate> {
    Vector<RetainPtr<NSString>> _authenticationMethods;
}
- (const Vector<RetainPtr<NSString>>&)authenticationMethods;
@end

@implementation ClientCertificateDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    _authenticationMethods.append(challenge.protectionSpace.authenticationMethod);

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
        return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    
    EXPECT_TRUE([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodClientCertificate]);
    completionHandler(NSURLSessionAuthChallengeUseCredential, credentialWithIdentity().get());
}

- (const Vector<RetainPtr<NSString>>&)authenticationMethods
{
    return _authenticationMethods;
}

@end

#if HAVE(SEC_KEY_PROXY) && HAVE(SSL)
TEST(Challenge, ClientCertificate)
{
    using namespace TestWebKitAPI;
    TCPServer server(TCPServer::Protocol::HTTPSWithClientCertificateRequest, TCPServer::respondWithOK);

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([ClientCertificateDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
    
    Util::run(&navigationFinished);
    auto& methods = [delegate authenticationMethods];
    EXPECT_EQ(methods.size(), 2ull);
    EXPECT_TRUE([methods[0] isEqualToString:NSURLAuthenticationMethodServerTrust]);
    EXPECT_TRUE([methods[1] isEqualToString:NSURLAuthenticationMethodClientCertificate]);
}
#endif

static bool receivedSecondChallenge;
static RetainPtr<NSURLCredential> persistentCredential;

@interface ProposedCredentialDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ProposedCredentialDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    static bool firstChallenge = true;
    if (firstChallenge) {
        firstChallenge = false;
        persistentCredential = adoptNS([[NSURLCredential alloc] initWithUser:@"testuser" password:@"testpassword" persistence:NSURLCredentialPersistencePermanent]);
        return completionHandler(NSURLSessionAuthChallengeUseCredential, persistentCredential.get());
        
    }
    receivedSecondChallenge = true;
    return completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

@end

#if HAVE(NETWORK_FRAMEWORK)

TEST(Challenge, BasicProposedCredential)
{
    using namespace TestWebKitAPI;
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BasicProposedCredentialPlugIn"]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([ProposedCredentialDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    // Make sure no credential left by previous tests.
    NSURLProtectionSpace *protectionSpace = [[[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:server.port() protocol:NSURLProtectionSpaceHTTP realm:@"testrealm" authenticationMethod:NSURLAuthenticationMethodHTTPBasic] autorelease];
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace];

    RetainPtr<NSURLRequest> request = server.request();
    [webView loadRequest:request.get()];
    Util::run(&navigationFinished);
    navigationFinished = false;
    [webView loadRequest:request.get()];
    Util::run(&navigationFinished);
    EXPECT_TRUE(receivedSecondChallenge);

    // Clear persistent credentials created by this test.
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace];
}

#endif // HAVE(NETWORK_FRAMEWORK)

#if HAVE(SSL)
static void verifyCertificateAndPublicKey(SecTrustRef trust)
{
    EXPECT_NOT_NULL(trust);

    auto compareData = [] (const RetainPtr<CFDataRef>& data, const Vector<uint8_t>& expected) {
        size_t length = CFDataGetLength(data.get());
        EXPECT_EQ(length, expected.size());
        const UInt8* bytes = CFDataGetBytePtr(data.get());
        for (size_t i = 0; i < length; ++i)
            EXPECT_EQ(expected[i], bytes[i]);
    };

    auto publicKey = adoptCF(SecKeyCopyExternalRepresentation(adoptCF(SecTrustCopyPublicKey(trust)).get(), nullptr));
    compareData(publicKey, {
        0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xd8, 0x2b, 0xc8, 0xa6, 0x32, 0xe4, 0x62, 0xff, 0x4d,
        0xf3, 0xd0, 0xad, 0x59, 0x8b, 0x45, 0xa7, 0xbd, 0xf1, 0x47, 0xbf, 0x09, 0x58, 0x7b, 0x22, 0xbd,
        0x35, 0xae, 0x97, 0x25, 0x86, 0x94, 0xa0, 0x80, 0xc0, 0xb4, 0x1f, 0x76, 0x91, 0x67, 0x46, 0x31,
        0xd0, 0x10, 0x84, 0xb7, 0x22, 0x1e, 0x70, 0x23, 0x91, 0x72, 0xc8, 0xe9, 0x6d, 0x79, 0x3a, 0x85,
        0x77, 0x80, 0x0f, 0xc4, 0x95, 0x16, 0x75, 0xc5, 0x4a, 0x71, 0x4c, 0xc8, 0x63, 0x3f, 0xa3, 0xf2,
        0x63, 0x9c, 0x2a, 0x4f, 0x9a, 0xfa, 0xcb, 0xc1, 0x71, 0x6e, 0x28, 0x85, 0x28, 0xa0, 0x27, 0x1e,
        0x65, 0x1c, 0xae, 0x07, 0xd5, 0x5b, 0x6f, 0x2d, 0x43, 0xed, 0x2b, 0x90, 0xb1, 0x8c, 0xaf, 0x24,
        0x6d, 0xae, 0xe9, 0x17, 0x3a, 0x05, 0xc1, 0xbf, 0xb8, 0x1c, 0xae, 0x65, 0x3b, 0x1b, 0x58, 0xc2,
        0xd9, 0xae, 0xd6, 0xaa, 0x67, 0x88, 0xf1, 0x02, 0x03, 0x01, 0x00, 0x01
    });
    
    EXPECT_EQ(1, SecTrustGetCertificateCount(trust));
    auto certificate = adoptCF(SecCertificateCopyData(SecTrustGetCertificateAtIndex(trust, 0)));
    compareData(certificate, {
        0x30, 0x82, 0x02, 0x58, 0x30, 0x82, 0x01, 0xc1, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x09, 0x00,
        0xfb, 0xb0, 0x4c, 0x2e, 0xab, 0x10, 0x9b, 0x0c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
        0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
        0x04, 0x06, 0x13, 0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
        0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65, 0x31, 0x21, 0x30, 0x1f, 0x06,
        0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x20, 0x57,
        0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x1e,
        0x17, 0x0d, 0x31, 0x34, 0x30, 0x34, 0x32, 0x33, 0x32, 0x30, 0x35, 0x30, 0x34, 0x30, 0x5a, 0x17,
        0x0d, 0x31, 0x37, 0x30, 0x34, 0x32, 0x32, 0x32, 0x30, 0x35, 0x30, 0x34, 0x30, 0x5a, 0x30, 0x45,
        0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x41, 0x55, 0x31, 0x13, 0x30,
        0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61,
        0x74, 0x65, 0x31, 0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49, 0x6e, 0x74,
        0x65, 0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67, 0x69, 0x74, 0x73, 0x20, 0x50, 0x74,
        0x79, 0x20, 0x4c, 0x74, 0x64, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
        0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81,
        0x81, 0x00, 0xd8, 0x2b, 0xc8, 0xa6, 0x32, 0xe4, 0x62, 0xff, 0x4d, 0xf3, 0xd0, 0xad, 0x59, 0x8b,
        0x45, 0xa7, 0xbd, 0xf1, 0x47, 0xbf, 0x09, 0x58, 0x7b, 0x22, 0xbd, 0x35, 0xae, 0x97, 0x25, 0x86,
        0x94, 0xa0, 0x80, 0xc0, 0xb4, 0x1f, 0x76, 0x91, 0x67, 0x46, 0x31, 0xd0, 0x10, 0x84, 0xb7, 0x22,
        0x1e, 0x70, 0x23, 0x91, 0x72, 0xc8, 0xe9, 0x6d, 0x79, 0x3a, 0x85, 0x77, 0x80, 0x0f, 0xc4, 0x95,
        0x16, 0x75, 0xc5, 0x4a, 0x71, 0x4c, 0xc8, 0x63, 0x3f, 0xa3, 0xf2, 0x63, 0x9c, 0x2a, 0x4f, 0x9a,
        0xfa, 0xcb, 0xc1, 0x71, 0x6e, 0x28, 0x85, 0x28, 0xa0, 0x27, 0x1e, 0x65, 0x1c, 0xae, 0x07, 0xd5,
        0x5b, 0x6f, 0x2d, 0x43, 0xed, 0x2b, 0x90, 0xb1, 0x8c, 0xaf, 0x24, 0x6d, 0xae, 0xe9, 0x17, 0x3a,
        0x05, 0xc1, 0xbf, 0xb8, 0x1c, 0xae, 0x65, 0x3b, 0x1b, 0x58, 0xc2, 0xd9, 0xae, 0xd6, 0xaa, 0x67,
        0x88, 0xf1, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x50, 0x30, 0x4e, 0x30, 0x1d, 0x06, 0x03, 0x55,
        0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x8b, 0x75, 0xd5, 0xac, 0xcb, 0x08, 0xbe, 0x0e, 0x1f, 0x65,
        0xb7, 0xfa, 0x56, 0xbe, 0x6c, 0xa7, 0x75, 0xda, 0x85, 0xaf, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d,
        0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x8b, 0x75, 0xd5, 0xac, 0xcb, 0x08, 0xbe, 0x0e, 0x1f,
        0x65, 0xb7, 0xfa, 0x56, 0xbe, 0x6c, 0xa7, 0x75, 0xda, 0x85, 0xaf, 0x30, 0x0c, 0x06, 0x03, 0x55,
        0x1d, 0x13, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
        0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x81, 0x81, 0x00, 0x3b, 0xe8, 0x78, 0x6d,
        0x95, 0xd6, 0x3d, 0x6a, 0xf7, 0x13, 0x19, 0x2c, 0x1b, 0xc2, 0x88, 0xae, 0x22, 0xab, 0xf4, 0x8d,
        0x32, 0xf5, 0x7c, 0x71, 0x67, 0xcf, 0x2d, 0xd1, 0x1c, 0xc2, 0xc3, 0x87, 0xe2, 0xe9, 0xbe, 0x89,
        0x5c, 0xe4, 0x34, 0xab, 0x48, 0x91, 0xc2, 0x3f, 0x95, 0xae, 0x2b, 0x47, 0x9e, 0x25, 0x78, 0x6b,
        0x4f, 0x9a, 0x10, 0xa4, 0x72, 0xfd, 0xcf, 0xf7, 0x02, 0x0c, 0xb0, 0x0a, 0x08, 0xa4, 0x5a, 0xe2,
        0xe5, 0x74, 0x7e, 0x11, 0x1d, 0x39, 0x60, 0x6a, 0xc9, 0x1f, 0x69, 0xf3, 0x2e, 0x63, 0x26, 0xdc,
        0x9e, 0xef, 0x6b, 0x7a, 0x0a, 0xe1, 0x54, 0x57, 0x98, 0xaa, 0x72, 0x91, 0x78, 0x04, 0x7e, 0x1f,
        0x8f, 0x65, 0x4d, 0x1f, 0x0b, 0x12, 0xac, 0x9c, 0x24, 0x0f, 0x84, 0x14, 0x1a, 0x55, 0x2d, 0x1f,
        0xbb, 0xf0, 0x9d, 0x09, 0xb2, 0x08, 0x5c, 0x59, 0x32, 0x65, 0x80, 0x26
    });
}

@interface ServerTrustDelegate : NSObject <WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
- (NSError *)waitForDidFailProvisionalNavigationError;
- (size_t)authenticationChallengeCount;
@end

@implementation ServerTrustDelegate {
    size_t _authenticationChallengeCount;
    bool _navigationFinished;
    RetainPtr<NSError> _provisionalNavigationFailedError;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _navigationFinished = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error
{
    _provisionalNavigationFailedError = error;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_navigationFinished);
}

- (NSError *)waitForDidFailProvisionalNavigationError
{
    while (!_provisionalNavigationFailedError)
        TestWebKitAPI::Util::spinRunLoop();
    return _provisionalNavigationFailedError.autorelease();
}

- (size_t)authenticationChallengeCount
{
    return _authenticationChallengeCount;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * _Nullable credential))completionHandler
{
    _authenticationChallengeCount++;
    SecTrustRef trust = challenge.protectionSpace.serverTrust;
    verifyCertificateAndPublicKey(trust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:trust]);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, ServerTrust)
{
    TCPServer server(TCPServer::Protocol::HTTPS, [] (SSL* ssl) {
        TCPServer::read(ssl);

        const char* reply = ""
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 13\r\n\r\n"
        "Hello, World!";
        TCPServer::write(ssl, reply, strlen(reply));
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([ServerTrustDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://localhost:%d/", server.port()]]]];
    [delegate waitForDidFinishNavigation];

    verifyCertificateAndPublicKey([webView serverTrust]);
    EXPECT_EQ([delegate authenticationChallengeCount], 1u);
}

TEST(WebKit, FastServerTrust)
{
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
    TCPServer server(TCPServer::Protocol::HTTPS, TCPServer::respondWithOK);
#else
    TCPServer server(TCPServer::Protocol::HTTPS, [](SSL* ssl) {
        EXPECT_FALSE(ssl);
    }, WTF::nullopt, 2);
#endif
    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    dataStoreConfiguration.fastServerTrustEvaluationEnabled = YES;
    configuration.websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto delegate = adoptNS([ServerTrustDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://localhost:%d/", server.port()]]]];
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ([delegate authenticationChallengeCount], 1ull);
#else
    NSError *error = [delegate waitForDidFailProvisionalNavigationError];
    EXPECT_WK_STREQ([error.userInfo[_WKRecoveryAttempterErrorKey] className], @"WKReloadFrameErrorRecoveryAttempter");
    EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
    EXPECT_EQ(error.code, NSURLErrorServerCertificateUntrusted);
    EXPECT_EQ([delegate authenticationChallengeCount], 0ull);
#endif
}

// FIXME: Find out why these tests time out on Mojave.
#if HAVE(NETWORK_FRAMEWORK) && (!PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500)

static HTTPServer clientCertServer()
{
    Vector<LChar> chars(50000, 'a');
    String longString(chars.data(), chars.size());
    return HTTPServer({
        { "/", { "<html><img src='1.png'/><img src='2.png'/><img src='3.png'/><img src='4.png'/><img src='5.png'/><img src='6.png'/></html>" } },
        { "/1.png", { longString } },
        { "/2.png", { longString } },
        { "/3.png", { longString } },
        { "/4.png", { longString } },
        { "/5.png", { longString } },
        { "/6.png", { longString } },
        { "/redirectToError", { 301, {{ "Location", "/error" }} } },
        { "/error", { HTTPServer::HTTPResponse::TerminateConnection::Yes } },
    }, HTTPServer::Protocol::Https, [] (auto, auto, auto certificateAllowed) {
        certificateAllowed(true);
    });
}

static BlockPtr<void(WKWebView *, NSURLAuthenticationChallenge *, void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))> challengeHandler(Vector<RetainPtr<NSString>>& vector)
{
    return makeBlockPtr([&] (WKWebView *webView, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        NSString *method = challenge.protectionSpace.authenticationMethod;
        vector.append(method);
        if ([method isEqualToString:NSURLAuthenticationMethodClientCertificate])
            return completionHandler(NSURLSessionAuthChallengeUseCredential, credentialWithIdentity().get());
        if ([method isEqualToString:NSURLAuthenticationMethodServerTrust])
            return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        ASSERT_NOT_REACHED();
    }).get();
}

static size_t countClientCertChallenges(Vector<RetainPtr<NSString>>& vector)
{
    vector.removeAllMatching([](auto& method) {
        return ![method isEqualToString:NSURLAuthenticationMethodClientCertificate];
    });
    return vector.size();
};

TEST(MultipleClientCertificateConnections, Basic)
{
    auto server = clientCertServer();

    Vector<RetainPtr<NSString>> methods;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didReceiveAuthenticationChallenge = challengeHandler(methods).get();

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(countClientCertChallenges(methods), 1u);
}

TEST(MultipleClientCertificateConnections, Redirect)
{
    auto server = clientCertServer();

    Vector<RetainPtr<NSString>> methods;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didReceiveAuthenticationChallenge = challengeHandler(methods).get();

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/redirectToError", server.port()]]]];
    [delegate waitForDidFailProvisionalNavigation];
    EXPECT_EQ(countClientCertChallenges(methods), 1u);
    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(countClientCertChallenges(methods), 1u);
}

TEST(MultipleClientCertificateConnections, Failure)
{
    auto server = clientCertServer();

    Vector<RetainPtr<NSString>> methods;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didReceiveAuthenticationChallenge = challengeHandler(methods).get();

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/error", server.port()]]]];
    [delegate waitForDidFailProvisionalNavigation];
    size_t certChallengesAfterInitialFailure = countClientCertChallenges(methods);
    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(countClientCertChallenges(methods), certChallengesAfterInitialFailure);
}

TEST(MultipleClientCertificateConnections, NonPersistentDataStore)
{
    auto server = clientCertServer();

    Vector<RetainPtr<NSString>> methods;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didReceiveAuthenticationChallenge = challengeHandler(methods).get();

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];
    EXPECT_EQ(countClientCertChallenges(methods), 1u);
}

#endif // HAVE(NETWORK_FRAMEWORK)

} // namespace TestWebKitAPI

#endif // HAVE(SSL)

/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if HAVE(UI_EVENT_ATTRIBUTION)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>

#if HAVE(RSA_BSSA)

#include <Security/SecKeyPriv.h>
#include <WebCore/PrivateClickMeasurement.h>

extern "C" {
#include <corecrypto/cc_priv.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccrsabssa.h>
}

#endif // HAVE(RSA_BSSA)

@interface MockEventAttribution : NSObject

@property (nonatomic, assign, readonly) uint8_t sourceIdentifier;
@property (nonatomic, copy, readonly) NSURL *destinationURL;
@property (nonatomic, copy, readonly) NSURL *reportEndpoint;
@property (nonatomic, copy, readonly) NSString *sourceDescription;
@property (nonatomic, copy, readonly) NSString *purchaser;
- (instancetype)initWithReportEndpoint:(NSURL *)reportEndpoint destinationURL:(NSURL *)destinationURL;

@end

@implementation MockEventAttribution

- (instancetype)initWithReportEndpoint:(NSURL *)reportEndpoint destinationURL:(NSURL *)destinationURL
{
    if (!(self = [super init]))
        return nil;

    _sourceIdentifier = 42;
    _destinationURL = destinationURL;
    _reportEndpoint = reportEndpoint;
    _sourceDescription = @"test source description";
    _purchaser = @"test purchaser";

    return self;
}

@end

namespace TestWebKitAPI {

TEST(EventAttribution, Basic)
{
    bool done = false;
    HTTPServer server([&done, connectionCount = 0] (Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([connection] (Vector<char>&& request1) {
                EXPECT_TRUE(strnstr(request1.data(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n", request1.size()));
                const char* redirect = "HTTP/1.1 302 Found\r\n"
                    "Location: /.well-known/private-click-measurement/trigger-attribution/12\r\n"
                    "Content-Length: 0\r\n\r\n";
                connection.send(redirect, [connection] {
                    connection.receiveHTTPRequest([connection] (Vector<char>&& request2) {
                        EXPECT_TRUE(strnstr(request2.data(), "GET /.well-known/private-click-measurement/trigger-attribution/12 HTTP/1.1\r\n", request2.size()));
                        const char* response = "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 0\r\n\r\n";
                        connection.send(response);
                    });
                });
            });
            break;
        case 2:
            connection.receiveHTTPRequest([&done] (Vector<char>&& request3) {
                request3.append('\0');
                EXPECT_TRUE(strnstr(request3.data(), "POST / HTTP/1.1\r\n", request3.size()));
                const char* bodyBegin = strnstr(request3.data(), "\r\n\r\n", request3.size()) + strlen("\r\n\r\n");
                EXPECT_STREQ(bodyBegin, "{\"source_engagement_type\":\"click\",\"source_site\":\"127.0.0.1\",\"source_id\":42,\"attributed_on_site\":\"example.com\",\"trigger_data\":12,\"version\":2}");
                done = true;
            });
            break;
        }
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto exampleURL = [NSURL URLWithString:@"https://example.com/"];
    auto attribution = adoptNS([[MockEventAttribution alloc] initWithReportEndpoint:server.request().URL destinationURL:exampleURL]);
    auto webView = adoptNS([WKWebView new]);
    webView.get()._uiEventAttribution = (UIEventAttribution *)attribution.get();
    [[webView configuration].websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [[webView configuration].websiteDataStore _allowTLSCertificateChain:@[(id)testCertificate().get()] forHost:serverURL.host];
    [webView _setPrivateClickMeasurementAttributionReportURLsForTesting:serverURL destinationURL:exampleURL completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{
            NSString *html = [NSString stringWithFormat:@"<script>fetch('%@conversionRequestBeforeRedirect',{mode:'no-cors'})</script>", serverURL];
            [webView loadHTMLString:html baseURL:exampleURL];
        }];
    }];
    Util::run(&done);
}

#if HAVE(RSA_BSSA)
TEST(EventAttribution, FraudPrevention)
{
    bool done = false;

    // Generate the server key pair.
    size_t modulusNBits = 4096;
    int error = 0;

    struct ccrng_state* rng = ccrng(&error);

    const uint8_t e[] = { 0x1, 0x00, 0x01 };
    ccrsa_full_ctx_decl(ccn_sizeof(modulusNBits), rsaPrivateKey);
    error = ccrsa_generate_key(modulusNBits, rsaPrivateKey, sizeof(e), e, rng);

    ccrsa_pub_ctx_t rsaPublicKey = ccrsa_ctx_public(rsaPrivateKey);
    size_t modulusNBytes = cc_ceiling(ccrsa_pubkeylength(rsaPublicKey), 8);

    size_t exportSize = ccder_encode_rsa_pub_size(rsaPublicKey);
    auto publicKey = adoptNS([[NSMutableData alloc] initWithLength:exportSize]);
    ccder_encode_rsa_pub(rsaPublicKey, static_cast<uint8_t*>([publicKey mutableBytes]), static_cast<uint8_t*>([publicKey mutableBytes]) + [publicKey length]);

    auto secKey = adoptCF(SecKeyCreateWithData((__bridge CFDataRef)publicKey.get(), (__bridge CFDictionaryRef)@{
        (__bridge id)kSecAttrKeyType: (__bridge id)kSecAttrKeyTypeRSA,
        (__bridge id)kSecAttrKeyClass: (__bridge id)kSecAttrKeyClassPublic
    }, nil));

    auto spkiData = adoptCF(SecKeyCopySubjectPublicKeyInfo(secKey.get()));
    auto *nsSpkiData = (__bridge NSData *)spkiData.get();

    auto keyData = WTF::base64URLEncode(nsSpkiData.bytes, nsSpkiData.length);

    // The server.
    HTTPServer server([&done, connectionCount = 0, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &secKey] (Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] (Vector<char>&& request1) {
                EXPECT_TRUE(strnstr(request1.data(), "GET / HTTP/1.1\r\n", request1.size()));

                // Example response: { "token_public_key": "ABCD" }. "ABCD" should be Base64URL encoded.
                auto response = makeString("HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: ", 24 + keyData.length(), "\r\n\r\n"
                    "{\"token_public_key\": \"", keyData, "\"}");
                connection.send(WTFMove(response), [connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] {
                    connection.receiveHTTPRequest([connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] (Vector<char>&& request2) {
                        EXPECT_TRUE(strnstr(request2.data(), "POST / HTTP/1.1\r\n", request2.size()));

                        auto request2String = String(request2.data());
                        auto key = String("source_unlinkable_token");
                        auto start = request2String.find(key);
                        start += key.length() + 3;
                        auto end = request2String.find('"', start);
                        auto token = request2String.substring(start, end - start);

                        Vector<uint8_t> blindedMessage;
                        base64URLDecode(token, blindedMessage);

                        const struct ccrsabssa_ciphersuite *ciphersuite = &ccrsabssa_ciphersuite_rsa4096_sha384;
                        auto blindedSignature = adoptNS([[NSMutableData alloc] initWithLength: modulusNBytes]);
                        ccrsabssa_sign_blinded_message(ciphersuite, rsaPrivateKey, blindedMessage.data(), blindedMessage.size(), static_cast<uint8_t *>([blindedSignature mutableBytes]), [blindedSignature length], rng);
                        auto unlinkableToken = WTF::base64URLEncode([blindedSignature bytes], [blindedSignature length]);

                        // Example response: { "unlinkable_token": "ABCD" }. "ABCD" should be Base64URL encoded.
                        auto response = makeString("HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: ", 24 + unlinkableToken.length(), "\r\n\r\n"
                            "{\"unlinkable_token\": \"", unlinkableToken, "\"}");
                        connection.send(WTFMove(response), [connection, &keyData, &done, unlinkableToken, token, &secKey] {
                            connection.receiveHTTPRequest([connection, &keyData, &done, unlinkableToken, token, &secKey] (Vector<char>&& request3) {
                                EXPECT_TRUE(strnstr(request3.data(), "GET / HTTP/1.1\r\n", request3.size()));

                                // Example response: { "token_public_key": "ABCD" }. "ABCD" should be Base64URL encoded.
                                auto response = makeString("HTTP/1.1 200 OK\r\n"
                                    "Content-Type: application/json\r\n"
                                    "Content-Length: ", 24 + keyData.length(), "\r\n\r\n"
                                    "{\"token_public_key\": \"", keyData, "\"}");
                                connection.send(WTFMove(response), [connection, &done, unlinkableToken, token, &secKey] {
                                    connection.receiveHTTPRequest([connection, &done, unlinkableToken, token, &secKey] (Vector<char>&& request4) {
                                        EXPECT_TRUE(strnstr(request4.data(), "POST / HTTP/1.1\r\n", request4.size()));
                                        EXPECT_TRUE(strnstr(request4.data(), "{\"source_engagement_type\":\"click\",\"source_site\":\"127.0.0.1\",\"source_id\":42,\"attributed_on_site\":\"example.com\",\"trigger_data\":12,\"version\":2,",
                                            request4.size()));

                                        EXPECT_FALSE(strnstr(request4.data(), token.utf8().data(), request4.size()));
                                        EXPECT_FALSE(strnstr(request4.data(), unlinkableToken.utf8().data(), request4.size()));

                                        auto request4String = String(request4.data());

                                        auto key = String("source_secret_token");
                                        auto start = request4String.find(key);
                                        start += key.length() + 3;
                                        auto end = request4String.find('"', start);
                                        auto token = request4String.substring(start, end - start);
                                        Vector<uint8_t> tokenVector;
                                        base64URLDecode(token, tokenVector);
                                        auto tokenData = adoptNS([[NSData alloc] initWithBytes:tokenVector.data() length:tokenVector.size()]);

                                        key = String("source_secret_token_signature");
                                        start = request4String.find(key);
                                        start += key.length() + 3;
                                        end = request4String.find('"', start);
                                        auto signature = request4String.substring(start, end - start);
                                        Vector<uint8_t> signatureVector;
                                        base64URLDecode(signature, signatureVector);
                                        auto signatureData = adoptNS([[NSData alloc] initWithBytes:signatureVector.data() length:signatureVector.size()]);

                                        EXPECT_TRUE(SecKeyVerifySignature(secKey.get(), kSecKeyAlgorithmRSASignatureMessagePSSSHA384, (__bridge CFDataRef)tokenData.get(), (__bridge CFDataRef)signatureData.get(), NULL));

                                        done = true;
                                    });
                                });
                            });
                        });
                    });
                });
            });
            break;
        case 2:
            connection.receiveHTTPRequest([connection] (Vector<char>&& request1) {
                EXPECT_TRUE(strnstr(request1.data(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n", request1.size()));
                const char* redirect = "HTTP/1.1 302 Found\r\n"
                    "Location: /.well-known/private-click-measurement/trigger-attribution/12\r\n"
                    "Content-Length: 0\r\n\r\n";
                connection.send(redirect, [connection] {
                    connection.receiveHTTPRequest([connection] (Vector<char>&& request2) {
                        EXPECT_TRUE(strnstr(request2.data(), "GET /.well-known/private-click-measurement/trigger-attribution/12 HTTP/1.1\r\n", request2.size()));
                        const char* response = "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 0\r\n\r\n";
                        connection.send(response);
                    });
                });
            });
            break;
        }
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto exampleURL = [NSURL URLWithString:@"https://example.com/"];
    auto attribution = adoptNS([[MockEventAttribution alloc] initWithReportEndpoint:serverURL destinationURL:exampleURL]);
    auto webView = adoptNS([WKWebView new]);
    [webView _setUIEventAttributionForTesting:(UIEventAttribution *)attribution.get() withNonce:@"ABCDEFabcdef0123456789"];
    [[webView configuration].websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [[webView configuration].websiteDataStore _allowTLSCertificateChain:@[(id)testCertificate().get()] forHost:serverURL.host];

    [webView _setPrivateClickMeasurementAttributionReportURLsForTesting:serverURL destinationURL:exampleURL completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{
            [webView _setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:serverURL completionHandler:^{
                [webView _setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:serverURL completionHandler:^{
                    NSString *html = [NSString stringWithFormat:@"<script>setTimeout(function(){ fetch('%@conversionRequestBeforeRedirect',{mode:'no-cors'}); }, 100);</script>", serverURL];
                    [webView loadHTMLString:html baseURL:exampleURL];
                }];
            }];
        }];
    }];
    Util::run(&done);
}
#endif

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)

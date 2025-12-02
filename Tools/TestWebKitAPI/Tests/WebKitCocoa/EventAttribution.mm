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

#import "ASN1Utilities.h"
#import "DaemonTestUtilities.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKMain.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPageInjectedBundleClient.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/MakeString.h>

#if HAVE(RSA_BSSA)

#include "CoreCryptoSPI.h"

#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/darwin/XPCObjectPtr.h>
#include <wtf/spi/cocoa/SecuritySPI.h>

#endif // HAVE(RSA_BSSA)

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

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

static RetainPtr<SecTrustRef> secTrustFromCertificateChain(NSArray *chain)
{
    SecTrustRef trustRef = nullptr;
    if (SecTrustCreateWithCertificates((__bridge CFArrayRef)chain, nullptr, &trustRef) != noErr)
        return nullptr;
    return adoptCF(trustRef);
}

static TestNavigationDelegate *delegateAllowingAllTLS()
{
    static RetainPtr<TestNavigationDelegate> delegate = [] {
        auto delegate = adoptNS([TestNavigationDelegate new]);
        delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };
        return delegate;
    }();
    return delegate.get();
}

static NSURL *exampleURL()
{
    return [NSURL URLWithString:@"https://example.com/"];
}

static RetainPtr<WKWebViewConfiguration> configurationWithoutUsingDaemon()
{
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().pcmMachServiceName = nil;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get();
    return configuration;
}

static RetainPtr<WKWebView> webViewWithoutUsingDaemon()
{
    return adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configurationWithoutUsingDaemon().get()]);
}

void runBasicPCMTest(WKWebViewConfiguration *configuration, Function<void(WKWebView *, const HTTPServer&)>&& addAttributionToWebView, bool setTestAppBundleID = true)
{
    [WKWebsiteDataStore _setNetworkProcessSuspensionAllowedForTesting:NO];
    bool done = false;
    HTTPServer server([&done, connectionCount = 0] (Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([connection] (Vector<char>&& request1) {
                EXPECT_TRUE(contains(request1.span(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n"_span));
                constexpr auto redirect = "HTTP/1.1 302 Found\r\n"
                    "Location: /.well-known/private-click-measurement/trigger-attribution/12\r\n"
                    "Content-Length: 0\r\n\r\n"_s;
                connection.send(redirect, [connection] {
                    connection.receiveHTTPRequest([connection] (Vector<char>&& request2) {
                        EXPECT_TRUE(contains(request2.span(), "GET /.well-known/private-click-measurement/trigger-attribution/12 HTTP/1.1\r\n"_span));
                        constexpr auto response = "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 0\r\n\r\n"_s;
                        connection.send(response);
                    });
                });
            });
            break;
        case 2:
            connection.receiveHTTPRequest([&done] (Vector<char>&& request3) {
                EXPECT_TRUE(contains(request3.span(), "POST / HTTP/1.1\r\n"_span));
                size_t bodyBegin = find(request3.span(), "\r\n\r\n"_span) + strlen("\r\n\r\n");
                EXPECT_TRUE(equalSpans(request3.subspan(bodyBegin), "{\"source_engagement_type\":\"click\",\"source_site\":\"127.0.0.1\",\"source_id\":42,\"attributed_on_site\":\"example.com\",\"trigger_data\":12,\"version\":3}"_span));
                done = true;
            });
            break;
        }
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto webView = configuration ? adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]) : webViewWithoutUsingDaemon();
    webView.get().navigationDelegate = delegateAllowingAllTLS();
    addAttributionToWebView(webView.get(), server);
    [[webView configuration].websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [[webView configuration].websiteDataStore _trustServerForLocalPCMTesting:secTrustFromCertificateChain(@[(id)testCertificate().get()]).get()];
    // Clear existing data.
    __block bool cleared = false;
    [[webView configuration].websiteDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        cleared = true;
    }];
    Util::run(&cleared);
    [webView _setPrivateClickMeasurementAttributionReportURLsForTesting:serverURL destinationURL:exampleURL() completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{
            NSString *html = [NSString stringWithFormat:@"<script>fetch('%@conversionRequestBeforeRedirect',{mode:'no-cors'})</script>", serverURL];
            if (setTestAppBundleID) {
                [webView _setPrivateClickMeasurementAppBundleIDForTesting:@"test.bundle.id" completionHandler:^{
                    [webView loadHTMLString:html baseURL:exampleURL()];
                }];
            } else
                [webView loadHTMLString:html baseURL:exampleURL()];
        }];
    }];
    Util::run(&done);
}

#if PLATFORM(MAC)
void runManualPCMTest(WKWebViewConfiguration *configuration, NSURL *sourceURL, NSURL *destinationURL, Function<void(WKWebView *, const HTTPServer&)>&& addAttributionToWebView, bool setTestAppBundleID = true)
{
    [WKWebsiteDataStore _setNetworkProcessSuspensionAllowedForTesting:NO];
    bool done = false;
    HTTPServer server([&done] (Connection connection) mutable {
        connection.receiveHTTPRequest([&done] (Vector<char>&& request) {
            EXPECT_FALSE(contains(request.span(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n"_span));
            EXPECT_TRUE(contains(request.span(), "POST / HTTP/1.1\r\n"_span));
            size_t bodyBegin = find(request.span(), "\r\n\r\n"_span) + strlen("\r\n\r\n");
            EXPECT_TRUE(equalSpans(request.subspan(bodyBegin), "{\"source_engagement_type\":\"click\",\"source_site\":\"pcmsource.example\",\"source_id\":42,\"attributed_on_site\":\"pcmdestination.example\",\"trigger_data\":12,\"version\":3}"_span));
            done = true;
        });
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto webView = configuration ? adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]) : webViewWithoutUsingDaemon();
    webView.get().navigationDelegate = delegateAllowingAllTLS();
    [[webView configuration].websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [[webView configuration].websiteDataStore _trustServerForLocalPCMTesting:secTrustFromCertificateChain(@[(id)testCertificate().get()]).get()];
    // Clear existing data.
    __block bool cleared = false;
    [[webView configuration].websiteDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        cleared = true;
    }];
    Util::run(&cleared);
    addAttributionToWebView(webView.get(), server);
    [webView _setPrivateClickMeasurementAttributionReportURLsForTesting:serverURL destinationURL:exampleURL() completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{

            if (setTestAppBundleID) {
                [webView _setPrivateClickMeasurementAppBundleIDForTesting:@"test.bundle.id" completionHandler:^{
                    [webView _storeSimulatedPrivateClickMeasurementConversionWithPriority:0 triggerData:12 sourceURL:sourceURL destinationURL:destinationURL];
                }];
            } else
                [webView _storeSimulatedPrivateClickMeasurementConversionWithPriority:0 triggerData:12 sourceURL:sourceURL destinationURL:destinationURL];
        }];
    }];
    Util::run(&done);
}
#endif

#if HAVE(RSA_BSSA)

enum class TokenSigningParty : bool { Source, Destination };

static void triggerAttributionWithSubresourceRedirect(Connection& connection, const String& attributionDestinationNonce)
{
    auto optionalQueryString = attributionDestinationNonce.isEmpty() ? attributionDestinationNonce : makeString("?attributionDestinationNonce="_s, attributionDestinationNonce);
    auto location = makeString("/.well-known/private-click-measurement/trigger-attribution/12"_s, optionalQueryString);
    connection.receiveHTTPRequest([connection, location] (Vector<char>&& request1) {
        EXPECT_TRUE(contains(request1.span(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n"_span));
        auto redirect = makeString("HTTP/1.1 302 Found\r\nLocation: "_s, location, "\r\nContent-Length: 0\r\n\r\n"_s);
        connection.send(WTFMove(redirect), [connection, location] {
            connection.receiveHTTPRequest([connection, location] (Vector<char>&& request2) {
                auto expectedHttpGetString = makeString("GET "_s, location, " HTTP/1.1\r\n"_s).utf8();
                EXPECT_TRUE(contains(request2.span(), expectedHttpGetString.span()));
                constexpr auto response = "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 0\r\n\r\n"_s;
                connection.send(response);
            });
        });
    });
}

static void runDestinationWebView(RetainPtr<WKWebView> webView, NSURL *serverURL)
{
    [webView _setPrivateClickMeasurementAttributionReportURLsForTesting:serverURL destinationURL:exampleURL() completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{
            [webView _setPrivateClickMeasurementAttributionTokenPublicKeyURLForTesting:serverURL completionHandler:^{
                [webView _setPrivateClickMeasurementAttributionTokenSignatureURLForTesting:serverURL completionHandler:^{
                    [webView _setPrivateClickMeasurementAppBundleIDForTesting:@"test.bundle.id" completionHandler:^{
                        NSString *html = [NSString stringWithFormat:@"<script>setTimeout(function(){ fetch('%@conversionRequestBeforeRedirect',{mode:'no-cors'}); }, 100);</script>", serverURL];
                        [webView loadHTMLString:html baseURL:exampleURL()];
                    }];
                }];
            }];
        }];
    }];
}

static void signUnlinkableTokenAndSendSecretToken(TokenSigningParty signingParty)
{
    [WKWebsiteDataStore _setNetworkProcessSuspensionAllowedForTesting:NO];
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
    auto wrappedKeyBytes = wrapPublicKeyWithRSAPSSOID(makeVector(publicKey.get()));

    auto keyData = base64URLEncodeToString(wrappedKeyBytes);
    // The server.
    HTTPServer server([signingParty, &done, connectionCount = signingParty == TokenSigningParty::Source ? 1 : 0, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &secKey] (Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            EXPECT_TRUE(signingParty == TokenSigningParty::Destination);
            triggerAttributionWithSubresourceRedirect(connection, "ABCDEFabcdef0123456789"_s);
            break;
        case 2:
            connection.receiveHTTPRequest([signingParty, connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] (Vector<char>&& request1) {
                EXPECT_TRUE(contains(request1.span(), "GET / HTTP/1.1\r\n"_span));

                // Example response: { "token_public_key": "ABCD" }. "ABCD" should be Base64URL encoded.
                auto response = makeString("HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: "_s, 24 + keyData.length(), "\r\n\r\n"
                    "{\"token_public_key\": \""_s, keyData, "\"}"_s);
                connection.send(WTFMove(response), [signingParty, connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] {
                    connection.receiveHTTPRequest([signingParty, connection, &rsaPrivateKey, &modulusNBytes, &rng, &keyData, &done, &secKey] (Vector<char>&& request2) {
                        EXPECT_TRUE(contains(request2.span(), "POST / HTTP/1.1\r\n"_span));

                        auto request2String = String(request2.span());
                        auto key = signingParty == TokenSigningParty::Source ? "source_unlinkable_token"_s : "destination_unlinkable_token"_s;
                        auto start = request2String.find(key);
                        start += key.length() + 3;
                        auto end = request2String.find('"', start);
                        auto token = request2String.substring(start, end - start);

                        auto blindedMessage = base64URLDecode(token);

                        const struct ccrsabssa_ciphersuite *ciphersuite = &ccrsabssa_ciphersuite_rsa4096_sha384;
                        auto blindedSignature = adoptNS([[NSMutableData alloc] initWithLength:modulusNBytes]);
                        ccrsabssa_sign_blinded_message(ciphersuite, rsaPrivateKey, blindedMessage->span().data(), blindedMessage->size(), static_cast<uint8_t *>([blindedSignature mutableBytes]), [blindedSignature length], rng);
                        auto unlinkableToken = base64URLEncodeToString(span(blindedSignature.get()));

                        // Example response: { "unlinkable_token": "ABCD" }. "ABCD" should be Base64URL encoded.
                        auto response = makeString("HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: "_s, 24 + unlinkableToken.length(), "\r\n\r\n"
                            "{\"unlinkable_token\": \""_s, unlinkableToken, "\"}"_s);
                        connection.send(WTFMove(response), [signingParty, connection, &keyData, &done, unlinkableToken, token, &secKey] {
                            connection.receiveHTTPRequest([signingParty, connection, &keyData, &done, unlinkableToken, token, &secKey] (Vector<char>&& request3) {
                                EXPECT_TRUE(contains(request3.span(), "GET / HTTP/1.1\r\n"_span));

                                // Example response: { "token_public_key": "ABCD" }. "ABCD" should be Base64URL encoded.
                                auto response = makeString("HTTP/1.1 200 OK\r\n"
                                    "Content-Type: application/json\r\n"
                                    "Content-Length: "_s, 24 + keyData.length(), "\r\n\r\n"
                                    "{\"token_public_key\": \""_s, keyData, "\"}"_s);
                                connection.send(WTFMove(response), [signingParty, connection, &done, unlinkableToken, token, &secKey] {
                                    connection.receiveHTTPRequest([signingParty, connection, &done, unlinkableToken, token, &secKey] (Vector<char>&& request4) {
                                        EXPECT_TRUE(contains(request4.span(), "POST / HTTP/1.1\r\n"_span));
                                        EXPECT_TRUE(contains(request4.span(), "{\"source_engagement_type\":\"click\",\"source_site\":\"127.0.0.1\",\"source_id\":42,\"attributed_on_site\":\"example.com\",\"trigger_data\":12,\"version\":3,"_span));

                                        EXPECT_FALSE(contains(request4.span(), token.utf8().span()));
                                        EXPECT_FALSE(contains(request4.span(), unlinkableToken.utf8().span()));

                                        auto request4String = String(request4.span());

                                        auto key = signingParty == TokenSigningParty::Source ? "source_secret_token"_s : "destination_secret_token"_s;
                                        auto start = request4String.find(key);
                                        start += key.length() + 3;
                                        auto end = request4String.find('"', start);
                                        auto token = request4String.substring(start, end - start);
                                        auto tokenVector = base64URLDecode(token);
                                        RetainPtr tokenData = toNSData(tokenVector->span());

                                        key = signingParty == TokenSigningParty::Source ? "source_secret_token_signature"_s : "destination_secret_token_signature"_s;
                                        start = request4String.find(key);
                                        start += key.length() + 3;
                                        end = request4String.find('"', start);
                                        auto signature = request4String.substring(start, end - start);
                                        auto signatureVector = base64URLDecode(signature);
                                        RetainPtr signatureData = toNSData(signatureVector->span());

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
        case 3:
            EXPECT_TRUE(signingParty == TokenSigningParty::Source);
            triggerAttributionWithSubresourceRedirect(connection, emptyString());
            break;
        }
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto webView = webViewWithoutUsingDaemon();
    webView.get().navigationDelegate = delegateAllowingAllTLS();
    auto clickNonce = signingParty == TokenSigningParty::Source ? @"ABCDEFabcdef0123456789" : nil;
    [webView _addEventAttributionWithSourceID:42 destinationURL:exampleURL() sourceDescription:@"test source description" purchaser:@"test purchaser" reportEndpoint:serverURL optionalNonce:clickNonce applicationBundleID:@"test.bundle.id" ephemeral:NO];
    [[webView configuration].websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [[webView configuration].websiteDataStore _trustServerForLocalPCMTesting:secTrustFromCertificateChain(@[(id)testCertificate().get()]).get()];

    runDestinationWebView(webView, serverURL);
    Util::run(&done);
}

TEST(PrivateClickMeasurement, SourceClickFraudPrevention)
{
    signUnlinkableTokenAndSendSecretToken(TokenSigningParty::Source);
}

TEST(PrivateClickMeasurement, DestinationClickFraudPrevention)
{
    signUnlinkableTokenAndSendSecretToken(TokenSigningParty::Destination);
}
#endif

TEST(PrivateClickMeasurement, Basic)
{
    runBasicPCMTest(nil, [](WKWebView *webView, const HTTPServer& server) {
        [webView _addEventAttributionWithSourceID:42 destinationURL:exampleURL() sourceDescription:@"test source description" purchaser:@"test purchaser" reportEndpoint:server.request().URL optionalNonce:nil applicationBundleID:@"test.bundle.id" ephemeral:NO];
    });
}

#if PLATFORM(MAC)
TEST(PrivateClickMeasurement, ManualAttributionAndConversion)
{
    NSURL *sourceURL = [NSURL URLWithString:@"https://site.pcmsource.example/"];
    NSURL *destinationURL = [NSURL URLWithString:@"https://site.pcmdestination.example/"];
    runManualPCMTest(nil, sourceURL, destinationURL, [&sourceURL, &destinationURL](WKWebView *webView, const HTTPServer& server) {
        [webView _storePrivateClickMeasurementWithSourceID:42 destinationURL:destinationURL reportEndpoint:sourceURL];
    }, false);
}
#endif

TEST(PrivateClickMeasurement, EphemeralWithAttributedBundleIdentifier)
{
    auto configuration = configurationWithoutUsingDaemon();
    configuration.get()._attributedBundleIdentifier = @"other.test.bundle.id";
    runBasicPCMTest(configuration.get(), [](WKWebView *webView, const HTTPServer& server) {
        [webView _addEventAttributionWithSourceID:42 destinationURL:exampleURL() sourceDescription:@"test source description" purchaser:@"test purchaser" reportEndpoint:server.request().URL optionalNonce:nil applicationBundleID:@"other.test.bundle.id" ephemeral:YES];
    }, false);
}

TEST(PrivateClickMeasurement, DatabaseLocation)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"EventAttributionDatabaseLocationTest"] isDirectory:YES];
    if ([fileManager fileExistsAtPath:tempDir.path])
        [fileManager removeItemAtURL:tempDir error:nil];

    auto webViewToKeepNetworkProcessAlive = adoptNS([TestWKWebView new]);
    [webViewToKeepNetworkProcessAlive synchronouslyLoadHTMLString:@"start network process"];

    pid_t originalNetworkProcessPid = 0;
    @autoreleasepool {
        auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
        dataStoreConfiguration.get()._resourceLoadStatisticsDirectory = tempDir;
        dataStoreConfiguration.get().pcmMachServiceName = nil;
        auto viewConfiguration = configurationWithoutUsingDaemon();
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
        viewConfiguration.get().websiteDataStore = dataStore.get();
        runBasicPCMTest(viewConfiguration.get(), [](WKWebView *webView, const HTTPServer& server) {
            [webView _addEventAttributionWithSourceID:42 destinationURL:exampleURL() sourceDescription:@"test source description" purchaser:@"test purchaser" reportEndpoint:server.request().URL optionalNonce:nil applicationBundleID:@"test.bundle.id" ephemeral:NO];
        });
        originalNetworkProcessPid = [dataStore _networkProcessIdentifier];
        EXPECT_GT(originalNetworkProcessPid, 0);

        __block bool suspended = false;
        [WKWebsiteDataStore _setNetworkProcessSuspensionAllowedForTesting:YES];
        [dataStore _sendNetworkProcessPrepareToSuspend:^{
            suspended = true;
            [dataStore _sendNetworkProcessDidResume];
        }];
        Util::run(&suspended);
        Util::spinRunLoop(10);
    }
    Util::spinRunLoop(10);
    usleep(100000);
    Util::spinRunLoop(10);

    EXPECT_TRUE([fileManager fileExistsAtPath:tempDir.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:[tempDir.path stringByAppendingPathComponent:@"pcm.db"]]);

    [webViewToKeepNetworkProcessAlive synchronouslyLoadHTMLString:@"start network process again if it crashed during teardown"];
    EXPECT_EQ(webViewToKeepNetworkProcessAlive.get().configuration.websiteDataStore._networkProcessIdentifier, originalNetworkProcessPid);
}

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)

static RetainPtr<NSURL> testPCMDaemonLocation()
{
    return [currentExecutableDirectory() URLByAppendingPathComponent:@"adattributiond" isDirectory:NO];
}

static NSDictionary<NSString *, id> *testDaemonPList(NSURL *storageLocation)
{
    return @{
        @"Label" : @"org.webkit.pcmtestdaemon",
        @"LaunchOnlyOnce" : @YES,
        @"StandardErrorPath" : [storageLocation URLByAppendingPathComponent:@"daemon_stderr"].path,
        @"EnvironmentVariables" : @{ @"DYLD_FRAMEWORK_PATH" : currentExecutableDirectory().get().path },
        @"MachServices" : @{ @"org.webkit.pcmtestdaemon.service" : @YES },
        @"ProgramArguments" : @[
            testPCMDaemonLocation().get().path,
            @"--machServiceName",
            @"org.webkit.pcmtestdaemon.service",
            @"--storageLocation",
            storageLocation.path
        ]
    };
}

static std::pair<NSURL *, WKWebViewConfiguration *> setUpDaemon(WKWebViewConfiguration *viewConfiguration)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"EventAttributionDaemonTest"] isDirectory:YES];
    NSError *error = nil;
    if ([fileManager fileExistsAtPath:tempDir.path])
        [fileManager removeItemAtURL:tempDir error:&error];
    EXPECT_NULL(error);

    killFirstInstanceOfDaemon(@"adattributiond");

    registerPlistWithLaunchD(testDaemonPList(tempDir), tempDir);

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().pcmMachServiceName = @"org.webkit.pcmtestdaemon.service";
    viewConfiguration.websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get();

    return std::make_pair(tempDir, viewConfiguration);
}

static void cleanUpDaemon(NSURL *tempDir)
{
    killFirstInstanceOfDaemon(@"adattributiond");

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:tempDir.path]);
    NSError *error = nil;
    [[NSFileManager defaultManager] removeItemAtURL:tempDir error:&error];
    EXPECT_NULL(error);
}

static void attemptConnectionInProcessWithoutEntitlement()
{
#if USE(APPLE_INTERNAL_SDK)
    __block bool done = false;
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto connection = adoptXPCObject(xpc_connection_create_mach_service("org.webkit.pcmtestdaemon.service", mainDispatchQueueSingleton(), 0));
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t event) {
        EXPECT_EQ(event, XPC_ERROR_CONNECTION_INTERRUPTED);
        done = true;
    });
    xpc_connection_activate(connection.get());
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto dictionary = adoptXPCObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_connection_send_message(connection.get(), dictionary.get());
    TestWebKitAPI::Util::run(&done);
#endif
}

// FIXME rdar://97553050
#if PLATFORM(IOS)
TEST(PrivateClickMeasurement, DISABLED_DaemonBasicFunctionality)
#else
TEST(PrivateClickMeasurement, DaemonBasicFunctionality)
#endif
{
    auto [tempDir, configuration] = setUpDaemon(configurationWithoutUsingDaemon().autorelease());
    attemptConnectionInProcessWithoutEntitlement();
    runBasicPCMTest(configuration, [](WKWebView *webView, const HTTPServer& server) {
        [webView _addEventAttributionWithSourceID:42 destinationURL:exampleURL() sourceDescription:@"test source description" purchaser:@"test purchaser" reportEndpoint:server.request().URL optionalNonce:nil applicationBundleID:@"test.bundle.id" ephemeral:NO];
    });
    cleanUpDaemon(tempDir);
}

#if PLATFORM(MAC)
static RetainPtr<TestWKWebView> webViewWithOpenInspector(WKWebViewConfiguration *configuration, id<WKUIDelegate> uiDelegate)
{
    configuration.preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"start processes"];
    [[webView _inspector] show];
    [webView _test_waitForInspectorToShow];
    [webView setUIDelegate:uiDelegate];
    return webView;
}

TEST(PrivateClickMeasurement, DaemonDebugMode)
{
    auto [tempDir, configuration] = setUpDaemon(adoptNS([WKWebViewConfiguration new]).autorelease());
    configuration._shouldSendConsoleLogsToUIProcessForTesting = YES;
    __block Vector<String> consoleMessages;
    auto delegate = adoptNS([TestUIDelegate new]);
    delegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        consoleMessages.append(log);
    };
    auto webView = webViewWithOpenInspector(configuration, delegate.get());

    [configuration.websiteDataStore _setPrivateClickMeasurementDebugModeEnabled:YES];
    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], "[Private Click Measurement] Turned Debug Mode on.");
    [configuration.websiteDataStore _setPrivateClickMeasurementDebugModeEnabled:NO];
    while (consoleMessages.size() < 2)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[1], "[Private Click Measurement] Turned Debug Mode off.");
    cleanUpDaemon(tempDir);
}

static void setupSKAdNetworkTest(Vector<String>& consoleMessages, id<WKNavigationDelegate> navigationDelegate, NSString *html, id<WKUIDelegate> uiDelegate)
{
    HTTPServer server({ { "/app/id1234567890"_s, { "hello"_s } } }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];

    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    viewConfiguration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get();
    viewConfiguration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    auto webView = webViewWithOpenInspector(viewConfiguration.get(), uiDelegate);

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SKAttributionEnabled"]) {
            [[viewConfiguration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    [viewConfiguration.get().websiteDataStore _setPrivateClickMeasurementDebugModeEnabled:YES];

    [webView synchronouslyLoadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com/"]];

    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], "[Private Click Measurement] Turned Debug Mode on.");
    consoleMessages.clear();

    webView.get().navigationDelegate = navigationDelegate;

    [webView clickOnElementID:@"anchorid"];
}

const char* expectedSKAdNetworkConsoleMessage = "Submitting potential install attribution for AdamId: 1234567890, adNetworkRegistrableDomain: destination, impressionId: MTIzNDU2Nzg5MDEyMzQ1Ng, sourceWebRegistrableDomain: example.com, version: 3";
static NSString *linkToAppStoreHTML = @"<body><a href='https://apps.apple.com/app/id1234567890' id='anchorid' attributiondestination='https://destination/' attributionSourceNonce='MTIzNDU2Nzg5MDEyMzQ1Ng'>anchor</a></body>";

// rdar://129248776
#if defined(NDEBUG)
TEST(PrivateClickMeasurement, DISABLED_SKAdNetwork)
#else
TEST(PrivateClickMeasurement, SKAdNetwork)
#endif
{
    __block Vector<String> consoleMessages;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink);
    };
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        consoleMessages.append(log);
    };
    setupSKAdNetworkTest(consoleMessages, delegate.get(), linkToAppStoreHTML, uiDelegate.get());
    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], expectedSKAdNetworkConsoleMessage);
}

// rdar://129248776
#if defined(NDEBUG)
TEST(PrivateClickMeasurement, DISABLED_SKAdNetworkAboutBlank)
#else
TEST(PrivateClickMeasurement, SKAdNetworkAboutBlank)
#endif
{
    __block Vector<String> consoleMessages;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    __block RetainPtr<TestWKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().navigationDelegate = delegate.get();
        return openedWebView.get();
    };
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        consoleMessages.append(log);
    };
    [delegate allowAnyTLSCertificate];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink);
    };
    NSString *linkToAppStoreHTMLWithAboutBlank = @"<body>"
    "    <a target='_blank' href='https://apps.apple.com/app/id1234567890' id='anchorid' attributiondestination='https://destination/' attributionSourceNonce='MTIzNDU2Nzg5MDEyMzQ1Ng'>anchor</a>"
    "</body>";
    setupSKAdNetworkTest(consoleMessages, delegate.get(), linkToAppStoreHTMLWithAboutBlank, uiDelegate.get());
    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], expectedSKAdNetworkConsoleMessage);
}

// rdar://129248776
#if defined(NDEBUG)
TEST(PrivateClickMeasurement, DISABLED_SKAdNetworkWithoutNavigatingToAppStoreLink)
#else
TEST(PrivateClickMeasurement, SKAdNetworkWithoutNavigatingToAppStoreLink)
#endif
{
    __block Vector<String> consoleMessages;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyCancel);
        Util::runFor(0.1_s);
        EXPECT_EQ(0u, consoleMessages.size());
        [navigationAction _storeSKAdNetworkAttribution];
    };
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        consoleMessages.append(log);
    };
    setupSKAdNetworkTest(consoleMessages, delegate.get(), linkToAppStoreHTML, uiDelegate.get());

    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], expectedSKAdNetworkConsoleMessage);
}

TEST(PrivateClickMeasurement, NetworkProcessDebugMode)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    __block Vector<String> consoleMessages;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        consoleMessages.append(log);
    };
    auto webView = webViewWithOpenInspector(configuration.get(), uiDelegate.get());
    [configuration.get().websiteDataStore _setPrivateClickMeasurementDebugModeEnabled:YES];
    while (consoleMessages.isEmpty())
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[0], "[Private Click Measurement] Turned Debug Mode on.");
    [configuration.get().websiteDataStore _setPrivateClickMeasurementDebugModeEnabled:NO];
    while (consoleMessages.size() < 2)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(consoleMessages[1], "[Private Click Measurement] Turned Debug Mode off.");
}
#endif // PLATFORM(MAC)

#endif // PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)

#if HAVE(UI_EVENT_ATTRIBUTION)

TEST(PrivateClickMeasurement, BasicWithIOSSPI)
{
    runBasicPCMTest(nil, [](WKWebView *webView, const HTTPServer& server) {
        auto attribution = adoptNS([[MockEventAttribution alloc] initWithReportEndpoint:server.request().URL destinationURL:exampleURL()]);
        webView._uiEventAttribution = (UIEventAttribution *)attribution.get();
        EXPECT_WK_STREQ(webView._uiEventAttribution.sourceDescription, "test source description");
        EXPECT_WK_STREQ(webView._uiEventAttribution.purchaser, "test purchaser");
    });
}

TEST(PrivateClickMeasurement, BasicWithEphemeralIOSSPI)
{
    runBasicPCMTest(nil, [](WKWebView *webView, const HTTPServer& server) {
        auto attribution = adoptNS([[MockEventAttribution alloc] initWithReportEndpoint:server.request().URL destinationURL:exampleURL()]);
        webView._ephemeralUIEventAttribution = (UIEventAttribution *)attribution.get();
    });
}

#endif // HAVE(UI_EVENT_ATTRIBUTION)

} // namespace TestWebKitAPI

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

#if HAVE(CFNETWORK_COMPRESSION_DICTIONARY)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringToIntegerConversion.h>

namespace TestWebKitAPI::CompressionDictionaryTests {

// The dictionary and dictionary-compressed bodies from
// LayoutTests/imported/w3c/web-platform-tests/fetch/compression-dictionary/resources/compressed-data.py.
static constexpr auto dictionary = "This is a test dictionary.\n"_s;
static constexpr auto decodedData = "This is compressed test data using a test dictionary"_s;
static constexpr auto availableDictionary = ":U5abz16WDg7b8KS93msLPpOB4Vbef1uRzoORYkJw9BY=:"_s;
static constexpr auto noDictionary = "no dictionary"_s;

static constexpr uint8_t dcbData[] = {
    0xff, 0x44, 0x43, 0x42, 0x53, 0x96, 0x9b, 0xcf, 0x5e, 0x96, 0x0e, 0x0e, 0xdb, 0xf0, 0xa4, 0xbd, 0xde, 0x6b, 0x0b, 0x3e,
    0x93, 0x81, 0xe1, 0x56, 0xde, 0x7f, 0x5b, 0x91, 0xce, 0x83, 0x91, 0x62, 0x42, 0x70, 0xf4, 0x16, 0xa1, 0x98, 0x01, 0x80,
    0x62, 0xa4, 0x4c, 0x1d, 0xdf, 0x12, 0x84, 0x8c, 0xae, 0xc2, 0xca, 0x60, 0x22, 0x07, 0x6e, 0x81, 0x05, 0x14, 0xc9, 0xb7,
    0xc3, 0x44, 0x8e, 0xbc, 0x16, 0xe0, 0x15, 0x0e, 0xec, 0xc1, 0xee, 0x34, 0x33, 0x3e, 0x0d
};

static constexpr uint8_t dczData[] = {
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00, 0x53, 0x96, 0x9b, 0xcf, 0x5e, 0x96, 0x0e, 0x0e, 0xdb, 0xf0, 0xa4, 0xbd,
    0xde, 0x6b, 0x0b, 0x3e, 0x93, 0x81, 0xe1, 0x56, 0xde, 0x7f, 0x5b, 0x91, 0xce, 0x83, 0x91, 0x62, 0x42, 0x70, 0xf4, 0x16,
    0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x34, 0xf5, 0x00, 0x00, 0x98, 0x63, 0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64,
    0x61, 0x74, 0x61, 0x20, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x03, 0x00, 0x59, 0xf9, 0x73, 0x54, 0x46, 0x27, 0x26, 0x10, 0x9e,
    0x99, 0xf2, 0xbc
};

static constexpr auto mainPage = "<script>"
    "async function fetchText(path, options) {"
    "    try {"
    "        return await (await fetch(path, options)).text();"
    "    } catch (e) {"
    "        return 'error';"
    "    }"
    "}"
    "async function waitForDictionary() {"
    "    for (let i = 0; i < 100; ++i) {"
    "        if (await fetchText('/data/dcb') != 'no dictionary')"
    "            return true;"
    "        await new Promise(resolve => setTimeout(resolve, 50));"
    "    }"
    "    return false;"
    "}"
    "</script>"_s;

static String headerValue(const Vector<char>& request, StringView name)
{
    auto requestString = String::fromLatin1(request.span());
    for (auto line : StringView(requestString).split('\n')) {
        auto colon = line.find(':');
        if (colon == notFound)
            continue;
        if (equalIgnoringASCIICase(line.left(colon), name))
            return line.substring(colon + 1).trim(isASCIIWhitespace<char16_t>).toString();
    }
    return { };
}

static HTTPResponse textResponse(const String& text)
{
    return { { { "Content-Type"_s, "text/plain"_s }, { "Cache-Control"_s, "no-store"_s } }, text };
}

static HTTPResponse redirectResponse(const String& location)
{
    return { 302, { { "Location"_s, location }, { "Cache-Control"_s, "no-store"_s } } };
}

struct ReceivedRequest {
    String availableDictionary;
    String dictionaryID;
    String acceptEncoding;
};

struct CompressionDictionaryServer {
    HashMap<String, ReceivedRequest> requests;
    bool respondWithDCBEvenWithoutDictionary { false };
    std::unique_ptr<HTTPServer> server;

    CompressionDictionaryServer()
    {
        server = makeUnique<HTTPServer>(HTTPServer::UseCoroutines::Yes, [this](Connection connection) -> ConnectionTask {
            while (true) {
                auto request = co_await connection.awaitableReceiveHTTPRequest();
                auto path = HTTPServer::parsePath(request);
                auto received = ReceivedRequest { headerValue(request, "Available-Dictionary"_s), headerValue(request, "Dictionary-ID"_s), headerValue(request, "Accept-Encoding"_s) };
                requests.set(path, received);

                if (path == "/"_s) {
                    co_await connection.awaitableSend(HTTPResponse(mainPage).serialize());
                    continue;
                }
                if (path == "/dictionary"_s) {
                    co_await connection.awaitableSend(HTTPResponse({
                        { "Content-Type"_s, "text/plain"_s },
                        { "Cache-Control"_s, "max-age=3600"_s },
                        { "Use-As-Dictionary"_s, "match=\"/data/*\", id=\"dictionary \\\"one\\\"\""_s },
                    }, dictionary).serialize());
                    continue;
                }
                if (path == "/data/dcb"_s || path == "/data/dcz"_s || path == "/unadvertised"_s) {
                    bool advertised = received.availableDictionary == availableDictionary;
                    if (!advertised && !(path == "/unadvertised"_s && respondWithDCBEvenWithoutDictionary)) {
                        co_await connection.awaitableSend(textResponse(noDictionary).serialize());
                        continue;
                    }
                    bool isDCZ = path == "/data/dcz"_s;
                    RetainPtr body = isDCZ ? [NSData dataWithBytes:dczData length:sizeof(dczData)] : [NSData dataWithBytes:dcbData length:sizeof(dcbData)];
                    co_await connection.awaitableSend(HTTPResponse({
                        { "Content-Type"_s, "text/plain"_s },
                        { "Content-Encoding"_s, isDCZ ? "dcz"_s : "dcb"_s },
                        { "Cache-Control"_s, "no-store"_s },
                        { "Vary"_s, "available-dictionary, accept-encoding"_s },
                    }, body.get()).serialize());
                    continue;
                }
                if (path == "/data/redirect-to-data"_s) {
                    co_await connection.awaitableSend(redirectResponse("/data/dcb"_s).serialize());
                    continue;
                }
                if (path == "/data/redirect-to-other"_s) {
                    co_await connection.awaitableSend(redirectResponse("/other"_s).serialize());
                    continue;
                }
                if (path == "/data/redirect-cross-origin"_s) {
                    co_await connection.awaitableSend(redirectResponse(makeString("https://localhost:"_s, server->port(), "/data/dcb"_s)).serialize());
                    continue;
                }
                if (path == "/other"_s) {
                    co_await connection.awaitableSend(textResponse("other"_s).serialize());
                    continue;
                }
                co_await connection.awaitableSend(HTTPResponse(404).serialize());
            }
        }, HTTPServer::Protocol::Https);
    }
};

static RetainPtr<TestWKWebView> createWebViewWithCompressionDictionaries()
{
    __block bool removedData = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        removedData = true;
    }];
    Util::run(&removedData);

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CompressionDictionaryEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
}

static void loadMainPageAndRegisterDictionary(TestWKWebView *webView, TestNavigationDelegate *delegate, CompressionDictionaryServer& server)
{
    [delegate allowAnyTLSCertificate];
    webView.navigationDelegate = delegate;
    [webView loadRequest:server.server->request()];
    [delegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ("This is a test dictionary.\n", [webView objectByCallingAsyncFunction:@"return await fetchText('/dictionary')" withArguments:nil]);
    EXPECT_TRUE([[webView objectByCallingAsyncFunction:@"return await waitForDictionary()" withArguments:nil] boolValue]);
}

TEST(CompressionDictionary, DecodeDCBAndDCZ)
{
    CompressionDictionaryServer server;
    RetainPtr webView = createWebViewWithCompressionDictionaries();
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    loadMainPageAndRegisterDictionary(webView.get(), delegate.get(), server);

    for (NSString *path in @[ @"/data/dcb", @"/data/dcz" ]) {
        EXPECT_WK_STREQ(decodedData, [webView objectByCallingAsyncFunction:@"return await fetchText(path)" withArguments:@{ @"path" : path }]);

        auto received = server.requests.get(path);
        EXPECT_WK_STREQ(availableDictionary, received.availableDictionary);
        EXPECT_WK_STREQ("\"dictionary \\\"one\\\"\"", received.dictionaryID);
        EXPECT_TRUE(received.acceptEncoding.contains("dcb"_s));
        EXPECT_TRUE(received.acceptEncoding.contains("dcz"_s));
    }
}

TEST(CompressionDictionary, SameOriginRedirectToMatchingURL)
{
    CompressionDictionaryServer server;
    RetainPtr webView = createWebViewWithCompressionDictionaries();
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    loadMainPageAndRegisterDictionary(webView.get(), delegate.get(), server);

    server.requests.clear();
    EXPECT_WK_STREQ(decodedData, [webView objectByCallingAsyncFunction:@"return await fetchText('/data/redirect-to-data')" withArguments:nil]);
    EXPECT_WK_STREQ(availableDictionary, server.requests.get("/data/redirect-to-data"_s).availableDictionary);
    EXPECT_WK_STREQ(availableDictionary, server.requests.get("/data/dcb"_s).availableDictionary);
}

TEST(CompressionDictionary, SameOriginRedirectToNonMatchingURL)
{
    CompressionDictionaryServer server;
    RetainPtr webView = createWebViewWithCompressionDictionaries();
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    loadMainPageAndRegisterDictionary(webView.get(), delegate.get(), server);

    // CFNetwork keeps the headers on a same-origin redirect, but /other does not match the dictionary.
    server.requests.clear();
    EXPECT_WK_STREQ("other", [webView objectByCallingAsyncFunction:@"return await fetchText('/data/redirect-to-other')" withArguments:nil]);
    EXPECT_WK_STREQ(availableDictionary, server.requests.get("/data/redirect-to-other"_s).availableDictionary);

    auto other = server.requests.get("/other"_s);
    EXPECT_TRUE(other.availableDictionary.isNull());
    EXPECT_TRUE(other.dictionaryID.isNull());
}

TEST(CompressionDictionary, CrossOriginRedirect)
{
    CompressionDictionaryServer server;
    RetainPtr webView = createWebViewWithCompressionDictionaries();
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    loadMainPageAndRegisterDictionary(webView.get(), delegate.get(), server);

    // The dictionary was registered for https://127.0.0.1, so nothing matches after the redirect to https://localhost.
    server.requests.clear();
    [webView objectByCallingAsyncFunction:@"return await fetchText('/data/redirect-cross-origin', { mode: 'no-cors' })" withArguments:nil];
    EXPECT_WK_STREQ(availableDictionary, server.requests.get("/data/redirect-cross-origin"_s).availableDictionary);

    auto redirected = server.requests.get("/data/dcb"_s);
    EXPECT_TRUE(redirected.availableDictionary.isNull());
    EXPECT_TRUE(redirected.dictionaryID.isNull());
}

TEST(CompressionDictionary, DictionaryNotAdvertisedByWebKit)
{
    CompressionDictionaryServer server;
    server.respondWithDCBEvenWithoutDictionary = true;
    RetainPtr webView = createWebViewWithCompressionDictionaries();
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    loadMainPageAndRegisterDictionary(webView.get(), delegate.get(), server);

    // The page names the dictionary itself, for a URL that WebKit has no dictionary for. CFNetwork asks for it when the
    // dcb response arrives, and the load has to fail rather than hand the page the still-compressed body.
    NSString *script = [NSString stringWithFormat:@"return await fetchText('/unadvertised', { headers: { 'Available-Dictionary': '%s' } })", availableDictionary.characters()];
    EXPECT_WK_STREQ("error", [webView objectByCallingAsyncFunction:script withArguments:nil]);
    EXPECT_WK_STREQ(availableDictionary, server.requests.get("/unadvertised"_s).availableDictionary);
}

} // namespace TestWebKitAPI::CompressionDictionaryTests

#endif // HAVE(CFNETWORK_COMPRESSION_DICTIONARY)

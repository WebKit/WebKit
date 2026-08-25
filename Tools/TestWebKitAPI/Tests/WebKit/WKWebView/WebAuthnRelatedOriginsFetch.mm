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

#if ENABLE(WEB_AUTHN)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKContentWorld.h>
#import <WebKit/WKSecurityOrigin.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <memory>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

static constexpr auto callerOrigin = "https://caller.example.net"_s;
static constexpr auto relyingPartyOrigin = "https://example.com"_s;
static constexpr auto relyingPartyIdentifier = "example.com"_s;
static constexpr auto wellKnownPath = "/.well-known/webauthn"_s;
static constexpr auto redirectTargetPath = "/redirected"_s;

static NSString *probePage()
{
    return @"<!DOCTYPE html>"
        "<input autocomplete='username webauthn'>"
        "<script>"
        "window.probe = async function(rpId) {"
        "  try {"
        "    await navigator.credentials.get({"
        "      mediation: 'conditional',"
        "      publicKey: { challenge: new Uint8Array(32), rpId: rpId, allowCredentials: [] }"
        "    });"
        "    return 'resolved';"
        "  } catch (e) { return e.name; }"
        "};"
        "</script>";
}

enum class WellKnownBehavior : uint8_t {
    ValidList,
    NotFound,
    WrongContentType,
    RedirectToHTTP,
    NeverRespond,
    Oversized,
};

static HTTPResponse wellKnownResponse(WellKnownBehavior behavior)
{
    switch (behavior) {
    case WellKnownBehavior::ValidList:
        return { 200, { { "Content-Type"_s, "application/json"_s } }, makeString("{\"origins\":[\""_s, callerOrigin, "\"]}"_s) };
    case WellKnownBehavior::NotFound:
        return { 404, { { "Content-Type"_s, "application/json"_s } }, "{}"_s };
    case WellKnownBehavior::WrongContentType:
        return { 200, { { "Content-Type"_s, "text/html"_s } }, makeString("{\"origins\":[\""_s, callerOrigin, "\"]}"_s) };
    case WellKnownBehavior::RedirectToHTTP:
        return { 301, { { "Location"_s, makeString("http://"_s, relyingPartyIdentifier, redirectTargetPath) } }, { } };
    case WellKnownBehavior::Oversized: {
        StringBuilder builder;
        builder.append("{\"origins\":[\""_s, callerOrigin, "\""_s);
        while (builder.length() < 128 * 1024)
            builder.append(",\"https://padding.example.org\""_s);
        builder.append("]}"_s);
        return { 200, { { "Content-Type"_s, "application/json"_s } }, builder.toString() };
    }
    case WellKnownBehavior::NeverRespond:
        return HTTPResponse::Behavior::NeverSendResponse;
    }
}

static void loadAndWait(TestWKWebView *webView, TestNavigationDelegate *navigationDelegate, ASCIILiteral origin)
{
    RetainPtr url = adoptNS([[NSURL alloc] initWithString:makeString(origin, "/"_s).createNSString().get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:url.get()]];
    [navigationDelegate waitForDidFinishNavigation];
}

enum class RelyingPartyCertificate : bool { Untrusted, TrustedForSession };
enum class SetRelyingPartyCookie : bool { No, Yes };

struct ProbeResult {
    String outcome;
    size_t requestsDuringCeremony { 0 };
    String cookiesOnLastRequest;
    bool relyingPartyCookieWasSet { false };
};

static ProbeResult runProbe(WellKnownBehavior behavior, size_t maxTries, RelyingPartyCertificate certificate = RelyingPartyCertificate::TrustedForSession, SetRelyingPartyCookie setCookie = SetRelyingPartyCookie::No)
{
    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String { probePage() } } },
        { wellKnownPath, wellKnownResponse(behavior) },
        { redirectTargetPath, { 200, { { "Content-Type"_s, "application/json"_s } }, "{\"origins\":[]}"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    ProbeResult result;

    if (certificate == RelyingPartyCertificate::TrustedForSession) {
        loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);

        if (setCookie == SetRelyingPartyCookie::Yes) {
            [webView objectByEvaluatingJavaScript:@"document.cookie = 'session=secret'"];
            RetainPtr cookies = [webView stringByEvaluatingJavaScript:@"document.cookie"];
            result.relyingPartyCookieWasSet = [cookies containsString:@"session=secret"];
        }
    }

    loadAndWait(webView.get(), navigationDelegate.get(), callerOrigin);

    auto requestsBeforeCeremony = server.totalRequests();

    auto outcome = std::make_shared<RetainPtr<NSString>>();
    RetainPtr arguments = @{ @"rpId": relyingPartyIdentifier.createNSString().get() };
    [webView callAsyncJavaScript:@"return await window.probe(rpId);" arguments:arguments.get() inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id value, NSError *error) {
        *outcome = error ? [error description] : (NSString *)value;
    }];

    Util::waitFor([outcome] {
        return !!*outcome;
    }, maxTries);

    if (*outcome)
        result.outcome = String { outcome->get() };
    result.requestsDuringCeremony = server.totalRequests() - requestsBeforeCeremony;
    result.cookiesOnLastRequest = server.lastRequestCookies();
    return result;
}

TEST(WebAuthnRelatedOriginsFetch, ValidListIsAccepted)
{
    auto result = runProbe(WellKnownBehavior::ValidList, 50);
    EXPECT_FALSE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, NonSuccessStatusIsRejected)
{
    auto result = runProbe(WellKnownBehavior::NotFound, 100);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, NonJSONContentTypeIsRejected)
{
    auto result = runProbe(WellKnownBehavior::WrongContentType, 100);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, InsecureRedirectIsNotFollowed)
{
    auto result = runProbe(WellKnownBehavior::RedirectToHTTP, 100);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, UnresponsiveRelyingPartyTimesOut)
{
    auto result = runProbe(WellKnownBehavior::NeverRespond, 200);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, OversizedResourceIsRejected)
{
    auto result = runProbe(WellKnownBehavior::Oversized, 100);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
}

TEST(WebAuthnRelatedOriginsFetch, RequestCarriesNoCookies)
{
    auto result = runProbe(WellKnownBehavior::NotFound, 100, RelyingPartyCertificate::TrustedForSession, SetRelyingPartyCookie::Yes);
    EXPECT_TRUE(result.relyingPartyCookieWasSet);
    EXPECT_EQ(result.requestsDuringCeremony, 1u);
    EXPECT_TRUE(result.cookiesOnLastRequest.isEmpty());
}

TEST(WebAuthnRelatedOriginsFetch, UntrustedCertificateIsRejected)
{
    auto result = runProbe(WellKnownBehavior::ValidList, 100, RelyingPartyCertificate::Untrusted);
    EXPECT_TRUE(result.outcome == "SecurityError"_s);
    EXPECT_EQ(result.requestsDuringCeremony, 0u);
}

#pragma mark - Conditional Create + relatedOrigins delegate SPI

} // namespace TestWebKitAPI

static bool s_relatedOriginsDelegateCalled = false;
static RetainPtr<NSArray<WKSecurityOrigin *>> s_receivedOrigins;
static RetainPtr<NSString> s_receivedUsername;

static void resetConditionalCreateState()
{
    s_relatedOriginsDelegateCalled = false;
    s_receivedOrigins = nil;
    s_receivedUsername = nil;
}

@interface RelatedOriginsConsentDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic) BOOL shouldConsent;
@end

@implementation RelatedOriginsConsentDelegate

- (void)_webView:(WKWebView *)webView requestWebAuthenticationConditionalMediationRegistrationForUser:(NSString *)user relatedOrigins:(NSArray<WKSecurityOrigin *> *)relatedOrigins completionHandler:(void (^)(BOOL))completionHandler
{
    s_relatedOriginsDelegateCalled = true;
    s_receivedOrigins = relatedOrigins;
    s_receivedUsername = user;
    completionHandler(self.shouldConsent);
}

@end

@interface RelatedOriginsLegacyDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation RelatedOriginsLegacyDelegate

- (void)_webView:(WKWebView *)webView requestWebAuthenticationConditionalMediationRegistrationForUser:(NSString *)user completionHandler:(void (^)(BOOL))completionHandler
{
    s_relatedOriginsDelegateCalled = true;
    completionHandler(NO);
}

@end

static NSString *conditionalCreatePage()
{
    return @"<!DOCTYPE html>"
        "<script>"
        "window.conditionalCreate = async function(rpId) {"
        "  try {"
        "    let result = await navigator.credentials.create({"
        "      mediation: 'conditional',"
        "      publicKey: {"
        "        challenge: new Uint8Array(32),"
        "        rp: { id: rpId, name: 'Test' },"
        "        user: { id: new Uint8Array(16), name: 'testuser', displayName: 'Test User' },"
        "        pubKeyCredParams: [{ type: 'public-key', alg: -7 }]"
        "      }"
        "    });"
        "    window.result = 'resolved';"
        "  } catch (e) { window.result = e.name + ': ' + e.message; }"
        "};"
        "</script>";
}

namespace TestWebKitAPI {

static String runConditionalCreate(id<WKUIDelegatePrivate> uiDelegate, size_t maxTries = 100)
{
    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String { conditionalCreatePage() } } },
        { wellKnownPath, { 200, { { "Content-Type"_s, "application/json"_s } }, makeString("{\"origins\":[\""_s, callerOrigin, "\"]}"_s) } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setUIDelegate:uiDelegate];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);
    loadAndWait(webView.get(), navigationDelegate.get(), callerOrigin);

    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"window.conditionalCreate('%@'); undefined", relyingPartyIdentifier.createNSString().get()]];

    auto outcome = std::make_shared<RetainPtr<NSString>>();
    Util::waitFor([webView, outcome] {
        *outcome = [webView stringByEvaluatingJavaScript:@"window.result"];
        return [*outcome length] > 0;
    }, maxTries);

    return *outcome ? String { outcome->get() } : "<no result>"_str;
}

TEST(WebAuthnRelatedOriginsFetch, RelatedOriginsPassedToDelegate)
{
    resetConditionalCreateState();
    RetainPtr delegate = adoptNS([RelatedOriginsConsentDelegate new]);
    [delegate setShouldConsent:YES];

    auto outcome = runConditionalCreate(delegate.get(), 200);

    EXPECT_TRUE(s_relatedOriginsDelegateCalled);
    EXPECT_TRUE([s_receivedUsername isEqualToString:@"testuser"]);
}

TEST(WebAuthnRelatedOriginsFetch, DelegateRejectsStopsCeremony)
{
    resetConditionalCreateState();
    RetainPtr delegate = adoptNS([RelatedOriginsConsentDelegate new]);
    [delegate setShouldConsent:NO];

    runConditionalCreate(delegate.get());

    EXPECT_TRUE(s_relatedOriginsDelegateCalled);
}

TEST(WebAuthnRelatedOriginsFetch, FallbackToOldDelegateWithoutRelatedOrigins)
{
    resetConditionalCreateState();
    RetainPtr delegate = adoptNS([RelatedOriginsLegacyDelegate new]);

    runConditionalCreate(delegate.get());

    EXPECT_TRUE(s_relatedOriginsDelegateCalled);
}

TEST(WebAuthnRelatedOriginsFetch, SameDomainConditionalCreatePassesOriginsToDelegate)
{
    resetConditionalCreateState();
    RetainPtr delegate = adoptNS([RelatedOriginsConsentDelegate new]);
    [delegate setShouldConsent:YES];

    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String { conditionalCreatePage() } } },
        { wellKnownPath, { 200, { { "Content-Type"_s, "application/json"_s } }, "{\"origins\":[\"https://login.live.com\",\"https://caller.example.net\"]}"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setUIDelegate:delegate.get()];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);

    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"window.conditionalCreate('%@'); undefined", relyingPartyIdentifier.createNSString().get()]];

    Util::waitFor([] {
        return s_relatedOriginsDelegateCalled;
    }, 200);

    EXPECT_TRUE(s_relatedOriginsDelegateCalled);
    EXPECT_TRUE([s_receivedUsername isEqualToString:@"testuser"]);
}

TEST(WebAuthnRelatedOriginsFetch, ModalGetWithCrossDomainRPID)
{
    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, @"<!DOCTYPE html><script>window.modalGet = async function(rpId) { try { await navigator.credentials.get({ publicKey: { challenge: new Uint8Array(32), rpId: rpId, allowCredentials: [] } }); window.result = 'resolved'; } catch (e) { window.result = e.name; } };</script>" } },
        { wellKnownPath, { 200, { { "Content-Type"_s, "application/json"_s } }, makeString("{\"origins\":[\""_s, callerOrigin, "\"]}"_s) } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);
    loadAndWait(webView.get(), navigationDelegate.get(), callerOrigin);

    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"window.modalGet('%@'); undefined", relyingPartyIdentifier.createNSString().get()]];

    auto outcome = std::make_shared<RetainPtr<NSString>>();
    Util::waitFor([webView, outcome] {
        *outcome = [webView stringByEvaluatingJavaScript:@"window.result"];
        return [*outcome length] > 0;
    }, 200);

    EXPECT_TRUE(*outcome);
    EXPECT_FALSE([*outcome isEqualToString:@"SecurityError"]);
}

TEST(WebAuthnRelatedOriginsFetch, ModalCreateWithCrossDomainRPID)
{
    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, @"<!DOCTYPE html><script>window.modalCreate = async function(rpId) { try { await navigator.credentials.create({ publicKey: { challenge: new Uint8Array(32), rp: { id: rpId, name: 'Test' }, user: { id: new Uint8Array(16), name: 'user', displayName: 'User' }, pubKeyCredParams: [{ type: 'public-key', alg: -7 }] } }); window.result = 'resolved'; } catch (e) { window.result = e.name; } };</script>" } },
        { wellKnownPath, { 200, { { "Content-Type"_s, "application/json"_s } }, makeString("{\"origins\":[\""_s, callerOrigin, "\"]}"_s) } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);
    loadAndWait(webView.get(), navigationDelegate.get(), callerOrigin);

    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"window.modalCreate('%@'); undefined", relyingPartyIdentifier.createNSString().get()]];

    auto outcome = std::make_shared<RetainPtr<NSString>>();
    Util::waitFor([webView, outcome] {
        *outcome = [webView stringByEvaluatingJavaScript:@"window.result"];
        return [*outcome length] > 0;
    }, 200);

    EXPECT_TRUE(*outcome);
    EXPECT_FALSE([*outcome isEqualToString:@"SecurityError"]);
}

TEST(WebAuthnRelatedOriginsFetch, UnauthorizedCallerConditionalCreateIsRejected)
{
    resetConditionalCreateState();
    RetainPtr delegate = adoptNS([RelatedOriginsConsentDelegate new]);
    [delegate setShouldConsent:YES];

    HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, String { conditionalCreatePage() } } },
        { wellKnownPath, { 200, { { "Content-Type"_s, "application/json"_s } }, "{\"origins\":[\"https://other.example.org\"]}"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:server.httpsProxyConfiguration()]);
    [webView setUIDelegate:delegate.get()];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView focus];

    loadAndWait(webView.get(), navigationDelegate.get(), relyingPartyOrigin);
    loadAndWait(webView.get(), navigationDelegate.get(), callerOrigin);

    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"window.conditionalCreate('%@'); undefined", relyingPartyIdentifier.createNSString().get()]];

    auto outcome = std::make_shared<RetainPtr<NSString>>();
    Util::waitFor([webView, outcome] {
        *outcome = [webView stringByEvaluatingJavaScript:@"window.result"];
        return [*outcome length] > 0;
    }, 200);

    EXPECT_TRUE(s_relatedOriginsDelegateCalled);
    EXPECT_EQ([s_receivedOrigins count], 0u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)

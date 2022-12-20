/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <LocalAuthentication/LocalAuthentication.h>
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/ExceptionCode.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKAuthenticationExtensionsClientInputs.h>
#import <WebKit/_WKAuthenticationExtensionsClientOutputs.h>
#import <WebKit/_WKAuthenticatorAssertionResponse.h>
#import <WebKit/_WKAuthenticatorAttestationResponse.h>
#import <WebKit/_WKAuthenticatorSelectionCriteria.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKPublicKeyCredentialCreationOptions.h>
#import <WebKit/_WKPublicKeyCredentialDescriptor.h>
#import <WebKit/_WKPublicKeyCredentialParameters.h>
#import <WebKit/_WKPublicKeyCredentialRequestOptions.h>
#import <WebKit/_WKPublicKeyCredentialRelyingPartyEntity.h>
#import <WebKit/_WKPublicKeyCredentialUserEntity.h>
#import <WebKit/_WKWebAuthenticationAssertionResponse.h>
#import <WebKit/_WKWebAuthenticationPanel.h>
#import <WebKit/_WKWebAuthenticationPanelForTesting.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakRandomNumber.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/StringConcatenateNumbers.h>

static bool webAuthenticationPanelRan = false;
static bool webAuthenticationPanelFailed = false;
static bool webAuthenticationPanelSucceded = false;
static bool webAuthenticationPanelUpdateMultipleNFCTagsPresent = false;
static bool webAuthenticationPanelUpdateNoCredentialsFound = false;
static bool webAuthenticationPanelUpdatePINBlocked = false;
static bool webAuthenticationPanelUpdatePINAuthBlocked = false;
static bool webAuthenticationPanelUpdatePINInvalid = false;
static bool webAuthenticationPanelUpdateLAError = false;
static bool webAuthenticationPanelUpdateLAExcludeCredentialsMatched = false;
static bool webAuthenticationPanelUpdateLANoCredential = false;
static bool webAuthenticationPanelCancelImmediately = false;
static bool webAuthenticationPanelRequestNoGesture = true;
static _WKLocalAuthenticatorPolicy localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyDisallow;
static String webAuthenticationPanelPin;
static BOOL webAuthenticationPanelNullUserHandle = NO;
static String testES256PrivateKeyBase64 =
    "BDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749VBJPgqUIwfhWHJ91nb7U"
    "PH76c0+WFOzZKslPyyFse4goGIW2R7k9VHLPEZl5nfnBgEVFh5zev+/xpHQIvuq6"
    "RQ=="_s;
static String testES256PrivateKeyBase64Alternate = "BBRoi2JbR0IXTeJmvXUp1YIuM4sph/Lu3eGf75F7n+HojHKG70a4R0rB2PQce5/SJle6T7OO5Cqet/LJZVM6NQ8yDDxWvayf71GTDp2yUtuIbqJLFVbpWymlj9WRizgX3A=="_s;
static String testUserEntityBundleBase64 = "omJpZEoAAQIDBAUGBwgJZG5hbWVkSm9obg=="_s; // { "id": h'00010203040506070809', "name": "John" }
static String testUserEntityBundleNoUserHandleBase64 = "oWRuYW1lbE1DIE5vLUhhbmRsZQ=="_s; // {"name": "MC No-Handle"}
static String webAuthenticationPanelSelectedCredentialName;
static String webAuthenticationPanelSelectedCredentialDisplayName;
static String testWebKitAPIAccessGroup = "com.apple.TestWebKitAPI"_s;
static String testWebKitAPIAlternateAccessGroup = "com.apple.TestWebKitAPIAlternate"_s;
static bool laContextRequested = false;

@interface TestWebAuthenticationPanelDelegate : NSObject <_WKWebAuthenticationPanelDelegate>
@end

@implementation TestWebAuthenticationPanelDelegate

- (void)panel:(_WKWebAuthenticationPanel *)panel updateWebAuthenticationPanel:(_WKWebAuthenticationPanelUpdate)update
{
    ASSERT_NE(panel, nil);
    if (webAuthenticationPanelCancelImmediately)
        [panel cancel];

    if (update == _WKWebAuthenticationPanelUpdateMultipleNFCTagsPresent) {
        webAuthenticationPanelUpdateMultipleNFCTagsPresent = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdateNoCredentialsFound) {
        webAuthenticationPanelUpdateNoCredentialsFound = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdatePINBlocked) {
        webAuthenticationPanelUpdatePINBlocked = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdatePINAuthBlocked) {
        webAuthenticationPanelUpdatePINAuthBlocked = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdatePINInvalid) {
        webAuthenticationPanelUpdatePINInvalid = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdateLAError) {
        webAuthenticationPanelUpdateLAError = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdateLAExcludeCredentialsMatched) {
        webAuthenticationPanelUpdateLAExcludeCredentialsMatched = true;
        return;
    }
    if (update == _WKWebAuthenticationPanelUpdateLANoCredential) {
        webAuthenticationPanelUpdateLANoCredential = true;
        return;
    }
}

- (void)panel:(_WKWebAuthenticationPanel *)panel dismissWebAuthenticationPanelWithResult:(_WKWebAuthenticationResult)result
{
    ASSERT_NE(panel, nil);
    if (webAuthenticationPanelCancelImmediately)
        [panel cancel];

    if (result == _WKWebAuthenticationResultFailed) {
        webAuthenticationPanelFailed = true;
        return;
    }
    if (result == _WKWebAuthenticationResultSucceeded) {
        webAuthenticationPanelSucceded = true;
        return;
    }
}

- (void)panel:(_WKWebAuthenticationPanel *)panel requestPINWithRemainingRetries:(NSUInteger)retries completionHandler:(void (^)(NSString *))completionHandler
{
    ASSERT_NE(panel, nil);
    EXPECT_EQ(retries, 8ul);
    completionHandler(webAuthenticationPanelPin);
}

- (void)panel:(_WKWebAuthenticationPanel *)panel selectAssertionResponse:(NSArray < _WKWebAuthenticationAssertionResponse *> *)responses source:(_WKWebAuthenticationSource)source completionHandler:(void (^)(_WKWebAuthenticationAssertionResponse *))completionHandler
{
    if (responses.count == 1) {
        auto laContext = adoptNS([[LAContext alloc] init]);
        [responses.firstObject setLAContext:laContext.get()];

        completionHandler(responses.firstObject);
        return;
    }

    // Responses returned from LocalAuthenticator is in the order of LRU. Therefore, we use the last item to populate it to
    // the first to test its correctness.
    if (source == _WKWebAuthenticationSourceLocal) {
        auto *object = responses.lastObject;

        auto laContext = adoptNS([[LAContext alloc] init]);
        [object setLAContext:laContext.get()];

        webAuthenticationPanelSelectedCredentialName = object.name;
        webAuthenticationPanelSelectedCredentialDisplayName = object.displayName;
        completionHandler(object);
        return;
    }

    EXPECT_EQ(source, _WKWebAuthenticationSourceExternal);
    EXPECT_EQ(responses.count, 2ul);
    for (_WKWebAuthenticationAssertionResponse *response in responses) {
        EXPECT_TRUE([[response.credentialID base64EncodedStringWithOptions:0] isEqual:@"KAitzuj+Tslzelf3/vZwIGtDQNgoKeFd5oEieYzhyzA65saf0tK2w/mooa7tQtGgDdwZIjOhjcuZ0pQ1ajoE4A=="] || !response.credentialID);
        EXPECT_TRUE([response.name isEqual:@"johnpsmith@example.com"] || [response.name isEqual:@""]);
        EXPECT_TRUE([response.displayName isEqual:@"John P. Smith"] || [response.displayName isEqual:@""]);
        EXPECT_TRUE([[response.userHandle base64EncodedStringWithOptions:0] isEqual:@"MIIBkzCCATigAwIBAjCCAZMwggE4oAMCAQIwggGTMII="] || !response.userHandle);
    }

    auto index = weakRandomNumber<uint32_t>() % 2;
    webAuthenticationPanelNullUserHandle = responses[index].userHandle ? NO : YES;
    completionHandler(responses[index]);
}

- (void)panel:(_WKWebAuthenticationPanel *)panel decidePolicyForLocalAuthenticatorWithCompletionHandler:(void (^)(_WKLocalAuthenticatorPolicy policy))completionHandler
{
    completionHandler(localAuthenticatorPolicy);
}

- (void)panel:(_WKWebAuthenticationPanel *)panel requestLAContextForUserVerificationWithCompletionHandler:(void (^)(LAContext *context))completionHandler
{
    laContextRequested = true;
    completionHandler(nil);
}

@end

@interface TestWebAuthenticationPanelFakeDelegate : NSObject <_WKWebAuthenticationPanelDelegate>
@end

@implementation TestWebAuthenticationPanelFakeDelegate
@end

@interface TestWebAuthenticationPanelUIDelegate : NSObject <WKUIDelegatePrivate>
@property bool isRacy;
@property bool isFake;
@property bool isNull;

- (instancetype)init;
@end

@implementation TestWebAuthenticationPanelUIDelegate {
    RetainPtr<NSObject<_WKWebAuthenticationPanelDelegate>> _delegate;
    BlockPtr<void(_WKWebAuthenticationPanelResult)> _callback;
    RetainPtr<WKFrameInfo> _frameInfo;
    RetainPtr<_WKWebAuthenticationPanel> _panel;
}

- (instancetype)init
{
    if (self = [super init]) {
        self.isRacy = false;
        self.isFake = false;
        self.isNull = false;
    }
    return self;
}

- (void)_webView:(WKWebView *)webView requestWebAuthenticationNoGestureForOrigin:(WKSecurityOrigin *)orgin completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(webAuthenticationPanelRequestNoGesture);
}

- (void)_webView:(WKWebView *)webView runWebAuthenticationPanel:(_WKWebAuthenticationPanel *)panel initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(_WKWebAuthenticationPanelResult))completionHandler
{
    webAuthenticationPanelRan = true;
    _frameInfo = frame;

    if (!_isNull) {
        if (!_isFake)
            _delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
        else
            _delegate = adoptNS([[TestWebAuthenticationPanelFakeDelegate alloc] init]);
    }
    ASSERT_NE(panel, nil);
    _panel = panel;
    [_panel setDelegate:_delegate.get()];

    if (_isRacy) {
        if (!_callback) {
            _callback = makeBlockPtr(completionHandler);
            return;
        }
        _callback(_WKWebAuthenticationPanelResultUnavailable);
    }
    completionHandler(_WKWebAuthenticationPanelResultPresented);
}

- (WKFrameInfo *)frame
{
    return _frameInfo.get();
}

- (_WKWebAuthenticationPanel *)panel
{
    return _panel.get();
}

@end

namespace TestWebKitAPI {
using namespace WebCore;

namespace {

static constexpr auto parentFrame = "<html><iframe id='theFrame' src='iFrame.html'></iframe></html>"_s;
static constexpr auto subFrame =
"<html>"
"<input type='text' id='input'>"
"<script>"
"    if (window.internals) {"
"        internals.setMockWebAuthenticationConfiguration({ hid: { expectCancel: true } });"
"        internals.withUserGesture(() => { input.focus(); });"
"    }"
"    const options = {"
"        publicKey: {"
"            challenge: new Uint8Array(16)"
"        }"
"    };"
"    navigator.credentials.get(options);"
"</script>"
"</html>"_s;

static void reset()
{
    webAuthenticationPanelRan = false;
    webAuthenticationPanelFailed = false;
    webAuthenticationPanelSucceded = false;
    webAuthenticationPanelUpdateMultipleNFCTagsPresent = false;
    webAuthenticationPanelUpdateNoCredentialsFound = false;
    webAuthenticationPanelUpdatePINBlocked = false;
    webAuthenticationPanelUpdatePINAuthBlocked = false;
    webAuthenticationPanelUpdatePINInvalid = false;
    webAuthenticationPanelUpdateLAError = false;
    webAuthenticationPanelUpdateLAExcludeCredentialsMatched = false;
    webAuthenticationPanelUpdateLANoCredential = false;
    webAuthenticationPanelCancelImmediately = false;
    webAuthenticationPanelRequestNoGesture = true;
    webAuthenticationPanelPin = emptyString();
    webAuthenticationPanelNullUserHandle = NO;
    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyDisallow;
    webAuthenticationPanelSelectedCredentialName = emptyString();
    laContextRequested = false;
}

static void checkPanel(_WKWebAuthenticationPanel *panel, NSString *relyingPartyID, NSArray *transports, _WKWebAuthenticationType type)
{
    EXPECT_WK_STREQ(panel.relyingPartyID, relyingPartyID);

    EXPECT_EQ(panel.transports.count, transports.count);
    size_t count = 0;
    for (NSNumber *transport : transports) {
        if ([panel.transports containsObject:transport])
            count++;
    }
    EXPECT_EQ(count, transports.count);

    EXPECT_EQ(panel.type, type);
}

static void checkFrameInfo(WKFrameInfo *frame, bool isMainFrame, NSString *url, NSString *protocol, NSString *host, int port, WKWebView *webView)
{
    EXPECT_EQ(frame.mainFrame, isMainFrame);
    EXPECT_TRUE([frame.request.URL.absoluteString isEqual:url]);
    EXPECT_WK_STREQ(frame.securityOrigin.protocol, protocol);
    EXPECT_WK_STREQ(frame.securityOrigin.host, host);
    EXPECT_EQ(frame.securityOrigin.port, port);
    EXPECT_EQ(frame.webView, webView);
}

#if USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

bool addKeyToKeychain(const String& privateKeyBase64, const String& rpId, const String& userHandleBase64, bool synchronizable = false)
{
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256,
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:privateKeyBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (__bridge CFDictionaryRef)options,
        &errorRef
    ));
    if (errorRef)
        return false;

    auto addQuery = adoptNS([[NSMutableDictionary alloc] init]);
    [addQuery setDictionary:@{
        (id)kSecValueRef: (id)key.get(),
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: rpId,
        (id)kSecAttrApplicationTag: adoptNS([[NSData alloc] initWithBase64EncodedString:userHandleBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: testWebKitAPIAccessGroup,
    }];
    if (synchronizable)
        [addQuery.get() setObject:@YES forKey:(__bridge id)kSecAttrSynchronizable];

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery.get(), NULL);
    if (status)
        return false;

    return true;
}

void cleanUpKeychain(const String& rpId)
{
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: rpId,
        (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain: @YES
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
}

#endif // USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

} // namesapce;

#if HAVE(NEAR_FIELD)
TEST(WebAuthenticationPanel, NoPanelNfcSucceed)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}
#endif

TEST(WebAuthenticationPanel, NoPanelHidSuccess)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PanelHidSuccess1)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);

    // A bit of extra checks.
    checkFrameInfo([delegate frame], true, [testURL absoluteString], @"file", @"", 0, webView.get());
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get()], _WKWebAuthenticationTypeGet);
}

TEST(WebAuthenticationPanel, PanelHidSuccess2)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);

    // A bit of extra checks.
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get()], _WKWebAuthenticationTypeCreate);
    EXPECT_WK_STREQ([delegate panel].userName, "John Appleseed");
}

#if HAVE(NEAR_FIELD)
// This test aims to see if the callback for the first ceremony could affect the second one.
// Therefore, the first callback will be held to return at the time when the second arrives.
// The first callback will return _WKWebAuthenticationPanelResultUnavailable which leads to timeout for NFC.
// The second callback will return _WKWebAuthenticationPanelResultPresented which leads to success.
TEST(WebAuthenticationPanel, PanelRacy1)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];

    // A bit of extra checks.
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportNFC]).get()], _WKWebAuthenticationTypeGet);
}

// Unlike the previous one, this one focuses on the order of the delegate callbacks.
TEST(WebAuthenticationPanel, PanelRacy2)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    webAuthenticationPanelRan = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}
#endif // HAVE(NEAR_FIELD)

TEST(WebAuthenticationPanel, PanelTwice)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);

    reset();
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}

TEST(WebAuthenticationPanel, ReloadHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView reload];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, LocationChangeHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> otherURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript: [NSString stringWithFormat:@"location = '%@'", [otherURL absoluteString]] completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, NewLoadHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> otherURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView loadRequest:[NSURLRequest requestWithURL:otherURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, CloseHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView _close];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, SubFrameChangeLocationHidCancel)
{
    HTTPServer server([parentFrame = String(parentFrame), subFrame = String(subFrame)] (const Connection& connection) {
        RetainPtr<NSString> firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentFrame.length(),
            (id)parentFrame
        ];
        RetainPtr<NSString> secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            subFrame.length(),
            (id)subFrame
        ];
        connection.receiveHTTPRequest([=] (Vector<char>&&) {
            connection.send(firstResponse.get(), [=] {
                connection.receiveHTTPRequest([=] (Vector<char>&&) {
                    connection.send(secondResponse.get());
                });
            });
        });
    });

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    auto port = static_cast<unsigned>(server.port());
    auto url = makeString("http://localhost:", port);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)url]]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript:@"theFrame.src = 'simple.html'" completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);

    // A bit of extra checks.
    checkFrameInfo([delegate frame], false, (id)makeString(url, "/iFrame.html"), @"http", @"localhost", port, webView.get());
    checkPanel([delegate panel], @"localhost", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get()], _WKWebAuthenticationTypeGet);
}

TEST(WebAuthenticationPanel, SubFrameDestructionHidCancel)
{
    HTTPServer server([parentFrame = String(parentFrame), subFrame = String(subFrame)] (const Connection& connection) {
        RetainPtr<NSString> firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentFrame.length(),
            (id)parentFrame
        ];
        RetainPtr<NSString> secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            subFrame.length(),
            (id)subFrame
        ];

        connection.receiveHTTPRequest([=] (Vector<char>&&) {
            connection.send(firstResponse.get(), [=] {
                connection.receiveHTTPRequest([=] (Vector<char>&&) {
                    connection.send(secondResponse.get());
                });
            });
        });
    });

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)makeString("http://localhost:", server.port())]]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript:@"theFrame.parentNode.removeChild(theFrame)" completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    __block bool done = false;
    [webView performAfterReceivingMessage:@"This request has been cancelled by the user." action:^{
        done = true;
    }];
    [[delegate panel] cancel];
    Util::run(&done);
    EXPECT_TRUE(webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidCtapNoCredentialsFound)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelUpdateNoCredentialsFound);
}

TEST(WebAuthenticationPanel, PanelU2fCtapNoCredentialsFound)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-u2f-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelUpdateNoCredentialsFound);
}

TEST(WebAuthenticationPanel, FakePanelHidSuccess)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsFake:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, FakePanelHidCtapNoCredentialsFound)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsFake:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView waitForMessage:@"Operation timed out."];
}

TEST(WebAuthenticationPanel, NullPanelHidSuccess)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsNull:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, NullPanelHidCtapNoCredentialsFound)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsNull:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView waitForMessage:@"Operation timed out."];
}

#if HAVE(NEAR_FIELD)
TEST(WebAuthenticationPanel, PanelMultipleNFCTagsPresent)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc-multiple-tags" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelUpdateMultipleNFCTagsPresent);
}
#endif

TEST(WebAuthenticationPanel, PanelHidCancelReloadNoCrash)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [[delegate panel] cancel];
    [webView reload];
    [webView waitForMessage:@"Operation timed out."];
}

TEST(WebAuthenticationPanel, PanelHidSuccessCancelNoCrash)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];
    webAuthenticationPanelCancelImmediately = true;

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PanelHidCtapNoCredentialsFoundCancelNoCrash)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];
    webAuthenticationPanelCancelImmediately = true;

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateNoCredentialsFound);
}

TEST(WebAuthenticationPanel, PinGetRetriesError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-retries-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView focus];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 2"];
}

TEST(WebAuthenticationPanel, PinGetKeyAgreementError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-key-agreement-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView focus];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 2"];
}

TEST(WebAuthenticationPanel, PinRequestPinErrorNoDelegate)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    [webView focus];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Pin is null."];
}

TEST(WebAuthenticationPanel, PinRequestPinErrorNullDelegate)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsNull:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Pin is null."];
}

TEST(WebAuthenticationPanel, PinRequestPinError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-fake-pin-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "123"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdatePINInvalid);
    webAuthenticationPanelPin = "1234"_s;
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 50"];
    EXPECT_FALSE(webAuthenticationPanelUpdatePINInvalid);
    EXPECT_TRUE(webAuthenticationPanelUpdatePINBlocked);
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinAuthBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-auth-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 52"];
    EXPECT_FALSE(webAuthenticationPanelUpdatePINInvalid);
    EXPECT_TRUE(webAuthenticationPanelUpdatePINAuthBlocked);
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinInvalidErrorAndRetry)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinAuthInvalidErrorAndRetry)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-auth-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, MakeCredentialInternalUV)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-internal-uv" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, MakeCredentialPin)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, MakeCredentialPinAuthBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-auth-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 52"];
    EXPECT_FALSE(webAuthenticationPanelUpdatePINInvalid);
    EXPECT_TRUE(webAuthenticationPanelUpdatePINAuthBlocked);
}

TEST(WebAuthenticationPanel, MakeCredentialPinInvalidErrorAndRetry)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, GetAssertionPin)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-pin" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, GetAssertionInternalUV)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-internal-uv" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, GetAssertionInternalUVPinFallback)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-internal-uv-pin-fallback" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, GetAssertionPinAuthBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-pin-auth-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Unknown internal error. Error code: 52"];
    EXPECT_FALSE(webAuthenticationPanelUpdatePINInvalid);
    EXPECT_TRUE(webAuthenticationPanelUpdatePINAuthBlocked);
}

TEST(WebAuthenticationPanel, GetAssertionPinInvalidErrorAndRetry)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-pin-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

#if HAVE(NEAR_FIELD)
TEST(WebAuthenticationPanel, NfcPinCachedDisconnect)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc-pin-disconnect" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234"_s;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}
#endif // HAVE(NEAR_FIELD)

TEST(WebAuthenticationPanel, MultipleAccountsNullDelegate)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-multiple-accounts" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsNull:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Operation timed out."];
}

TEST(WebAuthenticationPanel, MultipleAccounts)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-multiple-accounts" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_EQ([[webView stringByEvaluatingJavaScript:@"userHandle"] isEqualToString:@"<null>"], webAuthenticationPanelNullUserHandle);
}

// For macOS, only internal builds can sign keychain entitlemnets
// which are required to run local authenticator tests.
#if USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

TEST(WebAuthenticationPanel, LAError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateLAError);
}

TEST(WebAuthenticationPanel, LADuplicateCredential)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-duplicate-credential" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
    cleanUpKeychain(emptyString());
}

TEST(WebAuthenticationPanel, LADuplicateCredentialWithConsent)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-duplicate-credential" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));

    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyAllow;

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateLAExcludeCredentialsMatched);
    cleanUpKeychain(emptyString());
}

TEST(WebAuthenticationPanel, LANoCredential)
{
    reset();
    // In case this wasn't cleaned up by another test.
    cleanUpKeychain(emptyString());

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateLANoCredential);
}

TEST(WebAuthenticationPanel, LAMakeCredentialAllowLocalAuthenticator)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyAllow;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get(), adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportInternal]).get()], _WKWebAuthenticationTypeCreate);
    cleanUpKeychain(emptyString());
}

TEST(WebAuthenticationPanel, LAMakeCredentialNoMockNoUserGesture)
{
    reset();
    webAuthenticationPanelRequestNoGesture = false;
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-no-mock" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"This request has been cancelled by the user."];
}

#if PLATFORM(MAC)

TEST(WebAuthenticationPanel, LAGetAssertion)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get(), adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportInternal]).get()], _WKWebAuthenticationTypeGet);
    cleanUpKeychain(emptyString());
}

TEST(WebAuthenticationPanel, LAGetAssertionMultipleCredentialStore)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));
    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64Alternate, emptyString(), "o2JpZEoAAQIDBAUGBwgJZG5hbWVkSmFuZWtkaXNwbGF5TmFtZWpKYW5lIFNtaXRo"_s/* { "id": h'00010203040506070809', "name": "Jane", "displayName": "Jane Smith" } */, true /* synchronizable */));

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "John");

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "Jane");
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialDisplayName, "Jane Smith");

    cleanUpKeychain(emptyString());
}

TEST(WebAuthenticationPanel, LAGetAssertionNoMockNoUserGesture)
{
    reset();
    webAuthenticationPanelRequestNoGesture = false;
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la-no-mock" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
#if HAVE(UNIFIED_ASC_AUTH_UI)
    [webView waitForMessage:@"Operation failed."];
#else
    [webView waitForMessage:@"This request has been cancelled by the user."];
#endif
}

TEST(WebAuthenticationPanel, LAGetAssertionMultipleOrder)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));
    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64Alternate, emptyString(), "omJpZEoAAQIDBAUGBwgJZG5hbWVkSmFuZQ=="_s/* { "id": h'00010203040506070809', "name": "Jane" } */));

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "John");

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "Jane");

    cleanUpKeychain(emptyString());
}

#endif // PLATFORM(MAC)

#endif // USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

TEST(WebAuthenticationPanel, PublicKeyCredentialCreationOptionsMinimum)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];

    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    auto result = [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options.get()];

    EXPECT_WK_STREQ(result.rp.name, "example.com");
    EXPECT_TRUE(result.rp.icon.isNull());
    EXPECT_TRUE(result.rp.id && result.rp.id->isNull());

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_TRUE(result.user.icon.isNull());
    EXPECT_EQ(result.user.id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 1lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);

    EXPECT_EQ(result.timeout, std::nullopt);
    EXPECT_EQ(result.excludeCredentials.size(), 0lu);
    EXPECT_EQ(result.authenticatorSelection, std::nullopt);
    EXPECT_EQ(result.attestation, AttestationConveyancePreference::None);
    EXPECT_TRUE(result.extensions->appid.isNull());
    EXPECT_EQ(result.extensions->googleLegacyAppidSupport, false);
}

TEST(WebAuthenticationPanel, PublicKeyCredentialCreationOptionsMaximumDefault)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto parameters1 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);
    auto parameters2 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-257]);
    auto descriptor = adoptNS([[_WKPublicKeyCredentialDescriptor alloc] initWithIdentifier:nsIdentifier]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters1.get(), parameters2.get() ];
    auto authenticatorSelection = adoptNS([[_WKAuthenticatorSelectionCriteria alloc] init]);
    auto extensions = adoptNS([[_WKAuthenticationExtensionsClientInputs alloc] init]);

    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    [options setTimeout:@120];
    [options setExcludeCredentials:@[ descriptor.get() ]];
    [options setAuthenticatorSelection:authenticatorSelection.get()];
    [options setExtensions:extensions.get()];

    auto result = [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options.get()];

    EXPECT_WK_STREQ(result.rp.name, "example.com");
    EXPECT_TRUE(result.rp.icon.isNull());
    EXPECT_TRUE(result.rp.id && result.rp.id->isNull());

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_TRUE(result.user.icon.isNull());
    EXPECT_EQ(result.user.id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 1lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].id.data(), identifier, sizeof(identifier)), 0);

    EXPECT_EQ(result.authenticatorSelection->authenticatorAttachment, std::nullopt);
    EXPECT_EQ(result.authenticatorSelection->requireResidentKey, false);
    EXPECT_EQ(result.authenticatorSelection->userVerification, UserVerificationRequirement::Preferred);

    EXPECT_EQ(result.attestation, AttestationConveyancePreference::None);
    EXPECT_TRUE(result.extensions->appid.isNull());
}

TEST(WebAuthenticationPanel, PublicKeyCredentialCreationOptionsMaximum1)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto parameters1 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);
    auto parameters2 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-257]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    [rp setIcon:@"https//www.example.com/icon.jpg"];
    [rp setIdentifier:@"example.com"];

    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    [user setIcon:@"https//www.example.com/icon.jpg"];

    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters1.get(), parameters2.get() ];

    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    [options setTimeout:@120];

    auto usb = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportUSB]);
    auto nfc = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportNFC]);
    auto internal = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportInternal]);
    auto credential = adoptNS([[_WKPublicKeyCredentialDescriptor alloc] initWithIdentifier:nsIdentifier]);
    [credential setTransports:@[ usb.get(), nfc.get(), internal.get() ]];
    [options setExcludeCredentials:@[ credential.get(), credential.get() ]];

    auto authenticatorSelection = adoptNS([[_WKAuthenticatorSelectionCriteria alloc] init]);
    [authenticatorSelection setAuthenticatorAttachment:_WKAuthenticatorAttachmentPlatform];
    [authenticatorSelection setRequireResidentKey:YES];
    [authenticatorSelection setUserVerification:_WKUserVerificationRequirementRequired];
    [options setAuthenticatorSelection:authenticatorSelection.get()];

    [options setAttestation:_WKAttestationConveyancePreferenceDirect];

    auto result = [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options.get()];

    EXPECT_WK_STREQ(result.rp.name, "example.com");
    EXPECT_WK_STREQ(result.rp.icon, @"https//www.example.com/icon.jpg");
    EXPECT_WK_STREQ(*result.rp.id, "example.com");

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_WK_STREQ(result.user.icon, @"https//www.example.com/icon.jpg");
    EXPECT_EQ(result.user.id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 2lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_EQ(result.excludeCredentials[0].transports.size(), 3lu);
    EXPECT_EQ(result.excludeCredentials[0].transports[0], AuthenticatorTransport::Usb);
    EXPECT_EQ(result.excludeCredentials[0].transports[1], AuthenticatorTransport::Nfc);
    EXPECT_EQ(result.excludeCredentials[0].transports[2], AuthenticatorTransport::Internal);

    EXPECT_EQ(result.authenticatorSelection->authenticatorAttachment, AuthenticatorAttachment::Platform);
    EXPECT_EQ(result.authenticatorSelection->requireResidentKey, true);
    EXPECT_EQ(result.authenticatorSelection->userVerification, UserVerificationRequirement::Required);

    EXPECT_EQ(result.attestation, AttestationConveyancePreference::Direct);
}

TEST(WebAuthenticationPanel, PublicKeyCredentialCreationOptionsMaximum2)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto parameters1 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);
    auto parameters2 = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-257]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    [rp setIcon:@"https//www.example.com/icon.jpg"];
    [rp setIdentifier:@"example.com"];

    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    [user setIcon:@"https//www.example.com/icon.jpg"];

    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters1.get(), parameters2.get() ];

    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    [options setTimeout:@120];

    auto usb = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportUSB]);
    auto nfc = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportNFC]);
    auto internal = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportInternal]);
    auto credential = adoptNS([[_WKPublicKeyCredentialDescriptor alloc] initWithIdentifier:nsIdentifier]);
    [credential setTransports:@[ usb.get(), nfc.get(), internal.get() ]];
    [options setExcludeCredentials:@[ credential.get(), credential.get() ]];

    auto authenticatorSelection = adoptNS([[_WKAuthenticatorSelectionCriteria alloc] init]);
    [authenticatorSelection setAuthenticatorAttachment:_WKAuthenticatorAttachmentCrossPlatform]; //
    [authenticatorSelection setRequireResidentKey:YES];
    [authenticatorSelection setUserVerification:_WKUserVerificationRequirementDiscouraged]; //
    [options setAuthenticatorSelection:authenticatorSelection.get()];

    [options setAttestation:_WKAttestationConveyancePreferenceIndirect]; //

    auto result = [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options.get()];

    EXPECT_WK_STREQ(result.rp.name, "example.com");
    EXPECT_WK_STREQ(result.rp.icon, @"https//www.example.com/icon.jpg");
    EXPECT_WK_STREQ(*result.rp.id, "example.com");

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_WK_STREQ(result.user.icon, @"https//www.example.com/icon.jpg");
    EXPECT_EQ(result.user.id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 2lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_EQ(result.excludeCredentials[0].transports.size(), 3lu);
    EXPECT_EQ(result.excludeCredentials[0].transports[0], AuthenticatorTransport::Usb);
    EXPECT_EQ(result.excludeCredentials[0].transports[1], AuthenticatorTransport::Nfc);
    EXPECT_EQ(result.excludeCredentials[0].transports[2], AuthenticatorTransport::Internal);

    EXPECT_EQ(result.authenticatorSelection->authenticatorAttachment, AuthenticatorAttachment::CrossPlatform);
    EXPECT_EQ(result.authenticatorSelection->requireResidentKey, true);
    EXPECT_EQ(result.authenticatorSelection->userVerification, UserVerificationRequirement::Discouraged);

    EXPECT_EQ(result.attestation, AttestationConveyancePreference::Indirect);
}

TEST(WebAuthenticationPanel, MakeCredentialSPITimeout)
{
    reset();

    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];
    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    [options setTimeout:@10];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel makeCredentialWithChallenge:nsHash origin:@"" options:options.get() completionHandler:^(_WKAuthenticatorAttestationResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;

        EXPECT_NULL(response);
        EXPECT_EQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, NotAllowedError);
    }];
    Util::run(&webAuthenticationPanelRan);
}

// For macOS, only internal builds can sign keychain entitlemnets
// which are required to run local authenticator tests.
#if USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)
TEST(WebAuthenticationPanel, MakeCredentialLA)
{
    reset();

    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto nsHash = adoptNS([[NSData alloc] initWithBytes:hash length:sizeof(hash)]);
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    [rp setIdentifier:@"example.com"];
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];
    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ @"privateKeyBase64": testES256PrivateKeyBase64 }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel makeCredentialWithChallenge:nsHash.get() origin:@"https://example.com" options:options.get() completionHandler:^(_WKAuthenticatorAttestationResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain("example.com"_s);

        EXPECT_TRUE(laContextRequested);
        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_WK_STREQ([[NSString alloc] initWithData:response.clientDataJSON encoding:NSUTF8StringEncoding], "{\"type\":\"webauthn.create\",\"challenge\":\"AQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQ\",\"origin\":\"https://example.com\"}");
        EXPECT_WK_STREQ([response.rawId base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
        EXPECT_NULL(response.extensions);
        EXPECT_WK_STREQ([response.attestationObject base64EncodedStringWithOptions:0], "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YViYo3mm9u6vuaVeN4wRgDTidR5oL6ufLTCrE9ISVYbOGUdFAAAAAAAAAAAAAAAAAAAAAAAAAAAAFEjElx54Be4RDrBJQO9wt0WPvG0epQECAyYgASFYIDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749IlggVBJPgqUIwfhWHJ91nb7UPH76c0+WFOzZKslPyyFse4g=");
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, MakeCredentialLAClientDataHashMediation)
{
    reset();

    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto nsHash = adoptNS([[NSData alloc] initWithBytes:hash length:sizeof(hash)]);
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    [rp setIdentifier:@"example.com"];
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];
    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ @"privateKeyBase64": testES256PrivateKeyBase64 }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel makeCredentialWithMediationRequirement:_WKWebAuthenticationMediationRequirementOptional clientDataHash:nsHash.get() options:options.get() completionHandler:^(_WKAuthenticatorAttestationResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain("example.com"_s);

        EXPECT_TRUE(laContextRequested);
        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_NULL(response.clientDataJSON);
        // This is the sha1 hash of the public key of testES256PrivateKeyBase64.
        EXPECT_WK_STREQ([response.rawId base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
        EXPECT_NULL(response.extensions);
        EXPECT_WK_STREQ([response.attestationObject base64EncodedStringWithOptions:0], "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YViYo3mm9u6vuaVeN4wRgDTidR5oL6ufLTCrE9ISVYbOGUdFAAAAAAAAAAAAAAAAAAAAAAAAAAAAFEjElx54Be4RDrBJQO9wt0WPvG0epQECAyYgASFYIDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749IlggVBJPgqUIwfhWHJ91nb7UPH76c0+WFOzZKslPyyFse4g=");
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, MakeCredentialLAAttestationFalback)
{
    reset();

    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto nsHash = adoptNS([[NSData alloc] initWithBytes:hash length:sizeof(hash)]);
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    [rp setIdentifier:@"example.com"];
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];
    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);
    options.get().attestation = _WKAttestationConveyancePreferenceDirect;

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ @"privateKeyBase64": testES256PrivateKeyBase64 }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel makeCredentialWithClientDataHash:nsHash.get() options:options.get() completionHandler:^(_WKAuthenticatorAttestationResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain("example.com"_s);

        EXPECT_NOT_NULL(response);
        // {"fmt": "none", "attStmt": {}, "authData": ...}
        EXPECT_WK_STREQ([response.attestationObject base64EncodedStringWithOptions:0], "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YViYo3mm9u6vuaVeN4wRgDTidR5oL6ufLTCrE9ISVYbOGUdFAAAAAAAAAAAAAAAAAAAAAAAAAAAAFEjElx54Be4RDrBJQO9wt0WPvG0epQECAyYgASFYIDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749IlggVBJPgqUIwfhWHJ91nb7UPH76c0+WFOzZKslPyyFse4g=");
    }];
    Util::run(&webAuthenticationPanelRan);
}
#endif

TEST(WebAuthenticationPanel, PublicKeyCredentialRequestOptionsMinimun)
{
    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    auto result = [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options.get()];

    EXPECT_EQ(result.timeout, std::nullopt);
    EXPECT_TRUE(result.rpId.isNull());
    EXPECT_EQ(result.allowCredentials.size(), 0lu);
    EXPECT_EQ(result.userVerification, UserVerificationRequirement::Preferred);
    EXPECT_TRUE(result.extensions->appid.isNull());
    EXPECT_EQ(result.extensions->googleLegacyAppidSupport, false);
}

TEST(WebAuthenticationPanel, PublicKeyCredentialRequestOptionsMaximumDefault)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto descriptor = adoptNS([[_WKPublicKeyCredentialDescriptor alloc] initWithIdentifier:nsIdentifier]);
    auto extensions = adoptNS([[_WKAuthenticationExtensionsClientInputs alloc] init]);

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setTimeout:@120];
    [options setAllowCredentials:@[ descriptor.get() ]];
    [options setExtensions:extensions.get()];

    auto result = [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options.get()];

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.allowCredentials.size(), 1lu);
    EXPECT_EQ(result.allowCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.allowCredentials[0].id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.allowCredentials[0].id.data(), identifier, sizeof(identifier)), 0);

    EXPECT_EQ(result.userVerification, UserVerificationRequirement::Preferred);
    EXPECT_TRUE(result.extensions->appid.isNull());
}

TEST(WebAuthenticationPanel, PublicKeyCredentialRequestOptionsMaximum)
{
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setTimeout:@120];

    auto usb = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportUSB]);
    auto nfc = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportNFC]);
    auto internal = adoptNS([NSNumber numberWithInt:_WKWebAuthenticationTransportInternal]);
    auto credential = adoptNS([[_WKPublicKeyCredentialDescriptor alloc] initWithIdentifier:nsIdentifier]);
    [credential setTransports:@[ usb.get(), nfc.get(), internal.get() ]];
    [options setAllowCredentials:@[ credential.get(), credential.get() ]];

    [options setUserVerification:_WKUserVerificationRequirementRequired];

    auto extensions = adoptNS([[_WKAuthenticationExtensionsClientInputs alloc] init]);
    [extensions setAppid:@"https//www.example.com/fido"];
    [options setExtensions:extensions.get()];

    auto result = [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options.get()];

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.allowCredentials.size(), 2lu);
    EXPECT_EQ(result.allowCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.allowCredentials[0].id.length(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.allowCredentials[0].id.data(), identifier, sizeof(identifier)), 0);
    EXPECT_EQ(result.allowCredentials[0].transports.size(), 3lu);
    EXPECT_EQ(result.allowCredentials[0].transports[0], AuthenticatorTransport::Usb);
    EXPECT_EQ(result.allowCredentials[0].transports[1], AuthenticatorTransport::Nfc);
    EXPECT_EQ(result.allowCredentials[0].transports[2], AuthenticatorTransport::Internal);

    EXPECT_EQ(result.userVerification, UserVerificationRequirement::Required);
    EXPECT_WK_STREQ(result.extensions->appid, "https//www.example.com/fido");
}

TEST(WebAuthenticationPanel, GetAssertionSPITimeout)
{
    reset();

    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setTimeout:@120];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel getAssertionWithChallenge:nsHash origin:@"" options:options.get() completionHandler:^(_WKAuthenticatorAssertionResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;

        EXPECT_NULL(response);
        EXPECT_EQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, NotAllowedError);
    }];
    Util::run(&webAuthenticationPanelRan);
}

// For macOS, only internal builds can sign keychain entitlemnets
// which are required to run local authenticator tests.
#if USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)
TEST(WebAuthenticationPanel, GetAssertionLA)
{
    reset();
    auto beforeTime = adoptNS([[NSDate alloc] init]);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));
    
    auto *credentialsBefore = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentialsBefore);
    EXPECT_EQ([credentialsBefore count], 1lu);
    EXPECT_NOT_NULL([credentialsBefore firstObject]);
    EXPECT_EQ([[credentialsBefore firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:[credentialsBefore firstObject][_WKLocalAuthenticatorCredentialCreationDateKey]], 0);
    EXPECT_GE([[credentialsBefore firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:beforeTime.get()], 0);

    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setRelyingPartyIdentifier:@"example.com"];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel getAssertionWithChallenge:nsHash origin:@"https://example.com" options:options.get() completionHandler:^(_WKAuthenticatorAssertionResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;

        auto *credentialsAfter = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
        EXPECT_NOT_NULL(credentialsAfter);
        EXPECT_EQ([credentialsAfter count], 1lu);
        EXPECT_NOT_NULL([credentialsAfter firstObject]);
        EXPECT_GE([[credentialsAfter firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:[credentialsAfter firstObject][_WKLocalAuthenticatorCredentialCreationDateKey]], 0);
        cleanUpKeychain("example.com"_s);

        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_WK_STREQ([[NSString alloc] initWithData:response.clientDataJSON encoding:NSUTF8StringEncoding], "{\"type\":\"webauthn.get\",\"challenge\":\"AQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQ\",\"origin\":\"https://example.com\"}");
        EXPECT_WK_STREQ([response.rawId base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
        EXPECT_NULL(response.extensions);

        // echo -n "example.com" | shasum -a 256 | xxd -r -p | base64
        NSString *base64RPIDHash = @"o3mm9u6vuaVeN4wRgDTidR5oL6ufLTCrE9ISVYbOGUc=";
        constexpr uint8_t additionalAuthenticatorData[] = {
            0x05, // 'flags': UV=1, UP=1

            // 32-bit 'signCount'
            0x00,
            0x00,
            0x00,
            0x00,
        };
        NSMutableData *expectedAuthenticatorData = [[NSMutableData alloc] initWithBase64EncodedString:base64RPIDHash options:0];
        [expectedAuthenticatorData appendBytes:additionalAuthenticatorData length:sizeof(additionalAuthenticatorData)];

        EXPECT_WK_STREQ([response.authenticatorData base64EncodedStringWithOptions:0], [expectedAuthenticatorData base64EncodedStringWithOptions:0]);
        EXPECT_NOT_NULL(response.signature);
        EXPECT_WK_STREQ([response.userHandle base64EncodedStringWithOptions:0], "AAECAwQFBgcICQ==");
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, GetAssertionLAClientDataHashMediation)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));

    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setRelyingPartyIdentifier:@"example.com"];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel getAssertionWithMediationRequirement:_WKWebAuthenticationMediationRequirementOptional clientDataHash:nsHash options:options.get() completionHandler:^(_WKAuthenticatorAssertionResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain("example.com"_s);

        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_NULL(response.clientDataJSON);
        // This is the sha1 hash of the public key of testES256PrivateKeyBase64.
        EXPECT_WK_STREQ([response.rawId base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
        EXPECT_NULL(response.extensions);

        // echo -n "example.com" | shasum -a 256 | xxd -r -p | base64
        NSString *base64RPIDHash = @"o3mm9u6vuaVeN4wRgDTidR5oL6ufLTCrE9ISVYbOGUc=";
        constexpr uint8_t additionalAuthenticatorData[] = {
            0x05, // 'flags': UV=1, UP=1

            // 32-bit 'signCount'
            0x00,
            0x00,
            0x00,
            0x00,
        };
        NSMutableData *expectedAuthenticatorData = [[NSMutableData alloc] initWithBase64EncodedString:base64RPIDHash options:0];
        [expectedAuthenticatorData appendBytes:additionalAuthenticatorData length:sizeof(additionalAuthenticatorData)];

        EXPECT_WK_STREQ([response.authenticatorData base64EncodedStringWithOptions:0], [expectedAuthenticatorData base64EncodedStringWithOptions:0]);
        EXPECT_NOT_NULL(response.signature);
        EXPECT_WK_STREQ([response.userHandle base64EncodedStringWithOptions:0], "AAECAwQFBgcICQ==");
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, GetAssertionNullUserHandle)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleNoUserHandleBase64));

    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setRelyingPartyIdentifier:@"example.com"];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel getAssertionWithClientDataHash:nsHash options:options.get() completionHandler:^(_WKAuthenticatorAssertionResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain("example.com"_s);

        EXPECT_NULL(error);

        EXPECT_NULL(response.userHandle);
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, GetAssertionCrossPlatform)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, emptyString(), testUserEntityBundleBase64));

    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    NSData *nsHash = [NSData dataWithBytes:hash length:sizeof(hash)];

    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);
    [options setRelyingPartyIdentifier:@""];
    [options setAuthenticatorAttachment:_WKAuthenticatorAttachmentCrossPlatform];
    [options setTimeout:@120];

    auto panel = adoptNS([[_WKWebAuthenticationPanel alloc] init]);
    [panel setMockConfiguration:@{ }];
    auto delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    [panel setDelegate:delegate.get()];

    [panel getAssertionWithChallenge:nsHash origin:@"" options:options.get() completionHandler:^(_WKAuthenticatorAssertionResponse *response, NSError *error) {
        webAuthenticationPanelRan = true;
        cleanUpKeychain(emptyString());

        EXPECT_NULL(response);
        EXPECT_EQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, NotAllowedError);
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, GetAllCredential)
{
    reset();

    auto before = adoptNS([[NSDate alloc] init]);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));

    auto after = adoptNS([[NSDate alloc] init]);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_WK_STREQ([[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
    EXPECT_WK_STREQ([[credentials firstObject][_WKLocalAuthenticatorCredentialUserHandleKey] base64EncodedStringWithOptions:0], "AAECAwQFBgcICQ==");
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialRelyingPartyIDKey], "example.com");

    EXPECT_GE([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:before.get()], 0);
    EXPECT_LE([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:after.get()], 0);
    EXPECT_EQ([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:[credentials firstObject][_WKLocalAuthenticatorCredentialCreationDateKey]], 0);

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, GetAllCredentialNullUserHandle)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleNoUserHandleBase64));

    auto after = adoptNS([[NSDate alloc] init]);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_EQ([credentials firstObject][_WKLocalAuthenticatorCredentialUserHandleKey], [NSNull null]);

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, GetAllCredentialWithDisplayName)
{
    reset();

    // {"id": h'00010203040506070809', "name": "John", "displayName": "Johnny"}
    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, "o2JpZEoAAQIDBAUGBwgJZG5hbWVkSm9obmtkaXNwbGF5TmFtZWZKb2hubnk="_s));

    auto after = adoptNS([[NSDate alloc] init]);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey], "Johnny");

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, GetAllCredentialByRPID)
{
    reset();

    // {"id": h'00010203040506070809', "name": "John", "displayName": "Johnny"}
    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, "o2JpZEoAAQIDBAUGBwgJZG5hbWVkSm9obmtkaXNwbGF5TmFtZWZKb2hubnk="_s));

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64Alternate, "example2.com"_s, "o2JpZEoAAQIDBAUGBwgJZG5hbWVkSm9obmtkaXNwbGF5TmFtZWZKb2hubnk="_s));

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithRPIDAndAccessGroup:@"com.apple.TestWebKitAPI" rpID:@"example.com"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialRelyingPartyIDKey], "example.com");

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithRPIDAndAccessGroup:@"com.apple.TestWebKitAPI" rpID:@"example2.com"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialRelyingPartyIDKey], "example2.com");

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithRPIDAndAccessGroup:@"com.apple.TestWebKitAPI" rpID:@"example3.com"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 0lu);

    cleanUpKeychain("example.com"_s);
    cleanUpKeychain("example2.com"_s);
}

TEST(WebAuthenticationPanel, GetAllCredentialByCredentialID)
{
    reset();
    cleanUpKeychain("example.com"_s);

    // {"id": h'00010203040506070809', "name": "John", "displayName": "Johnny"}
    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, "o2JpZEoAAQIDBAUGBwgJZG5hbWVkSm9obmtkaXNwbGF5TmFtZWZKb2hubnk="_s));

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithCredentialIDAndAccessGroup:@"com.apple.TestWebKitAPI" credentialID:[[NSData alloc] initWithBase64EncodedString:@"SMSXHngF7hEOsElA73C3RY+8bR4=" options:0]];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithCredentialIDAndAccessGroup:@"com.apple.TestWebKitAPI" credentialID:[[NSData alloc] initWithBase64EncodedString:@"SMSXHngF7hEOsElA73C3RY+8bR8=" options:0]];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 0lu);

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, EncodeCTAPAssertion)
{
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    auto nsHash = adoptNS([[NSData alloc] initWithBytes:hash length:sizeof(hash)]);
    auto options = adoptNS([[_WKPublicKeyCredentialRequestOptions alloc] init]);

    auto *command = [_WKWebAuthenticationPanel encodeGetAssertionCommandWithClientDataHash:nsHash.get() options: options.get() userVerificationAvailability:_WKWebAuthenticationUserVerificationAvailabilityNotSupported];

    // Base64 of the following CBOR:
    // 2, {1: "", 2: h'0102030401020304010203040102030401020304010203040102030401020304', 5: {"up": true}}
    EXPECT_WK_STREQ([command base64EncodedStringWithOptions:0], "AqMBYAJYIAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQBAgMEBaFidXD1");
}

TEST(WebAuthenticationPanel, EncodeCTAPCreation)
{
    uint8_t hash[] = { 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 };
    auto nsHash = adoptNS([[NSData alloc] initWithBytes:hash length:sizeof(hash)]);
    uint8_t identifier[] = { 0x01, 0x02, 0x03, 0x04 };
    NSData *nsIdentifier = [NSData dataWithBytes:identifier length:sizeof(identifier)];
    auto parameters = adoptNS([[_WKPublicKeyCredentialParameters alloc] initWithAlgorithm:@-7]);

    auto rp = adoptNS([[_WKPublicKeyCredentialRelyingPartyEntity alloc] initWithName:@"example.com"]);
    auto user = adoptNS([[_WKPublicKeyCredentialUserEntity alloc] initWithName:@"jappleseed@example.com" identifier:nsIdentifier displayName:@"J Appleseed"]);
    NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters = @[ parameters.get() ];

    auto options = adoptNS([[_WKPublicKeyCredentialCreationOptions alloc] initWithRelyingParty:rp.get() user:user.get() publicKeyCredentialParamaters:publicKeyCredentialParamaters]);

    auto *command = [_WKWebAuthenticationPanel encodeMakeCredentialCommandWithClientDataHash:nsHash.get() options: options.get() userVerificationAvailability:_WKWebAuthenticationUserVerificationAvailabilityNotSupported];

    // Base64 of the following CBOR:
    // 1, {1: h'0102030401020304010203040102030401020304010203040102030401020304', 2: {"name": "example.com"}, 3: {"id": h'01020304', "name": "jappleseed@example.com", "displayName": "J Appleseed"}, 4: [{"alg": -7, "type": "public-key"}]}
    EXPECT_WK_STREQ([command base64EncodedStringWithOptions:0], "AaQBWCABAgMEAQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAKhZG5hbWVrZXhhbXBsZS5jb20Do2JpZEQBAgMEZG5hbWV2amFwcGxlc2VlZEBleGFtcGxlLmNvbWtkaXNwbGF5TmFtZWtKIEFwcGxlc2VlZASBomNhbGcmZHR5cGVqcHVibGljLWtleQ==");
}

TEST(WebAuthenticationPanel, UpdateCredentialDisplayName)
{
    reset();
    cleanUpKeychain("example.com"_s);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_NULL([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey]);

    [_WKWebAuthenticationPanel setDisplayNameForLocalCredentialWithGroupAndID:nil credential:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] displayName: @"Saffron"];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey], "Saffron");

    [_WKWebAuthenticationPanel setDisplayNameForLocalCredentialWithGroupAndID:nil credential:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] displayName: @"Something Different"];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey], "Something Different");

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, UpdateCredentialName)
{
    reset();
    cleanUpKeychain("example.com"_s);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_NULL([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey]);

    [_WKWebAuthenticationPanel setNameForLocalCredentialWithGroupAndID:nil credential:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] name:@"Jill"];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "Jill");
    EXPECT_NULL([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey]);

    [_WKWebAuthenticationPanel setNameForLocalCredentialWithGroupAndID:nil credential:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] name: @"Something Different"];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "Something Different");
    EXPECT_NULL([credentials firstObject][_WKLocalAuthenticatorCredentialDisplayNameKey]);

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, ExportImportCredential)
{
    reset();
    cleanUpKeychain("example.com"_s);

    addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAccessGroup];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);
    EXPECT_EQ([[_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAlternateAccessGroup] count], 0lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    NSError *error = nil;
    auto exportedKey = [_WKWebAuthenticationPanel exportLocalAuthenticatorCredentialWithID:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] error:&error];
    
    cleanUpKeychain("example.com"_s);

    EXPECT_EQ([[_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAccessGroup] count], 0lu);

    auto credentialId = [_WKWebAuthenticationPanel importLocalAuthenticatorWithAccessGroup:testWebKitAPIAlternateAccessGroup credential:exportedKey error:&error];
    EXPECT_WK_STREQ([[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] base64EncodedStringWithOptions:0], [credentialId base64EncodedStringWithOptions:0]);

    EXPECT_EQ([[_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAccessGroup] count], 0lu);
    EXPECT_EQ([[_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAlternateAccessGroup] count], 1lu);
    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, ExportImportDuplicateCredential)
{
    reset();
    cleanUpKeychain(emptyString());

    addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAccessGroup];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    NSError *error = nil;
    auto exportedKey = [_WKWebAuthenticationPanel exportLocalAuthenticatorCredentialWithID:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] error:&error];
    cleanUpKeychain("example.com"_s);

    auto credentialId = [_WKWebAuthenticationPanel importLocalAuthenticatorWithAccessGroup:testWebKitAPIAccessGroup credential:exportedKey error:&error];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:testWebKitAPIAccessGroup];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);
    addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64, [credentials firstObject][_WKLocalAuthenticatorCredentialSynchronizableKey]);

    credentialId = [_WKWebAuthenticationPanel importLocalAuthenticatorWithAccessGroup:testWebKitAPIAccessGroup credential:exportedKey error:&error];
    EXPECT_EQ(credentialId, nil);
    EXPECT_EQ(error.code, WKErrorDuplicateCredential);

    cleanUpKeychain("example.com"_s);
}

TEST(WebAuthenticationPanel, ImportMalformedCredential)
{
    reset();

    NSError *error = nil;
    auto credentialId = [_WKWebAuthenticationPanel importLocalAuthenticatorCredential:adoptNS([[NSData alloc] initWithBase64EncodedString:testUserEntityBundleBase64 options:0]).get() error:&error];

    EXPECT_EQ(error.code, WKErrorMalformedCredential);
    EXPECT_EQ(credentialId, nil);
}

TEST(WebAuthenticationPanel, DeleteOneCredential)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com"_s, testUserEntityBundleBase64));

    [_WKWebAuthenticationPanel deleteLocalAuthenticatorCredentialWithID:adoptNS([[NSData alloc] initWithBase64EncodedString:@"SMSXHngF7hEOsElA73C3RY+8bR4=" options:0]).get()];

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 0lu);
}
#endif // USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)

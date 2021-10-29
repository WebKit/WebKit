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
#import "Test.h"

#if ENABLE(WEB_AUTHN)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
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
#import <wtf/RandomNumber.h>
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
static _WKLocalAuthenticatorPolicy localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyDisallow;
static String webAuthenticationPanelPin;
static BOOL webAuthenticationPanelNullUserHandle = NO;
static String testES256PrivateKeyBase64 =
    "BDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749VBJPgqUIwfhWHJ91nb7U"
    "PH76c0+WFOzZKslPyyFse4goGIW2R7k9VHLPEZl5nfnBgEVFh5zev+/xpHQIvuq6"
    "RQ==";
static String testUserEntityBundleBase64 = "omJpZEoAAQIDBAUGBwgJZG5hbWVkSm9obg=="; // { "id": h'00010203040506070809', "name": "John" }
static String webAuthenticationPanelSelectedCredentialName;
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
        completionHandler(object);
        return;
    }

    EXPECT_EQ(source, _WKWebAuthenticationSourceExternal);
    EXPECT_EQ(responses.count, 2ul);
    for (_WKWebAuthenticationAssertionResponse *response in responses) {
        EXPECT_TRUE([response.name isEqual:@"johnpsmith@example.com"] || [response.name isEqual:@""]);
        EXPECT_TRUE([response.displayName isEqual:@"John P. Smith"] || [response.displayName isEqual:@""]);
        EXPECT_TRUE([[response.userHandle base64EncodedStringWithOptions:0] isEqual:@"MIIBkzCCATigAwIBAjCCAZMwggE4oAMCAQIwggGTMII="] || !response.userHandle);
    }

    auto index = weakRandomUint32() % 2;
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

const char parentFrame[] = "<html><iframe id='theFrame' src='iFrame.html'></iframe></html>";
const char subFrame[] =
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
"</html>";

static _WKExperimentalFeature *webAuthenticationExperimentalFeature()
{
    static RetainPtr<_WKExperimentalFeature> theFeature;
    if (theFeature)
        return theFeature.get();

    NSArray *features = [WKPreferences _experimentalFeatures];
    for (_WKExperimentalFeature *feature in features) {
        if ([feature.key isEqual:@"WebAuthenticationEnabled"]) {
            theFeature = feature;
            break;
        }
    }
    return theFeature.get();
}

static _WKExperimentalFeature *webAuthenticationModernExperimentalFeature()
{
    static RetainPtr<_WKExperimentalFeature> theFeature;
    if (theFeature)
        return theFeature.get();

    NSArray *features = [WKPreferences _experimentalFeatures];
    for (_WKExperimentalFeature *feature in features) {
        if ([feature.key isEqual:@"WebAuthenticationModernEnabled"]) {
            theFeature = feature;
            break;
        }
    }
    return theFeature.get();
}

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

bool addKeyToKeychain(const String& privateKeyBase64, const String& rpId, const String& userHandleBase64)
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

    NSDictionary* addQuery = @{
        (id)kSecValueRef: (id)key.get(),
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: rpId,
        (id)kSecAttrApplicationTag: adoptNS([[NSData alloc] initWithBase64EncodedString:userHandleBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain: @YES
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    if (status)
        return false;

    return true;
}

void cleanUpKeychain(const String& rpId)
{
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: rpId,
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [[delegate panel] cancel];
    [webView waitForMessage:@"This request has been cancelled by the user."];
    EXPECT_TRUE(webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidCtapNoCredentialsFound)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-no-credentials" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "123";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdatePINInvalid);
    webAuthenticationPanelPin = "1234";
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, PinGetPinTokenPinAuthInvalidErrorAndRetry)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-get-pin-token-pin-auth-invalid-error-retry" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, MakeCredentialInternalUV)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-internal-uv" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, MakeCredentialPinAuthBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-hid-pin-auth-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_TRUE(webAuthenticationPanelUpdatePINInvalid);
}

TEST(WebAuthenticationPanel, GetAssertionPin)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-pin" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, GetAssertionInternalUV)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-internal-uv" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, GetAssertionPinAuthBlockedError)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-pin-auth-blocked-error" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    webAuthenticationPanelPin = "1234";
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}
#endif // HAVE(NEAR_FIELD)

TEST(WebAuthenticationPanel, MultipleAccountsNullDelegate)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-multiple-accounts" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

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
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "", testUserEntityBundleBase64));
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
    cleanUpKeychain("");
}

TEST(WebAuthenticationPanel, LADuplicateCredentialWithConsent)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-duplicate-credential" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "", testUserEntityBundleBase64));

    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyAllow;

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateLAExcludeCredentialsMatched);
    cleanUpKeychain("");
}

TEST(WebAuthenticationPanel, LANoCredential)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelUpdateLANoCredential);
}

TEST(WebAuthenticationPanel, LAMakeCredentialNullDelegate)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsNull:true];
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Disallow local authenticator."];
}

TEST(WebAuthenticationPanel, LAMakeCredentialDisallowLocalAuthenticator)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Disallow local authenticator."];
}

TEST(WebAuthenticationPanel, LAMakeCredentialAllowLocalAuthenticator)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyAllow;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get(), adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportInternal]).get()], _WKWebAuthenticationTypeCreate);
    cleanUpKeychain("");
}

TEST(WebAuthenticationPanel, LAMakeCredentialNoMockNoUserGesture)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-no-mock" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"This request has been cancelled by the user."];
}

TEST(WebAuthenticationPanel, LAMakeCredentialRollBackCredential)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-make-credential-la-no-attestation" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    localAuthenticatorPolicy = _WKLocalAuthenticatorPolicyAllow;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Couldn't attest: The operation couldn't complete."];

    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: @"",
        (id)kSecUseDataProtectionKeychain: @YES
    };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, nullptr);
    EXPECT_EQ(status, errSecItemNotFound);
}

#if PLATFORM(MAC)

TEST(WebAuthenticationPanel, LAGetAssertion)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "", testUserEntityBundleBase64));
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    checkPanel([delegate panel], @"", @[adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportUSB]).get(), adoptNS([[NSNumber alloc] initWithInt:_WKWebAuthenticationTransportInternal]).get()], _WKWebAuthenticationTypeGet);
    cleanUpKeychain("");
}

TEST(WebAuthenticationPanel, LAGetAssertionNoMockNoUserGesture)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la-no-mock" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"This request has been cancelled by the user."];
}

TEST(WebAuthenticationPanel, LAGetAssertionMultipleOrder)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-la" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:NO forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView focus];

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "", testUserEntityBundleBase64));
    ASSERT_TRUE(addKeyToKeychain("BBRoi2JbR0IXTeJmvXUp1YIuM4sph/Lu3eGf75F7n+HojHKG70a4R0rB2PQce5/SJle6T7OO5Cqet/LJZVM6NQ8yDDxWvayf71GTDp2yUtuIbqJLFVbpWymlj9WRizgX3A==", "", "omJpZEoAAQIDBAUGBwgJZG5hbWVkSmFuZQ=="/* { "id": h'00010203040506070809', "name": "Jane" } */));

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "John");

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
    EXPECT_WK_STREQ(webAuthenticationPanelSelectedCredentialName, "Jane");

    cleanUpKeychain("");
}

#endif // PLATFORM(MAC)

#endif // USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

TEST(WebAuthenticationPanel, PublicKeyCredentialCreationOptionsMinimun)
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
    EXPECT_TRUE(result.rp.id.isNull());

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_TRUE(result.user.icon.isNull());
    EXPECT_EQ(result.user.idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.idVector.data(), identifier, sizeof(identifier)), 0);
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
    EXPECT_TRUE(result.rp.id.isNull());

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_TRUE(result.user.icon.isNull());
    EXPECT_EQ(result.user.idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.idVector.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 1lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].idVector.data(), identifier, sizeof(identifier)), 0);

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
    EXPECT_WK_STREQ(result.rp.id, "example.com");

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_WK_STREQ(result.user.icon, @"https//www.example.com/icon.jpg");
    EXPECT_EQ(result.user.idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.idVector.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 2lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].idVector.data(), identifier, sizeof(identifier)), 0);
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
    EXPECT_WK_STREQ(result.rp.id, "example.com");

    EXPECT_WK_STREQ(result.user.name, "jappleseed@example.com");
    EXPECT_WK_STREQ(result.user.icon, @"https//www.example.com/icon.jpg");
    EXPECT_EQ(result.user.idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.user.idVector.data(), identifier, sizeof(identifier)), 0);
    EXPECT_WK_STREQ(result.user.displayName, "J Appleseed");

    EXPECT_EQ(result.pubKeyCredParams.size(), 2lu);
    EXPECT_EQ(result.pubKeyCredParams[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[0].alg, -7);
    EXPECT_EQ(result.pubKeyCredParams[1].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.pubKeyCredParams[1].alg, -257);

    EXPECT_EQ(result.timeout, 120u);

    EXPECT_EQ(result.excludeCredentials.size(), 2lu);
    EXPECT_EQ(result.excludeCredentials[0].type, WebCore::PublicKeyCredentialType::PublicKey);
    EXPECT_EQ(result.excludeCredentials[0].idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.excludeCredentials[0].idVector.data(), identifier, sizeof(identifier)), 0);
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
        cleanUpKeychain("example.com");

        EXPECT_TRUE(laContextRequested);
        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_WK_STREQ([[NSString alloc] initWithData:response.clientDataJSON encoding:NSUTF8StringEncoding], "{\"challenge\":\"AQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQ\",\"origin\":\"https://example.com\",\"type\":\"webauthn.create\"}");
        EXPECT_WK_STREQ([response.rawId base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
        EXPECT_NULL(response.extensions);
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
    EXPECT_EQ(result.allowCredentials[0].idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.allowCredentials[0].idVector.data(), identifier, sizeof(identifier)), 0);

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
    EXPECT_EQ(result.allowCredentials[0].idVector.size(), sizeof(identifier));
    EXPECT_EQ(memcmp(result.allowCredentials[0].idVector.data(), identifier, sizeof(identifier)), 0);
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
        EXPECT_EQ(error.code, WKErrorUnknown);
    }];
    Util::run(&webAuthenticationPanelRan);
}

// For macOS, only internal builds can sign keychain entitlemnets
// which are required to run local authenticator tests.
#if USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)
TEST(WebAuthenticationPanel, GetAssertionLA)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com", testUserEntityBundleBase64));

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
        cleanUpKeychain("example.com");

        EXPECT_NULL(error);

        EXPECT_NOT_NULL(response);
        EXPECT_WK_STREQ([[NSString alloc] initWithData:response.clientDataJSON encoding:NSUTF8StringEncoding], "{\"challenge\":\"AQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQ\",\"origin\":\"https://example.com\",\"type\":\"webauthn.get\"}");
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

TEST(WebAuthenticationPanel, GetAssertionCrossPlatform)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "", testUserEntityBundleBase64));

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
        cleanUpKeychain("");

        EXPECT_NULL(response);
        EXPECT_EQ(error.domain, WKErrorDomain);
        EXPECT_EQ(error.code, WKErrorUnknown);
    }];
    Util::run(&webAuthenticationPanelRan);
}

TEST(WebAuthenticationPanel, GetAllCredential)
{
    reset();

    auto before = adoptNS([[NSDate alloc] init]);

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com", testUserEntityBundleBase64));

    auto after = adoptNS([[NSDate alloc] init]);

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");
    EXPECT_WK_STREQ([[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] base64EncodedStringWithOptions:0], "SMSXHngF7hEOsElA73C3RY+8bR4=");
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialRelyingPartyIDKey], "example.com");

    EXPECT_GE([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:before.get()], 0);
    EXPECT_LE([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:after.get()], 0);
    EXPECT_EQ([[credentials firstObject][_WKLocalAuthenticatorCredentialLastModificationDateKey] compare:[credentials firstObject][_WKLocalAuthenticatorCredentialCreationDateKey]], 0);

    cleanUpKeychain("example.com");
}

TEST(WebAuthenticationPanel, UpdateCredentialUsername)
{
    reset();
    cleanUpKeychain("example.com");

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com", testUserEntityBundleBase64));

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "John");

    [_WKWebAuthenticationPanel setUsernameForLocalCredentialWithID:[credentials firstObject][_WKLocalAuthenticatorCredentialIDKey] username: @"Saffron"];

    credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 1lu);

    EXPECT_NOT_NULL([credentials firstObject]);
    EXPECT_WK_STREQ([credentials firstObject][_WKLocalAuthenticatorCredentialNameKey], "Saffron");

    cleanUpKeychain("example.com");
}

TEST(WebAuthenticationPanel, DeleteOneCredential)
{
    reset();

    ASSERT_TRUE(addKeyToKeychain(testES256PrivateKeyBase64, "example.com", testUserEntityBundleBase64));

    [_WKWebAuthenticationPanel deleteLocalAuthenticatorCredentialWithID:adoptNS([[NSData alloc] initWithBase64EncodedString:@"SMSXHngF7hEOsElA73C3RY+8bR4=" options:0]).get()];

    auto *credentials = [_WKWebAuthenticationPanel getAllLocalAuthenticatorCredentialsWithAccessGroup:@"com.apple.TestWebKitAPI"];
    EXPECT_NOT_NULL(credentials);
    EXPECT_EQ([credentials count], 0lu);
}
#endif // USE(APPLE_INTERNAL_SDK) || PLATFORM(IOS)

TEST(WebAuthenticationPanel, RecoverAfterAuthNProcessCrash)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationModernExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    EXPECT_EQ([WKProcessPool _webAuthnProcessIdentifier], 0);
    [webView objectByEvaluatingJavaScript:@"internals.setMockWebAuthenticationConfiguration({ })"];
    auto firstWebPID = [webView _webProcessIdentifier];
    auto firstWebAuthnPID = [WKProcessPool _webAuthnProcessIdentifier];
    EXPECT_NE(firstWebPID, 0);
    EXPECT_NE(firstWebAuthnPID, 0);

    kill(firstWebAuthnPID, SIGKILL);
    while ([WKProcessPool _webAuthnProcessIdentifier] || [webView _webProcessIdentifier])
        Util::spinRunLoop();
    [webView objectByEvaluatingJavaScript:@"internals.setMockWebAuthenticationConfiguration({ })"];
    auto secondWebPID = [webView _webProcessIdentifier];
    auto secondWebAuthnPID = [WKProcessPool _webAuthnProcessIdentifier];
    EXPECT_NE(secondWebAuthnPID, 0);
    EXPECT_NE(secondWebAuthnPID, firstWebAuthnPID);
    EXPECT_NE(secondWebPID, 0);
    EXPECT_NE(secondWebPID, firstWebPID);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)

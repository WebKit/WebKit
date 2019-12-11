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
#import <WebKit/WKFoundation.h>

#if PLATFORM(IOS_FAMILY)

#import "BundleFormDelegateProtocol.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFocusedElementInfo.h>
#import <WebKit/_WKInputDelegate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool didGetFocus;
static bool didGetUserObject;
static id userObject;

@interface BundleFormDelegateRemoteObject : NSObject <BundleFormDelegateProtocol>
@end

@implementation BundleFormDelegateRemoteObject

- (void)didGetFocus
{
    didGetFocus = true;
}

@end

@interface FormInputDelegate : NSObject <_WKInputDelegate> {
@public
    RetainPtr<id> _userObject;
}
@end

@implementation FormInputDelegate

- (_WKFocusStartsInputSessionPolicy)_webView:(WKWebView *)webView decidePolicyForFocusedElement:(id <_WKFocusedElementInfo>)info
{
    _userObject = info.userObject;
    didGetUserObject = true;
    return _WKFocusStartsInputSessionPolicyAllow;
}

@end

TEST(WebKit, WKWebProcessPlugInWithoutRegisteredCustomClass)
{
    auto delegate = adoptNS([[FormInputDelegate alloc] init]);

    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleFormDelegatePlugIn"]);
    [[configuration processPool] _setObject:@YES forBundleParameter:@"FormDelegateShouldAddCustomClass"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setInputDelegate:delegate.get()];
    [webView loadHTMLString:@"<input></input><select><option selected>foo</option><option>bar</option></select>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto object = adoptNS([[BundleFormDelegateRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleFormDelegateProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView evaluateJavaScript:@"document.querySelector('input').focus()" completionHandler:nil];

    userObject = nil;
    didGetUserObject = false;

    TestWebKitAPI::Util::run(&didGetFocus);
    TestWebKitAPI::Util::run(&didGetUserObject);

    // We should have serialized the NSUnitConverterLinear, since we did not indicate we wanted
    // to limit serialization.
    EXPECT_TRUE([delegate->_userObject isKindOfClass:[NSUnitConverterLinear class]]);
}

TEST(WebKit, WKWebProcessPlugInWithRegisteredCustomClass)
{
    auto delegate = adoptNS([[FormInputDelegate alloc] init]);

    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleFormDelegatePlugIn"
        configureJSCForTesting:NO andCustomParameterClasses:[NSSet setWithObjects:[NSUnitConverterLinear class], nil]]);
    [[configuration processPool] _setObject:@YES forBundleParameter:@"FormDelegateShouldAddCustomClass"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setInputDelegate:delegate.get()];
    [webView loadHTMLString:@"<input></input><select><option selected>foo</option><option>bar</option></select>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto object = adoptNS([[BundleFormDelegateRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleFormDelegateProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView evaluateJavaScript:@"document.querySelector('input').focus()" completionHandler:nil];
    
    userObject = nil;
    didGetUserObject = false;

    TestWebKitAPI::Util::run(&didGetFocus);
    TestWebKitAPI::Util::run(&didGetUserObject);
    
    EXPECT_TRUE([delegate->_userObject isKindOfClass:[NSUnitConverterLinear class]]);
}

TEST(WebKit, WKWebProcessPlugInWithUnregisteredCustomClass)
{
    auto delegate = adoptNS([[FormInputDelegate alloc] init]);

    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleFormDelegatePlugIn"
        configureJSCForTesting:NO andCustomParameterClasses:[NSSet setWithObjects:[NSUnitAcceleration class], nil]]);
    [[configuration processPool] _setObject:@YES forBundleParameter:@"FormDelegateShouldAddCustomClass"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setInputDelegate:delegate.get()];
    [webView loadHTMLString:@"<input></input><select><option selected>foo</option><option>bar</option></select>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto object = adoptNS([[BundleFormDelegateRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleFormDelegateProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView evaluateJavaScript:@"document.querySelector('input').focus()" completionHandler:nil];

    userObject = nil;
    didGetUserObject = false;

    TestWebKitAPI::Util::run(&didGetFocus);
    TestWebKitAPI::Util::run(&didGetUserObject);

    // We should not have deserialized the NSUnitConverterLinear, since we did not specify it as valid.
    EXPECT_NULL(delegate->_userObject);
}

#endif

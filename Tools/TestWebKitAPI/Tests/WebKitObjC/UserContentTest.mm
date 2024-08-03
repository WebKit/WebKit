/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI && PLATFORM(MAC)

#import "Test.h"

#import "PlatformUtilities.h"
#import "TestBrowsingContextLoadDelegate.h"
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKSerializedScriptValue.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

static bool testFinished = false;
static NSString *htmlString = @"<body style='background-color: red'>";
static NSString *userStyleSheet = @"body { background-color: green !important; }";
static const char* backgroundColorScript = "window.getComputedStyle(document.body, null).getPropertyValue('background-color')";
static const char* greenInRGB = "rgb(0, 128, 0)";
static const char* redInRGB = "rgb(255, 0, 0)";
static const char* userScriptTestProperty = "window._userScriptInstalled";

namespace {
    class WebKit2UserContentTest : public ::testing::Test {
    public:
        RetainPtr<WKProcessGroup> processGroup;

        WebKit2UserContentTest() = default;
        
        virtual void SetUp()
        {
            processGroup = adoptNS([[WKProcessGroup alloc] init]);
        }
        
        virtual void TearDown()
        {
            processGroup = nullptr;
        }
    };
} // namespace

static void expectScriptValueIsString(WKTypeRef value, const char* expectedValue)
{
    EXPECT_EQ(WKStringGetTypeID(), WKGetTypeID(value));
    EXPECT_TRUE(WKStringIsEqualToUTF8CString((WKStringRef)value, expectedValue));
}

static void expectScriptValueIsBoolean(WKTypeRef value, bool expectedValue)
{
    EXPECT_EQ(WKBooleanGetTypeID(), WKGetTypeID(value));
    EXPECT_EQ(WKBooleanGetValue((WKBooleanRef)value), expectedValue);
}

static void expectScriptValueIsUndefined(WKTypeRef value)
{
    EXPECT_FALSE(value);
}

typedef void (^RunJavaScriptBlock)(WKTypeRef, WKErrorRef);

static void callRunJavaScriptBlockAndRelease(WKTypeRef resultValue, WKErrorRef error, void* context)
{
    auto block = (RunJavaScriptBlock)context;
    block(resultValue, error);
    Block_release(block);
}

static void runJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, RunJavaScriptBlock block)
{
    WKPageEvaluateJavaScriptInMainFrame(pageRef, scriptRef, Block_copy(block), callRunJavaScriptBlockAndRelease);
}

TEST_F(WebKit2UserContentTest, AddUserStyleSheetBeforeCreatingView)
{
    testFinished = false;
    
    auto sheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:userStyleSheet forMainFrameOnly:YES]);

    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [wkView.get().configuration.userContentController _addUserStyleSheet:sheet.get()];
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], backgroundColorQuery.get(), ^(WKTypeRef value, WKErrorRef error) {
            expectScriptValueIsString(value, greenInRGB);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();
    
    [wkView loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserStyleSheetAfterCreatingView)
{
    testFinished = false;
    
    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], backgroundColorQuery.get(), ^(WKTypeRef result, WKErrorRef error) {
            expectScriptValueIsString(result, greenInRGB);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();

    auto sheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:userStyleSheet forMainFrameOnly:YES]);
    [wkView.get().configuration.userContentController _addUserStyleSheet:sheet.get()];

    [wkView loadHTMLString:htmlString baseURL:nil];

    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, RemoveAllUserStyleSheets)
{
    testFinished = false;
    auto sheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:userStyleSheet forMainFrameOnly:YES]);
    
    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [wkView.get().configuration.userContentController _addUserStyleSheet:sheet.get()];
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], backgroundColorQuery.get(), ^(WKTypeRef result, WKErrorRef error) {
            expectScriptValueIsString(result, redInRGB);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();
    
    [wkView.get().configuration.userContentController _removeAllUserStyleSheets];
    
    [wkView loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserScriptBeforeCreatingView)
{
    testFinished = false;
    auto script = adoptNS([[WKUserScript alloc] initWithSource:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [wkView.get().configuration.userContentController addUserScript:script.get()];
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], userScriptTestPropertyString.get(), ^(WKTypeRef result, WKErrorRef error) {
            expectScriptValueIsBoolean(result, true);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();
    
    [wkView loadHTMLString:@"" baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserScriptAfterCreatingView)
{
    testFinished = false;
    
    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], userScriptTestPropertyString.get(), ^(WKTypeRef result, WKErrorRef error) {
            expectScriptValueIsBoolean(result, true);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();
    
    auto script = adoptNS([[WKUserScript alloc] initWithSource:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);
    [wkView.get().configuration.userContentController addUserScript:script.get()];
    
    [wkView loadHTMLString:@"" baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, RemoveAllUserScripts)
{
    testFinished = false;
    auto script = adoptNS([[WKUserScript alloc] initWithSource:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]);

    auto wkView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [wkView.get().configuration.userContentController addUserScript:script.get()];
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKWebView *sender) {
        runJavaScriptInMainFrame([wkView _pageRefForTransitionToWKWebView], userScriptTestPropertyString.get(), ^(WKTypeRef value, WKErrorRef error) {
            expectScriptValueIsUndefined(value);
            testFinished = true;
        });
    }]);
    wkView.get().navigationDelegate = loadDelegate.get();
    
    [wkView.get().configuration.userContentController removeAllUserScripts];
    
    [wkView loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

#endif 

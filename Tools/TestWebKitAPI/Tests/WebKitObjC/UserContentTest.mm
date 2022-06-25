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
#import <WebKit/WKViewPrivate.h>
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
        RetainPtr<WKBrowsingContextGroup> browsingContextGroup;

        WebKit2UserContentTest() = default;
        
        virtual void SetUp()
        {
            processGroup = adoptNS([[WKProcessGroup alloc] init]);
            browsingContextGroup = adoptNS([[WKBrowsingContextGroup alloc] initWithIdentifier:@"UserContentIdentifier"]);
        }
        
        virtual void TearDown()
        {
            browsingContextGroup = nullptr;
            processGroup = nullptr;
        }
    };
} // namespace

static void expectScriptValueIsString(WKSerializedScriptValueRef serializedScriptValue, const char* expectedValue)
{
    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    
    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(serializedScriptValue, scriptContext, 0);
    EXPECT_TRUE(JSValueIsString(scriptContext, scriptValue));
    
    auto scriptString = adopt(JSValueToStringCopy(scriptContext, scriptValue, 0));
    EXPECT_TRUE(JSStringIsEqualToUTF8CString(scriptString.get(), expectedValue));
    
    JSGlobalContextRelease(scriptContext);
}

static void expectScriptValueIsBoolean(WKSerializedScriptValueRef serializedScriptValue, bool expectedValue)
{
    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    
    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(serializedScriptValue, scriptContext, 0);
    EXPECT_TRUE(JSValueIsBoolean(scriptContext, scriptValue));
    EXPECT_EQ(JSValueToBoolean(scriptContext, scriptValue), expectedValue);
    
    JSGlobalContextRelease(scriptContext);
}

static void expectScriptValueIsUndefined(WKSerializedScriptValueRef serializedScriptValue)
{
    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    
    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(serializedScriptValue, scriptContext, 0);
    EXPECT_TRUE(JSValueIsUndefined(scriptContext, scriptValue));
    
    JSGlobalContextRelease(scriptContext);
}

typedef void (^RunJavaScriptBlock)(WKSerializedScriptValueRef, WKErrorRef);

static void callRunJavaScriptBlockAndRelease(WKSerializedScriptValueRef resultValue, WKErrorRef error, void* context)
{
    auto block = (RunJavaScriptBlock)context;
    block(resultValue, error);
    Block_release(block);
}

static void runJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, RunJavaScriptBlock block)
{
    WKPageRunJavaScriptInMainFrame(pageRef, scriptRef, Block_copy(block), callRunJavaScriptBlockAndRelease);
}

TEST_F(WebKit2UserContentTest, AddUserStyleSheetBeforeCreatingView)
{
    testFinished = false;
    [browsingContextGroup addUserStyleSheet:userStyleSheet baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil mainFrameOnly:YES];
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], backgroundColorQuery.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsString(serializedScriptValue, greenInRGB);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    
    [[wkView browsingContextController] loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserStyleSheetAfterCreatingView)
{
    testFinished = false;
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], backgroundColorQuery.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsString(serializedScriptValue, greenInRGB);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();

    [browsingContextGroup addUserStyleSheet:userStyleSheet baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil mainFrameOnly:YES];
    
    [[wkView browsingContextController] loadHTMLString:htmlString baseURL:nil];

    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, RemoveAllUserStyleSheets)
{
    testFinished = false;
    [browsingContextGroup addUserStyleSheet:userStyleSheet baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil mainFrameOnly:YES];
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto backgroundColorQuery = adoptWK(WKStringCreateWithUTF8CString(backgroundColorScript));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], backgroundColorQuery.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsString(serializedScriptValue, redInRGB);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    
    [browsingContextGroup removeAllUserStyleSheets];
    
    [[wkView browsingContextController] loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserScriptBeforeCreatingView)
{
    testFinished = false;
    [browsingContextGroup addUserScript:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectionTime:kWKInjectAtDocumentStart mainFrameOnly:YES];
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], userScriptTestPropertyString.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsBoolean(serializedScriptValue, true);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    
    [[wkView browsingContextController] loadHTMLString:@"" baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, AddUserScriptAfterCreatingView)
{
    testFinished = false;
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], userScriptTestPropertyString.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsBoolean(serializedScriptValue, true);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    
    [browsingContextGroup addUserScript:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectionTime:kWKInjectAtDocumentStart mainFrameOnly:YES];
    
    [[wkView browsingContextController] loadHTMLString:@"" baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

TEST_F(WebKit2UserContentTest, RemoveAllUserScripts)
{
    testFinished = false;
    [browsingContextGroup addUserScript:[NSString stringWithFormat:@"%s = true;", userScriptTestProperty] baseURL:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectionTime:kWKInjectAtDocumentStart mainFrameOnly:YES];
    
    auto wkView = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    auto userScriptTestPropertyString = adoptWK(WKStringCreateWithUTF8CString(userScriptTestProperty));
    auto loadDelegate = adoptNS([[TestBrowsingContextLoadDelegate alloc] initWithBlockToRunOnLoad:^(WKBrowsingContextController *sender) {
        runJavaScriptInMainFrame([wkView pageRef], userScriptTestPropertyString.get(), ^(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error) {
            expectScriptValueIsUndefined(serializedScriptValue);
            testFinished = true;
        });
    }]);
    [wkView browsingContextController].loadDelegate = loadDelegate.get();
    
    [browsingContextGroup removeAllUserScripts];
    
    [[wkView browsingContextController] loadHTMLString:htmlString baseURL:nil];
    
    TestWebKitAPI::Util::run(&testFinished);
}

#endif 

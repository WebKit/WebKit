/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <Carbon/Carbon.h> // for GetCurrentEventTime()
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

@interface CommandBackForwardOffscreenWindow : NSWindow
@end

@implementation CommandBackForwardOffscreenWindow
- (BOOL)isKeyWindow
{
    return YES;
}
- (BOOL)isVisible
{
    return YES;
}
@end

enum ArrowDirection {
    Left,
    Right
};

static void simulateCommandArrow(NSView *view, ArrowDirection direction)
{
    const unichar right = NSRightArrowFunctionKey;
    const unichar left = NSLeftArrowFunctionKey;

    NSString *eventCharacter = (direction == Left) ? [NSString stringWithCharacters:&left length:1] : [NSString stringWithCharacters:&right length:1];
    unsigned short keyCode = (direction == Left) ? 0x7B : 0x7C;

    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                      location:NSMakePoint(5, 5)
                                 modifierFlags:NSEventModifierFlagCommand
                                     timestamp:GetCurrentEventTime()
                                  windowNumber:[view.window windowNumber]
                                       context:[NSGraphicsContext currentContext]
                                    characters:eventCharacter
                   charactersIgnoringModifiers:eventCharacter
                                     isARepeat:NO
                                       keyCode:keyCode];

    [view keyDown:event];

    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                             location:NSMakePoint(5, 5)
                        modifierFlags:NSEventModifierFlagCommand
                            timestamp:GetCurrentEventTime()
                         windowNumber:[view.window windowNumber]
                              context:[NSGraphicsContext currentContext]
                           characters:eventCharacter
          charactersIgnoringModifiers:eventCharacter
                            isARepeat:NO
                              keyCode:keyCode];

    [view keyUp:event];
}

class WebKit2_CommandBackForwardTest : public testing::Test {
public:
    RetainPtr<NSWindow> window;

    virtual void SetUp()
    {
        NSWindow *window = [[CommandBackForwardOffscreenWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [window setColorSpace:[[NSScreen mainScreen] colorSpace]];
        [window orderBack:nil];
        [window setAutodisplay:NO];
        [window setReleasedWhenClosed:NO];
    }
};

static bool didFinishNavigation;

class WebKit2_CommandBackForwardTestWKView : public WebKit2_CommandBackForwardTest {
public:
    RetainPtr<WKView> webView;
    WKRetainPtr<WKURLRef> file1;
    WKRetainPtr<WKURLRef> file2;

    virtual void SetUp()
    {
        WebKit2_CommandBackForwardTest::SetUp();

        WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
        WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());        
        WKPageConfigurationSetContext(configuration.get(), context.get());

        webView = [[WKView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configurationRef:configuration.get()];

        WKPageNavigationClientV0 loaderClient;
        memset(&loaderClient, 0, sizeof(loaderClient));

        loaderClient.base.version = 0;
        loaderClient.didFinishNavigation = [] (WKPageRef, WKNavigationRef, WKTypeRef, const void* clientInfo) {
            didFinishNavigation = true;
        };

        WKPageSetPageNavigationClient([webView pageRef], &loaderClient.base);
        
        file1 = adoptWK(TestWebKitAPI::Util::createURLForResource("simple", "html"));
        file2 = adoptWK(TestWebKitAPI::Util::createURLForResource("simple2", "html"));

    }

    void loadFiles()
    {
        WKPageLoadFile([webView pageRef], file1.get(), nullptr);
        TestWebKitAPI::Util::run(&didFinishNavigation);
        didFinishNavigation = false;

        WKPageLoadFile([webView pageRef], file2.get(), nullptr);
        TestWebKitAPI::Util::run(&didFinishNavigation);
        didFinishNavigation = false;
    }
};

#if WK_API_ENABLED

class WebKit2_CommandBackForwardTestWKWebView : public WebKit2_CommandBackForwardTest {
public:
    RetainPtr<WKWebView> webView;

    virtual void SetUp()
    {
        WebKit2_CommandBackForwardTest::SetUp();

        webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
        [[window contentView] addSubview:webView.get()];
    }
    
    void loadFiles()
    {
        NSURL *file1 = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
        [webView loadFileURL:file1 allowingReadAccessToURL:file1];
        [webView _test_waitForDidFinishNavigation];

        NSURL *file2 = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
        [webView loadFileURL:file2 allowingReadAccessToURL:file2];
        [webView _test_waitForDidFinishNavigation];
    }
};

TEST_F(WebKit2_CommandBackForwardTestWKWebView, LTR)
{
    EXPECT_EQ(NSUserInterfaceLayoutDirectionLeftToRight, [webView userInterfaceLayoutDirection]);
    
    loadFiles();

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple2.html");

    // Attempt to go back (using command-left).
    simulateCommandArrow(webView.get(), Left);
    [webView _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple.html");

    // Attempt to go back (using command-right).
    simulateCommandArrow(webView.get(), Right);
    [webView _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple2.html");
}

TEST_F(WebKit2_CommandBackForwardTestWKWebView, RTL)
{
    // Override the layout direction to be RTL.
    [webView setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionRightToLeft];
    
    loadFiles();

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple2.html");

    // Attempt to go back (using command-right)
    simulateCommandArrow(webView.get(), Right);
    [webView _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple.html");

    // Attempt to go back (using command-left).
    simulateCommandArrow(webView.get(), Left);
    [webView _test_waitForDidFinishNavigation];

    EXPECT_WK_STREQ([webView URL].path.lastPathComponent, @"simple2.html");
}

#endif

TEST_F(WebKit2_CommandBackForwardTestWKView, LTR)
{
    EXPECT_EQ(NSUserInterfaceLayoutDirectionLeftToRight, [webView userInterfaceLayoutDirection]);
    
    loadFiles();

    auto currentURL = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file2.get(), currentURL.get()));

    // Attempt to go back (using command-left).
    simulateCommandArrow(webView.get(), Left);
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    auto currentURL2 = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file1.get(), currentURL2.get()));

    // Attempt to go back (using command-right).
    simulateCommandArrow(webView.get(), Right);
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    auto currentURL3 = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file2.get(), currentURL3.get()));
}

TEST_F(WebKit2_CommandBackForwardTestWKView, RTL)
{
    // Override the layout direction to be RTL.
    [webView setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionRightToLeft];
    
    loadFiles();

    auto currentURL = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file2.get(), currentURL.get()));

    // Attempt to go back (using command-right)
    simulateCommandArrow(webView.get(), Right);
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    auto currentURL2 = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file1.get(), currentURL2.get()));

    // Attempt to go back (using command-left).
    simulateCommandArrow(webView.get(), Left);
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;

    auto currentURL3 = adoptWK(WKPageCopyActiveURL([webView pageRef]));
    EXPECT_TRUE(WKURLIsEqual(file2.get(), currentURL3.get()));
}

#endif // PLATFORM(MAC)

/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKBrowsingContextGroup.h>
#import <WebKit/WKBrowsingContextLoadDelegate.h>
#import <WebKit/WKProcessGroup.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKView.h>
#import <wtf/RetainPtr.h>

#import "PlatformUtilities.h"

#if PLATFORM(MAC)

namespace {

class WKBrowsingContextLoadDelegateTest : public ::testing::Test { 
public:
    RetainPtr<WKProcessGroup> processGroup;
    RetainPtr<WKBrowsingContextGroup> browsingContextGroup;
    RetainPtr<WKView> view;

    WKBrowsingContextLoadDelegateTest() = default;

    virtual void SetUp()
    {
        processGroup = adoptNS([[WKProcessGroup alloc] init]);
        browsingContextGroup = adoptNS([[WKBrowsingContextGroup alloc] initWithIdentifier:@"TestIdentifier"]);
        view = adoptNS([[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup.get() browsingContextGroup:browsingContextGroup.get()]);
    }

    virtual void TearDown()
    {
        view = nullptr;
        browsingContextGroup = nullptr;
        processGroup = nullptr;
    }
};

} // namespace

@interface SimpleLoadDelegate : NSObject <WKBrowsingContextLoadDelegate>
{
    bool* _simpleLoadDone;
}

- (id)initWithFlag:(bool*)flag;

@end

@implementation SimpleLoadDelegate

- (id)initWithFlag:(bool*)flag
{
    self = [super init];
    if (!self)
        return nil;

    _simpleLoadDone = flag;
    return self;
}

- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender
{
    *_simpleLoadDone = true;
}

@end

TEST_F(WKBrowsingContextLoadDelegateTest, Empty)
{
    // Just make sure the setup/tear down works.
}

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoad)
{
    bool simpleLoadDone = false;

    // Add the load delegate.
    auto loadDelegate = adoptNS([[SimpleLoadDelegate alloc] initWithFlag:&simpleLoadDone]);
    view.get().browsingContextController.loadDelegate = loadDelegate.get();

    // Load the file.
    NSURL *nsURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [view.get().browsingContextController loadFileURL:nsURL restrictToFilesWithin:nil];

    // Wait for the load to finish.
    TestWebKitAPI::Util::run(&simpleLoadDone);

    // Tear down the delegate.
    view.get().browsingContextController.loadDelegate = nil;
}

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoadOfHTMLString)
{
    bool simpleLoadDone = false;

    // Add the load delegate.
    auto loadDelegate = adoptNS([[SimpleLoadDelegate alloc] initWithFlag:&simpleLoadDone]);
    view.get().browsingContextController.loadDelegate = loadDelegate.get();

    // Load the HTML string.
    [view.get().browsingContextController loadHTMLString:@"<html><body>Simple HTML String</body></html>" baseURL:[NSURL URLWithString:@"about:blank"]];

    // Wait for the load to finish.
    TestWebKitAPI::Util::run(&simpleLoadDone);

    // Tear down the delegate.
    view.get().browsingContextController.loadDelegate = nil;
}

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoadOfHTMLString_NilBaseURL)
{
    bool simpleLoadDone = false;

    // Add the load delegate.
    auto loadDelegate = adoptNS([[SimpleLoadDelegate alloc] initWithFlag:&simpleLoadDone]);
    view.get().browsingContextController.loadDelegate = loadDelegate.get();

    // Load the HTML string, pass nil as the baseURL.
    [view.get().browsingContextController loadHTMLString:@"<html><body>Simple HTML String</body></html>" baseURL:nil];

    // Wait for the load to finish.
    TestWebKitAPI::Util::run(&simpleLoadDone);

    // Tear down the delegate.
    view.get().browsingContextController.loadDelegate = nil;
}

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoadOfHTMLString_NilHTMLStringAndBaseURL)
{
    bool simpleLoadDone = false;

    // Add the load delegate.
    auto loadDelegate = adoptNS([[SimpleLoadDelegate alloc] initWithFlag:&simpleLoadDone]);
    view.get().browsingContextController.loadDelegate = loadDelegate.get();

    // Load the HTML string (as nil).
    [view.get().browsingContextController loadHTMLString:nil baseURL:nil];

    // Wait for the load to finish.
    TestWebKitAPI::Util::run(&simpleLoadDone);

    // Tear down the delegate.
    view.get().browsingContextController.loadDelegate = nil;
}

@interface SimpleLoadFailDelegate : NSObject <WKBrowsingContextLoadDelegate>
{
    bool* _simpleLoadFailDone;
}

- (id)initWithFlag:(bool*)flag;

@end

@implementation SimpleLoadFailDelegate

- (id)initWithFlag:(bool*)flag
{
    self = [super init];
    if (!self)
        return nil;

    _simpleLoadFailDone = flag;
    return self;
}

- (void)browsingContextController:(WKBrowsingContextController *)sender didFailProvisionalLoadWithError:(NSError *)error
{
    EXPECT_EQ(-1100, error.code);
    EXPECT_WK_STREQ(NSURLErrorDomain, error.domain);
    
    *_simpleLoadFailDone = true;
}

@end

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoadFail)
{
    bool simpleLoadFailDone = false;

    // Add the load delegate.
    auto loadDelegate = adoptNS([[SimpleLoadFailDelegate alloc] initWithFlag:&simpleLoadFailDone]);
    view.get().browsingContextController.loadDelegate = loadDelegate.get();

    // Load a non-existent file.
    NSURL *nsURL = [NSURL URLWithString:@"file:///does-not-exist.html"];
    [view.get().browsingContextController loadFileURL:nsURL restrictToFilesWithin:nil];

    // Wait for the load to fail.
    TestWebKitAPI::Util::run(&simpleLoadFailDone);

    // Tear down the delegate.
    view.get().browsingContextController.loadDelegate = nil;
}

#endif // PLATFORM(MAC)

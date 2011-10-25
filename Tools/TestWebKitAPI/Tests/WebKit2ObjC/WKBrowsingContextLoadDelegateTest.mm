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

#import <WebKit2/WKBrowsingContextController.h>
#import <WebKit2/WKBrowsingContextGroup.h>
#import <WebKit2/WKBrowsingContextLoadDelegate.h>
#import <WebKit2/WKProcessGroup.h>
#import <WebKit2/WKRetainPtr.h>
#import <WebKit2/WKView.h>

#import "PlatformUtilities.h"

class WKBrowsingContextLoadDelegateTest : public ::testing::Test { 
public:
    WKProcessGroup *processGroup;
    WKBrowsingContextGroup *browsingContextGroup;
    WKView *view;

    WKBrowsingContextLoadDelegateTest()
        : processGroup(nil)
        , browsingContextGroup(nil)
        , view(nil)
    {
    }

    virtual void SetUp()
    {
        processGroup = [[WKProcessGroup alloc] init];
        browsingContextGroup = [[WKBrowsingContextGroup alloc] initWithIdentifier:@"TestIdentifier"];
        view = [[WKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) processGroup:processGroup browsingContextGroup:browsingContextGroup];
    }

    virtual void TearDown()
    {
        [view release];
        [browsingContextGroup release];
        [processGroup release];
    }
};

static bool simpleLoadDone;

@interface SimpleLoadDelegate : NSObject <WKBrowsingContextLoadDelegate>
@end

@implementation SimpleLoadDelegate

- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender
{
    simpleLoadDone = true;
}

@end

TEST_F(WKBrowsingContextLoadDelegateTest, Empty)
{
    // Just make sure the setup/tear down works.
}

TEST_F(WKBrowsingContextLoadDelegateTest, SimpleLoad)
{
    // Add the load delegate.
    SimpleLoadDelegate *simpleLoadDelegate = [[SimpleLoadDelegate alloc] init];
    view.browsingContextController.loadDelegate = simpleLoadDelegate;

    // Load the file.
    NSURL *nsURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html"];
    [view.browsingContextController loadFileURL:nsURL restrictToFilesWithin:nil];

    // Wait for the load.
    TestWebKitAPI::Util::run(&simpleLoadDone);

    // Tear down the delegate.
    view.browsingContextController.loadDelegate = nil;
    [simpleLoadDelegate release];
}

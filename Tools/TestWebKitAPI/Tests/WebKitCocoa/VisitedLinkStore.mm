/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import <WebKit/_WKVisitedLinkStore.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit, VisitedLinkStore_Add)
{
    auto visitedLinkStore = adoptNS([[_WKVisitedLinkStore alloc] init]);
    NSURL *appleURL = [NSURL URLWithString:@"https://www.apple.com"];
    NSURL *webkitURL = [NSURL URLWithString:@"https://www.webkit.org"];
    NSURL *bugzillaURL = [NSURL URLWithString:@"https://bugs.webkit.org"];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithURL:webkitURL];
    [visitedLinkStore addVisitedLinkWithURL:bugzillaURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
}

TEST(WebKit, VisitedLinkStore_RemoveAll)
{
    auto visitedLinkStore = adoptNS([[_WKVisitedLinkStore alloc] init]);
    NSURL *appleURL = [NSURL URLWithString:@"https://www.apple.com"];
    NSURL *webkitURL = [NSURL URLWithString:@"https://www.webkit.org"];
    NSURL *bugzillaURL = [NSURL URLWithString:@"https://bugs.webkit.org"];
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithURL:webkitURL];
    [visitedLinkStore addVisitedLinkWithURL:bugzillaURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    [visitedLinkStore removeAll];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
}

TEST(WebKit, VisitedLinkStore_Remove)
{
    auto visitedLinkStore = adoptNS([[_WKVisitedLinkStore alloc] init]);
    NSURL *appleURL = [NSURL URLWithString:@"https://www.apple.com"];
    NSURL *webkitURL = [NSURL URLWithString:@"https://www.webkit.org"];
    NSURL *bugzillaURL = [NSURL URLWithString:@"https://bugs.webkit.org"];
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithURL:webkitURL];
    [visitedLinkStore addVisitedLinkWithURL:bugzillaURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    [visitedLinkStore removeVisitedLinkWithURL:appleURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    [visitedLinkStore removeVisitedLinkWithURL:webkitURL];
    [visitedLinkStore removeVisitedLinkWithURL:bugzillaURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    [visitedLinkStore removeVisitedLinkWithURL:appleURL];
    [visitedLinkStore removeVisitedLinkWithURL:appleURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
}

TEST(WebKit, VisitedLinkStore_AddAndRemove)
{
    auto visitedLinkStore = adoptNS([[_WKVisitedLinkStore alloc] init]);
    NSURL *appleURL = [NSURL URLWithString:@"https://www.apple.com"];
    NSURL *webkitURL = [NSURL URLWithString:@"https://www.webkit.org"];
    NSURL *bugzillaURL = [NSURL URLWithString:@"https://bugs.webkit.org"];
    NSURL *tracURL = [NSURL URLWithString:@"https://trac.webkit.org"];
    NSString *tracString = @"https://trac.webkit.org";
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithString:tracString];
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:tracURL]);
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore removeVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithURL:webkitURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:tracURL]);
    [visitedLinkStore removeVisitedLinkWithURL:appleURL];
    [visitedLinkStore removeVisitedLinkWithURL:tracURL];
    [visitedLinkStore addVisitedLinkWithURL:appleURL];
    [visitedLinkStore addVisitedLinkWithURL:bugzillaURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:tracURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:appleURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:webkitURL]);
    EXPECT_TRUE([visitedLinkStore containsVisitedLinkWithURL:bugzillaURL]);
    [visitedLinkStore removeVisitedLinkWithURL:tracURL];
    EXPECT_FALSE([visitedLinkStore containsVisitedLinkWithURL:tracURL]);
}

}

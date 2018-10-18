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
#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED && !PLATFORM(IOS_FAMILY_SIMULATOR)

#import "AdditionalReadAccessAllowedURLsProtocol.h"
#import "PlatformUtilities.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool done;

TEST(WebKit, AdditionalReadAccessAllowedURLs)
{
    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"AdditionalReadAccessAllowedURLsPlugIn"]);

    _WKProcessPoolConfiguration *processPoolConfiguration = [configuration processPool]._configuration;

    bool exceptionRaised = false;
    @try {
        processPoolConfiguration.additionalReadAccessAllowedURLs = @[ [NSURL URLWithString:@"about:blank"] ];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);

    NSURL *fileURLWithNonLatin1Path = [NSURL fileURLWithPath:@"/这是中文"];
    processPoolConfiguration.additionalReadAccessAllowedURLs = @[ fileURLWithNonLatin1Path ];
    EXPECT_TRUE([processPoolConfiguration.additionalReadAccessAllowedURLs.firstObject isEqual:fileURLWithNonLatin1Path]);

    char temporaryDirectory[PATH_MAX];
    confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory));

    char readableDirectory[PATH_MAX];
    strlcpy(readableDirectory, [[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] stringByAppendingPathComponent:@"WebKitTestRunner.AdditionalReadAccessAllowedURLs-XXXXXX"].fileSystemRepresentation, sizeof(temporaryDirectory));
    mkdtemp(readableDirectory);
    NSURL *readableDirectoryURL = [NSURL fileURLWithFileSystemRepresentation:readableDirectory isDirectory:YES relativeToURL:nil];

    char unreadableDirectory[PATH_MAX];
    strlcpy(unreadableDirectory, [[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] stringByAppendingPathComponent:@"WebKitTestRunner.AdditionalReadAccessAllowedURLs-XXXXXX"].fileSystemRepresentation, sizeof(temporaryDirectory));
    mkdtemp(unreadableDirectory);
    NSURL *unreadableDirectoryURL = [NSURL fileURLWithFileSystemRepresentation:unreadableDirectory isDirectory:YES relativeToURL:nil];

    processPoolConfiguration.additionalReadAccessAllowedURLs = @[ readableDirectoryURL ];

    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration]);
    [processPool _setObject:@"AdditionalReadAccessAllowedURLsPlugIn" forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    [configuration setProcessPool:processPool.get()];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    id<AdditionalReadAccessAllowedURLsProtocol> proxy = [[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:[_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(AdditionalReadAccessAllowedURLsProtocol)]];

    NSURL *readableFileURL = [readableDirectoryURL URLByAppendingPathComponent:@"file"];
    NSURL *unreadableFileURL = [unreadableDirectoryURL URLByAppendingPathComponent:@"file"];

    [@"hello" writeToURL:readableFileURL atomically:YES encoding:NSUTF8StringEncoding error:nullptr];
    [@"secret" writeToURL:unreadableFileURL atomically:YES encoding:NSUTF8StringEncoding error:nullptr];

    [proxy readStringFromURL:readableFileURL completionHandler:^(NSString *string, NSError *error) {
        done = true;
        EXPECT_WK_STREQ(@"hello", string);
        EXPECT_EQ(nullptr, error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [proxy readStringFromURL:unreadableFileURL completionHandler:^(NSString *string, NSError *error) {
        done = true;
        EXPECT_EQ(nullptr, string);
        EXPECT_WK_STREQ(NSCocoaErrorDomain, error.domain);
        EXPECT_EQ(NSFileReadNoPermissionError, error.code);
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif

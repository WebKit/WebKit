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

#if !PLATFORM(IOS_FAMILY_SIMULATOR)

#import "AdditionalReadAccessAllowedURLsProtocol.h"
#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/NSAttributedStringPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static std::pair<RetainPtr<NSURL>, RetainPtr<NSURL>> readableAndUnreadableDirectories()
{
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

    return std::make_pair(readableDirectoryURL, unreadableDirectoryURL);
}

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

    auto [readableDirectoryURL, unreadableDirectoryURL] = readableAndUnreadableDirectories();

    processPoolConfiguration.additionalReadAccessAllowedURLs = @[ readableDirectoryURL.get() ];

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

TEST(WebKit, NSAttributedStringWithReadOnlyPaths)
{
    __block bool done = false;

    auto [readableDirectoryURL, unreadableDirectoryURL] = readableAndUnreadableDirectories();

    NSURL *iconImagePath = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *readableFileURL = [readableDirectoryURL URLByAppendingPathComponent:@"readable.png"];
    NSError *error;
    if (![NSFileManager.defaultManager copyItemAtURL:iconImagePath toURL:readableFileURL error:&error])
        EXPECT_TRUE(error.code == NSFileWriteFileExistsError);

    NSURL *redImagePath = [[NSBundle mainBundle] URLForResource:@"large-red-square" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *unreadableFileURL = [unreadableDirectoryURL URLByAppendingPathComponent:@"unreadable.png"];
    if (![NSFileManager.defaultManager copyItemAtURL:redImagePath toURL:unreadableFileURL error:&error])
        EXPECT_TRUE(error.code == NSFileWriteFileExistsError);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    auto options = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:@[ readableDirectoryURL.get() ], _WKReadAccessFileURLsOption, nil]);
#pragma clang diagnostic pop

    NSString *testString = [NSString stringWithFormat:@"<p>Hello<img src='%@'></p><p>World<img src='%@'></p>", [readableFileURL absoluteString], [unreadableFileURL absoluteString]];

    [NSAttributedString loadFromHTMLWithString:testString options:options.get() completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {

        __block Vector<NSTextAttachment *> attachments;
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
            id attachment = [attrs objectForKey:NSAttachmentAttributeName];
            if (attachment)
                attachments.append(attachment);
        }];
            
        EXPECT_EQ(attachments.size(), 2ul);

        // Sandbox allows access, so get reference to image path:
        EXPECT_TRUE(attachments[0]);
        EXPECT_TRUE([attachments[0].fileType isEqualToString:@"public.png"]);
        EXPECT_TRUE([attachments[0].fileWrapper.preferredFilename isEqualToString:@"readable.png"]);
        EXPECT_NULL(attachments[0].image); // Image is to be loaded from the path

        // Sandbox prohibits access, so get placeholder image:
        EXPECT_TRUE(attachments[1]);
        EXPECT_NULL(attachments[1].fileType);
        EXPECT_TRUE([attachments[1].fileWrapper.preferredFilename isEqualToString:@"Attachment.png"]); // Placeholder attachment image
        EXPECT_TRUE(attachments[1].image); // Placeholder image

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, NSAttributedStringWithAndWithoutReadOnlyPaths)
{
    __block bool done = false;

    auto [readableDirectoryURL, unreadableDirectoryURL] = readableAndUnreadableDirectories();

    NSURL *iconImagePath = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *readableFileURL = [readableDirectoryURL URLByAppendingPathComponent:@"readable.png"];
    NSError *error;
    if (![NSFileManager.defaultManager copyItemAtURL:iconImagePath toURL:readableFileURL error:&error])
        EXPECT_TRUE(error.code == NSFileWriteFileExistsError);

    NSURL *redImagePath = [[NSBundle mainBundle] URLForResource:@"large-red-square" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *unreadableFileURL = [unreadableDirectoryURL URLByAppendingPathComponent:@"unreadable.png"];
    if (![NSFileManager.defaultManager copyItemAtURL:redImagePath toURL:unreadableFileURL error:&error])
        EXPECT_TRUE(error.code == NSFileWriteFileExistsError);
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    auto options = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:@[ readableDirectoryURL.get() ], _WKReadAccessFileURLsOption, nil]);
#pragma clang diagnostic pop

    NSString *testString = [NSString stringWithFormat:@"<p>Hello<img src='%@'></p><p>World<img src='%@'></p>", [readableFileURL absoluteString], [unreadableFileURL absoluteString]];

    [NSAttributedString loadFromHTMLWithString:testString options:options.get() completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {

        __block Vector<NSTextAttachment *> attachments;
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
            id attachment = [attrs objectForKey:NSAttachmentAttributeName];
            if (attachment)
                attachments.append(attachment);
        }];
            
        EXPECT_EQ(attachments.size(), 2ul);

        // Sandbox allows access, so get reference to image path:
        EXPECT_TRUE(attachments[0]);
        EXPECT_TRUE([attachments[0].fileType isEqualToString:@"public.png"]);
        EXPECT_TRUE([attachments[0].fileWrapper.preferredFilename isEqualToString:@"readable.png"]);
        EXPECT_NULL(attachments[0].image); // Image is to be loaded from the path

        // Sandbox prohibits access, so get placeholder image:
        EXPECT_TRUE(attachments[1]);
        EXPECT_NULL(attachments[1].fileType);
        EXPECT_TRUE([attachments[1].fileWrapper.preferredFilename isEqualToString:@"Attachment.png"]); // Placeholder attachment image
        EXPECT_TRUE(attachments[1].image); // Placeholder image

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [NSAttributedString loadFromHTMLWithString:testString options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {

        __block Vector<NSTextAttachment *> attachments;
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
            if (id attachment = [attrs objectForKey:NSAttachmentAttributeName])
                attachments.append(attachment);
        }];

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, NSAttributedStringWithoutReadOnlyPaths)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"Hello World" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);
        
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    done = false;
    [NSAttributedString loadFromHTMLWithString:@"Hello Again!" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, NSAttributedStringWithTooManyReadOnlyPaths)
{
    __block bool done = false;

    auto [readableDirectoryURL, unreadableDirectoryURL] = readableAndUnreadableDirectories();
    NSURL *bundlePathURL = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    auto options = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:@[ readableDirectoryURL.get(), unreadableDirectoryURL.get(), bundlePathURL ], _WKReadAccessFileURLsOption, nil]);
#pragma clang diagnostic pop

    bool exceptionRaised = false;
    @try {
        [NSAttributedString loadFromHTMLWithString:@"Hello World" options:options.get() completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);
}

TEST(WebKit, NSAttributedStringWithInvalidReadOnlyPaths)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    auto options = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:@[ @"/some/random/path" ], _WKReadAccessFileURLsOption, nil]);
#pragma clang diagnostic pop

    bool exceptionRaised = false;
    @try {
        [NSAttributedString loadFromHTMLWithString:@"Hello World" options:options.get() completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);

    done = false;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    NSURL* testURL = [NSURL URLWithString:@"https://example.com"];
    auto options2 = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:@[ testURL ], _WKReadAccessFileURLsOption, nil]);
#pragma clang diagnostic pop
    exceptionRaised = false;
    @try {
        [NSAttributedString loadFromHTMLWithString:@"Hello World" options:options2.get() completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        exceptionRaised = true;
    }
    EXPECT_TRUE(exceptionRaised);
}

#endif

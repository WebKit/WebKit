/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/FileHandle.h>
#import <wtf/FileSystem.h>

#define HTML_FORMAT_STRING @" \
    <body> \
        <script> \
        fetch('%s').then(v => v.arrayBuffer()).then(txt => { \
            window.local_file_content = (btoa(String.fromCharCode.apply(null, new Uint8Array(txt)))); \
        }) \
    </script> \
    </body>"

#if ENABLE(BLOCKING_OF_LOCAL_FILE_LOADS_WITHOUT_SANDBOX_EXTENSION) && !PLATFORM(IOS_SIMULATOR)
TEST(WebKit, FetchLocalFile)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr controller = adoptNS([[WKUserContentController alloc] init]);
    configuration.get().userContentController = controller.get();
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto [tempFileHandle, tempFilePath] = FileSystem::createTemporaryFileInDirectory("/tmp"_s, ".html"_s);
    ASCIILiteral fileData = "Testdata"_s;
    auto fileDataSpan = fileData.span8();
    tempFileHandle.write(fileDataSpan);
    tempFileHandle = { };

    URL fileURL = URL::fileURLWithFileSystemPath(tempFilePath.span());
    RetainPtr payload = adoptNS([[NSString alloc] initWithFormat:HTML_FORMAT_STRING, fileURL.string().utf8().data()]);

    auto [fetchFilePath, fetchFileHandle] = FileSystem::openTemporaryFile("fetch"_s, ".html"_s);
    fetchFileHandle.write(String(payload.get()).span8());
    fetchFileHandle = { };
    URL fetchFileURL = URL::fileURLWithFileSystemPath(fetchFilePath);

    RetainPtr request = [NSURLRequest requestWithURL:fetchFileURL.createNSURL().get()];

    [webView synchronouslyLoadRequest:request.get()];

    __block bool done = false;
    [webView evaluateJavaScript:@"window.local_file_content" completionHandler:^(id result, NSError *err) {
        EXPECT_TRUE(result == nil && err);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    FileSystem::deleteFile(tempFilePath.span());
    FileSystem::deleteFile(fetchFilePath);
}
#endif // ENABLE(BLOCKING_OF_LOCAL_FILE_LOADS_WITHOUT_SANDBOX_EXTENSION) && !PLATFORM(IOS_SIMULATOR)

TEST(WebKit, FetchLocalFileFromTempDirectory)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr controller = adoptNS([[WKUserContentController alloc] init]);
    configuration.get().userContentController = controller.get();
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto [tempFilePath, tempFileHandle] = FileSystem::openTemporaryFile("Temp"_s, ".html"_s);
    ASCIILiteral fileData = "Testdata"_s;
    auto fileDataSpan = fileData.span8();
    tempFileHandle.write(fileDataSpan);
    tempFileHandle = { };

    URL fileURL = URL::fileURLWithFileSystemPath(tempFilePath);
    RetainPtr payload = adoptNS([[NSString alloc] initWithFormat:HTML_FORMAT_STRING, fileURL.string().utf8().data()]);

    auto [fetchFilePath, fetchFileHandle] = FileSystem::openTemporaryFile("fetch"_s, ".html"_s);
    fetchFileHandle.write(String(payload.get()).span8());
    fetchFileHandle = { };
    URL fetchFileURL = URL::fileURLWithFileSystemPath(fetchFilePath);

    RetainPtr request = [NSURLRequest requestWithURL:fetchFileURL.createNSURL().get()];

    [webView synchronouslyLoadRequest:request.get()];

    __block bool done = false;
    [webView evaluateJavaScript:@"window.local_file_content" completionHandler:^(id result, NSError *err) {
        EXPECT_TRUE(result != nil && !err);

        RetainPtr decodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:result options:0]);

        EXPECT_GT([decodedData length], 0u);

        RetainPtr expectedData = [NSData dataWithBytes:fileDataSpan.data() length:fileDataSpan.size()];
        EXPECT_TRUE([decodedData isEqualToData:expectedData.get()]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    FileSystem::deleteFile(tempFilePath);
    FileSystem::deleteFile(fetchFilePath);
}

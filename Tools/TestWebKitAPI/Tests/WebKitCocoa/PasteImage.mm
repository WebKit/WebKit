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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

#if PLATFORM(MAC)
void writeImageDataToPasteboard(NSString *type, NSData *data)
{
    [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:type] owner:nil];
    [[NSPasteboard generalPasteboard] setData:data forType:type];
}
#else
void writeImageDataToPasteboard(NSString *type, NSData *data)
{
    [[UIPasteboard generalPasteboard] setItems:@[[NSDictionary dictionaryWithObject:data forKey:type]]];
}
#endif

@interface TestWKWebView (PasteImage)
- (void)waitForMessage:(NSString *)message afterEvaluatingScript:(NSString *)script;
@end

@implementation TestWKWebView (PasteImage)

- (void)waitForMessage:(NSString *)message afterEvaluatingScript:(NSString *)script
{
    __block bool evaluatedScript = false;
    __block bool receivedMessage = false;
    [self performAfterReceivingMessage:message action:^{
        receivedMessage = true;
    }];
    [self evaluateJavaScript:script completionHandler:^(id, NSError *) {
        evaluatedScript = true;
    }];
    TestWebKitAPI::Util::run(&evaluatedScript);
    TestWebKitAPI::Util::run(&receivedMessage);
}

@end

TEST(PasteImage, PasteGIFImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-400px" ofType:@"gif" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard((__bridge NSString *)kUTTypeGIF, data);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/gif').toString()"]);
    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/tiff').toString()"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"gifItem = dataTransfer.items.find((item) => item.type == 'image/gif'); (!!gifItem).toString()"]);
    EXPECT_WK_STREQ("file", [webView stringByEvaluatingJavaScript:@"gifItem.kind"]);
    EXPECT_WK_STREQ("image/gif", [webView stringByEvaluatingJavaScript:@"gifItem.file.type"]);
    EXPECT_WK_STREQ("image.gif", [webView stringByEvaluatingJavaScript:@"gifItem.file.name"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.includes(gifItem.file).toString()"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(gifItem.file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("400", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PasteJPEGImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-600px" ofType:@"jpg" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard((__bridge NSString *)kUTTypeJPEG, data);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/gif').toString()"]);
    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/tiff').toString()"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"jpegItem = dataTransfer.items.find((item) => item.type == 'image/jpeg'); (!!jpegItem).toString()"]);
    EXPECT_WK_STREQ("file", [webView stringByEvaluatingJavaScript:@"jpegItem.kind"]);
    EXPECT_WK_STREQ("image/jpeg", [webView stringByEvaluatingJavaScript:@"jpegItem.file.type"]);
    EXPECT_WK_STREQ("image.jpeg", [webView stringByEvaluatingJavaScript:@"jpegItem.file.name"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.includes(jpegItem.file).toString()"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(jpegItem.file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("600", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PastePNGImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard((__bridge NSString *)kUTTypePNG, data);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/gif').toString()"]);
    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/tiff').toString()"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"pngItem = dataTransfer.items.find((item) => item.type == 'image/png'); (!!pngItem).toString()"]);
    EXPECT_WK_STREQ("file", [webView stringByEvaluatingJavaScript:@"pngItem.kind"]);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"pngItem.file.type"]);
    EXPECT_WK_STREQ("image.png", [webView stringByEvaluatingJavaScript:@"pngItem.file.name"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.includes(pngItem.file).toString()"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(pngItem.file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("200", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

#if PLATFORM(MAC)
void writeBundleFileToPasteboard(id object)
{
    [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
    [[NSPasteboard generalPasteboard] writeObjects:[NSArray arrayWithObjects:object, nil]];
}

TEST(PasteImage, PasteGIFFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"sunset-in-cupertino-400px" withExtension:@"gif" subdirectory:@"TestWebKitAPI.resources"];
    writeBundleFileToPasteboard(url);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/png').toString()"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.items.filter((item) => item.kind == 'file').length"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.length"]);
    EXPECT_WK_STREQ("image/gif", [webView stringByEvaluatingJavaScript:@"file = dataTransfer.files[0]; file.type"]);
    EXPECT_WK_STREQ("sunset-in-cupertino-400px.gif", [webView stringByEvaluatingJavaScript:@"file.name"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("400", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PasteJPEGFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"sunset-in-cupertino-600px" withExtension:@"jpg" subdirectory:@"TestWebKitAPI.resources"];
    writeBundleFileToPasteboard(url);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/jpeg').toString()"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.items.filter((item) => item.kind == 'file').length"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.length"]);
    EXPECT_WK_STREQ("image/jpeg", [webView stringByEvaluatingJavaScript:@"file = dataTransfer.files[0]; file.type"]);
    EXPECT_WK_STREQ("sunset-in-cupertino-600px.jpg", [webView stringByEvaluatingJavaScript:@"file.name"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("600", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PastePNGFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"sunset-in-cupertino-200px" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    writeBundleFileToPasteboard(url);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/png').toString()"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.items.filter((item) => item.kind == 'file').length"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.length"]);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"file = dataTransfer.files[0]; file.type"]);
    EXPECT_WK_STREQ("sunset-in-cupertino-200px.png", [webView stringByEvaluatingJavaScript:@"file.name"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("200", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PasteTIFFFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff" subdirectory:@"TestWebKitAPI.resources"];
    writeBundleFileToPasteboard(url);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/tiff').toString()"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.items.filter((item) => item.kind == 'file').length"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.length"]);
    EXPECT_WK_STREQ("image/tiff", [webView stringByEvaluatingJavaScript:@"file = dataTransfer.files[0]; file.type"]);
    EXPECT_WK_STREQ("sunset-in-cupertino-100px.tiff", [webView stringByEvaluatingJavaScript:@"file.name"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("100", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PasteLegacyTIFFImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-100px" ofType:@"tiff" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard(NSTIFFPboardType, data);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/png').toString()"]);
    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/tiff').toString()"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.items.filter((item) => item.kind == 'file').length"]);
    EXPECT_WK_STREQ("1", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.length"]);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"file = dataTransfer.files[0]; file.type"]);
    EXPECT_WK_STREQ("image.png", [webView stringByEvaluatingJavaScript:@"file.name"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("100", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}

TEST(PasteImage, PasteTIFFImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"paste-image"];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-100px" ofType:@"tiff" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard((__bridge NSString *)kUTTypeTIFF, data);
    [webView paste:nil];

    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/gif').toString()"]);
    EXPECT_WK_STREQ("false", [webView stringByEvaluatingJavaScript:@"dataTransfer.types.includes('image/png').toString()"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"pngItem = dataTransfer.items.find((item) => item.type == 'image/png'); (!!pngItem).toString()"]);
    EXPECT_WK_STREQ("file", [webView stringByEvaluatingJavaScript:@"pngItem.kind"]);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"pngItem.file.type"]);
    EXPECT_WK_STREQ("image.png", [webView stringByEvaluatingJavaScript:@"pngItem.file.name"]);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"dataTransfer.files.includes(pngItem.file).toString()"]);

    [webView waitForMessage:@"loaded" afterEvaluatingScript:@"insertFileAsImage(pngItem.file)"];
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src); url.protocol"]);
    EXPECT_WK_STREQ("100", [webView stringByEvaluatingJavaScript:@"imageElement.width"]);
}
#endif

#endif // WK_API_ENABLED && PLATFORM(MAC)



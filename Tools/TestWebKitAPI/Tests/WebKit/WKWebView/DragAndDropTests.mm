/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)

#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "InstanceMethodSwizzler.h"
#import "TestURLSchemeHandler.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WebArchive.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

@implementation TestWKWebView (DragAndDropTests)

- (void)selectElementWithID:(NSString *)elementID
{
    [self evaluateJavaScript:[NSString stringWithFormat:@"getSelection().selectAllChildren(document.getElementById('%@'))", elementID] completionHandler:nil];
    [self waitForNextPresentationUpdate];
}

@end

@implementation DragAndDropSimulator (DragAndDropTests)

- (void)dragFromElementWithID:(NSString *)elementID to:(CGPoint)destination
{
    NSArray<NSNumber *> *rectValues = [self.webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => {"
        "    const element = document.getElementById('%@');"
        "    const bounds = element.getBoundingClientRect();"
        "    return [bounds.left, bounds.top, bounds.width, bounds.height];"
        "})();", elementID]];

    auto bounds = CGRectMake(rectValues[0].floatValue, rectValues[1].floatValue, rectValues[2].floatValue, rectValues[3].floatValue);
    auto midPoint = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
    [self runFrom:midPoint to:destination];
}

@end

TEST(DragAndDropTests, ModernWebArchiveType)
{
    NSData *markupData = [@"<strong><i>Hello world</i></strong>" dataUsingEncoding:NSUTF8StringEncoding];
    RetainPtr mainResource = adoptNS([[WebResource alloc] initWithData:markupData URL:[NSURL URLWithString:@"foo.html"] MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    RetainPtr archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:@[ ] subframeArchives:@[ ]]);
    NSString *webArchiveType = UTTypeWebArchive.identifier;

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body style='width: 100%; height: 100%;' contenteditable>"];
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[webArchiveType, UTTypeUTF8PlainText.identifier] owner:nil];
    [pasteboard setData:[archive data] forType:webArchiveType];
    [pasteboard setData:[@"Hello world" dataUsingEncoding:NSUTF8StringEncoding] forType:UTTypeUTF8PlainText.identifier];
    [simulator setExternalDragPasteboard:pasteboard];
#else
    RetainPtr item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:webArchiveType visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([archive data], nil);
        return nil;
    }];
    [item registerDataRepresentationForTypeIdentifier:UTTypeUTF8PlainText.identifier visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];
    [simulator setExternalItemProviders:@[ item.get() ]];
#endif
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    [webView stringByEvaluatingJavaScript:@"document.body.focus(); getSelection().setBaseAndExtent(document.body, 0, document.body, 1)"];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('italic')"].boolValue);
}

TEST(DragAndDropTests, DragImageLocationForLinkInSubframe)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 400, 400)]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"link-in-iframe-and-input"];
    [simulator runFrom:CGPointMake(200, 375) to:CGPointMake(200, 125)];

    EXPECT_WK_STREQ("https://www.apple.com/", [[simulator webView] stringByEvaluatingJavaScript:@"document.querySelector('input').value"]);

#if PLATFORM(MAC)
    EXPECT_TRUE(NSPointInRect([simulator initialDragImageLocationInView], NSMakeRect(0, 250, 400, 250)));
#endif
}

#if PLATFORM(MAC)

// 200x100 with four vertical 50px stripes: red, blue, green, yellow.
static NSString *const stripedImageURL = @"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMgAAABkCAIAAABM5OhcAAABRElEQVR42u3SMQ0AAAzDsPInvdHIYSkAcni3BUtOXXQq2cACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACCyywwAILLLDAAgsssMACqw3rAVsaTAhYVBpUAAAAAElFTkSuQmCC";

enum class DragImageColor : uint8_t { Transparent, Red, Blue, Green, Yellow, Other };

static RetainPtr<NSBitmapImageRep> dragImageBitmapForImage(NSString *source, NSString *style)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 400, 400)]);
    [[simulator webView] synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<body style='margin: 0'><img style='display: block; %@' src='%@'></body>", style, source]];
    [simulator runFrom:CGPointMake(150, 250) to:CGPointMake(350, 50)];

    NSImage *dragImage = [simulator draggingInfo].draggedImage;
    if (!dragImage)
        return nil;
    return adoptNS([[NSBitmapImageRep alloc] initWithCGImage:[dragImage CGImageForProposedRect:nil context:nil hints:nil]]);
}

static RetainPtr<NSBitmapImageRep> dragImageBitmapForStripedImageWithStyle(NSString *style)
{
    return dragImageBitmapForImage(stripedImageURL, [NSString stringWithFormat:@"width: 300px; height: 300px; %@", style]);
}

static DragImageColor dragImageColorAt(NSBitmapImageRep *bitmap, CGFloat x, CGFloat y)
{
    NSColor *color = [[bitmap colorAtX:x * bitmap.pixelsWide y:y * bitmap.pixelsHigh] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (color.alphaComponent < 0.1)
        return DragImageColor::Transparent;

    bool red = color.redComponent > 0.5;
    bool green = color.greenComponent > 0.5;
    bool blue = color.blueComponent > 0.5;
    if (red && green && !blue)
        return DragImageColor::Yellow;
    if (red && !green && !blue)
        return DragImageColor::Red;
    if (!red && green && !blue)
        return DragImageColor::Green;
    if (!red && !green && blue)
        return DragImageColor::Blue;
    return DragImageColor::Other;
}

TEST(DragAndDropTests, DragImageForObjectFitFill)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.1));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.9, 0.9));
}

TEST(DragAndDropTests, DragImageForObjectFitContain)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"object-fit: contain");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.1));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.5));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.9, 0.5));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.9));
}

TEST(DragAndDropTests, DragImageForObjectFitCover)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"object-fit: cover");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Blue, dragImageColorAt(bitmap.get(), 0.1, 0.1));
    EXPECT_EQ(DragImageColor::Green, dragImageColorAt(bitmap.get(), 0.9, 0.9));
}

TEST(DragAndDropTests, DragImageForObjectFitNone)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"object-fit: none");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.1, 0.5));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.2, 0.5));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.8, 0.5));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.1));
}

TEST(DragAndDropTests, DragImageForObjectPosition)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"object-fit: contain; object-position: right bottom");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.25));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.75));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.9, 0.75));
}

// The same four stripes as stripedImageURL but drawn as SVG image.
static NSString *stripedSVGImageURL(NSString *rootAttributes)
{
    NSString *svg = [NSString stringWithFormat:@"<svg xmlns='http://www.w3.org/2000/svg' %@><rect width='50' height='100' fill='#f00'/><rect x='50' width='50' height='100' fill='#00f'/><rect x='100' width='50' height='100' fill='#0f0'/><rect x='150' width='50' height='100' fill='#ff0'/></svg>", rootAttributes];
    return [@"data:image/svg+xml;base64," stringByAppendingString:[[svg dataUsingEncoding:NSUTF8StringEncoding] base64EncodedStringWithOptions:0]];
}

TEST(DragAndDropTests, DragImageForSVGImageWithNaturalSize)
{
    RetainPtr bitmap = dragImageBitmapForImage(stripedSVGImageURL(@"width='200' height='100'"), @"width: 300px; height: 300px; object-fit: contain");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.1));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.5));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.9, 0.5));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.9));
}

TEST(DragAndDropTests, DragImageForSVGImageWithOnlyViewBox)
{
    RetainPtr bitmap = dragImageBitmapForImage(stripedSVGImageURL(@"viewBox='0 0 200 100'"), @"width: 300px; height: 300px");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.1));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.5));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.9, 0.5));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.5, 0.9));
}

TEST(DragAndDropTests, DragImageForSVGImageWithoutNaturalSize)
{
    RetainPtr bitmap = dragImageBitmapForImage(stripedSVGImageURL(@""), @"width: 300px; height: 300px");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.1));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.6, 0.1));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.9, 0.1));
    EXPECT_EQ(DragImageColor::Transparent, dragImageColorAt(bitmap.get(), 0.1, 0.5));
}

TEST(DragAndDropTests, DragImageForObjectViewBox)
{
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"object-view-box: inset(0 50% 0 0)");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.1, 0.5));
    EXPECT_EQ(DragImageColor::Blue, dragImageColorAt(bitmap.get(), 0.9, 0.5));
}

TEST(DragAndDropTests, DragImageForImageOrientation)
{
    RetainPtr bitmap = dragImageBitmapForImage(@"exif-orientation-8-llo.jpg", @"width: 160px; height: 320px");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(2 * bitmap.get().pixelsWide, bitmap.get().pixelsHigh);
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap.get(), 0.1, 0.1));
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap.get(), 0.9, 0.1));
    EXPECT_EQ(DragImageColor::Blue, dragImageColorAt(bitmap.get(), 0.9, 0.9));
    EXPECT_EQ(DragImageColor::Green, dragImageColorAt(bitmap.get(), 0.1, 0.9));
}

static NSColorSpace *extendedSRGBScreenColorSpace(id, SEL)
{
    return NSColorSpace.extendedSRGBColorSpace;
}

TEST(DragAndDropTests, DragImageForExtendedRangeScreenColorSpace)
{
    InstanceMethodSwizzler swizzler {
        NSScreen.class, @selector(colorSpace),
        reinterpret_cast<IMP>(extendedSRGBScreenColorSpace)
    };
    RetainPtr bitmap = dragImageBitmapForStripedImageWithStyle(@"");
    ASSERT_TRUE(bitmap);
    EXPECT_EQ([bitmap pixelsWide], [bitmap pixelsHigh]);
    EXPECT_EQ(DragImageColor::Red, dragImageColorAt(bitmap, 0.1, 0.1));
    EXPECT_EQ(DragImageColor::Yellow, dragImageColorAt(bitmap, 0.9, 0.9));
}

#endif // PLATFORM(MAC)

TEST(DragAndDropTests, ExposeMultipleURLsInDataTransfer)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];

    NSString *stringData = @"Hello world";
    NSURL *firstURL = [NSURL URLWithString:@"https://webkit.org/"];
    NSURL *secondURL = [NSURL URLWithString:@"https://apple.com/"];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard writeObjects:@[ stringData, firstURL, secondURL ]];
    [simulator setExternalDragPasteboard:pasteboard];
#else
    RetainPtr stringItem = adoptNS([[NSItemProvider alloc] initWithObject:stringData]);
    RetainPtr firstURLItem = adoptNS([[NSItemProvider alloc] initWithObject:firstURL]);
    RetainPtr secondURLItem = adoptNS([[NSItemProvider alloc] initWithObject:secondURL]);
    for (NSItemProvider *item in @[ stringItem.get(), firstURLItem.get(), secondURLItem.get() ])
        item.preferredPresentationStyle = UIPreferredPresentationStyleInline;
    [simulator setExternalItemProviders:@[ stringItem.get(), firstURLItem.get(), secondURLItem.get() ]];
#endif

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("text/plain, text/uri-list", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/plain), (STRING, text/uri-list)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("https://webkit.org/\nhttps://apple.com/", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
}

#if PLATFORM(MAC)
TEST(DragAndDropTests, DragAndDropOnEmptyView)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    simulator.get().dragDestinationAction = WKDragDestinationActionAny;
    auto webView = [simulator webView];

    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard writeObjects:@[ url ]];
    [simulator setExternalDragPasteboard:pasteboard];

    __block bool finished = false;
    [webView performAfterLoading:^{
        finished = true;
    }];

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 100)];

    TestWebKitAPI::Util::run(&finished);

    EXPECT_WK_STREQ("Simple HTML file.", [webView stringByEvaluatingJavaScript:@"document.body.innerText"]);
}
#endif // PLATFORM(MAC)

TEST(DragAndDropTests, PreventingMouseDownShouldPreventDragStart)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];
    [simulator runFrom:CGPointMake(160, 100) to:CGPointMake(160, 300)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    EXPECT_WK_STREQ("dragstart dragend", [webView stringByEvaluatingJavaScript:@"output.textContent"]);

    // Now verify that preventing default on the 'mousedown' event cancels the drag altogether.
    [webView evaluateJavaScript:@"target.textContent = output.textContent = ''" completionHandler:nil];
    [webView evaluateJavaScript:@"source.addEventListener('mousedown', event => { event.preventDefault(); window.observedMousedown = true; })" completionHandler:nil];
    [simulator runFrom:CGPointMake(160, 100) to:CGPointMake(160, 300)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"observedMousedown"].boolValue);
}

struct DragStartData {
    BOOL containsFile { NO };
    NSString *text { nil };
    NSString *url { nil };
    NSString *html { nil };
    RetainPtr<NSDictionary<NSString *, NSString *>> customData;
};

static DragStartData runDragStartDataTestCase(DragAndDropSimulator *simulator, NSString *elementID)
{
    auto webView = [simulator webView];
    NSString *tagName = [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.getElementById('%@').tagName", elementID]];
    if (![tagName isEqualToString:@"IMG"] && ![tagName isEqualToString:@"A"])
        [webView selectElementWithID:elementID];

    [simulator dragFromElementWithID:elementID to:CGPointMake(400, 400)];
    RetainPtr<NSMutableDictionary<NSString *, NSString *>> allData = adoptNS([[webView objectByEvaluatingJavaScript:@"allData"] mutableCopy]);
    DragStartData result;
    result.text = [allData objectForKey:@"text/plain"];
    result.url = [allData objectForKey:@"text/uri-list"];
    result.html = [allData objectForKey:@"text/html"];
    result.containsFile = !![allData objectForKey:@"Files"];
    [allData removeObjectForKey:@"text/plain"];
    [allData removeObjectForKey:@"text/uri-list"];
    [allData removeObjectForKey:@"text/html"];
    [allData removeObjectForKey:@"Files"];
    if ([allData count])
        result.customData = WTF::move(allData);
    return result;
}

TEST(DragAndDropTests, DataTransferTypesOnDragStartForTextSelection)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 500, 500)]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"dragstart-data"];

    auto result = runDragStartDataTestCase(simulator.get(), @"regular");
    EXPECT_WK_STREQ("Regular text", result.text);
    EXPECT_NULL(result.url);
    EXPECT_TRUE([result.html containsString:@"Regular text"]);
    EXPECT_NULL(result.customData);
    EXPECT_FALSE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"regular-url");
    EXPECT_WK_STREQ("Regular text + URL data", result.text);
    EXPECT_WK_STREQ("https://www.google.com", result.url);
    EXPECT_TRUE([result.html containsString:@"Regular text + URL data"]);
    EXPECT_NULL(result.customData);
    EXPECT_FALSE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"regular-custom");
    EXPECT_WK_STREQ("Regular text + custom data", result.text);
    EXPECT_NULL(result.url);
    EXPECT_TRUE([result.html containsString:@"Regular text + custom data"]);
    EXPECT_WK_STREQ("Hello world", result.customData.get()[@"text/foo"]);
    EXPECT_FALSE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"url");
    EXPECT_NULL(result.text);
    EXPECT_WK_STREQ("https://www.google.com", result.url);
    EXPECT_NULL([result.html containsString:@"Regular text + custom data"]);
    EXPECT_NULL(result.customData);
    EXPECT_FALSE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"custom");
    EXPECT_NULL(result.text);
    EXPECT_NULL(result.url);
    EXPECT_NULL(result.html);
    EXPECT_WK_STREQ("Hello world", result.customData.get()[@"text/foo"]);
    EXPECT_FALSE(result.containsFile);
}

TEST(DragAndDropTests, DataTransferTypesOnDragStartForImage)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 500, 500)]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"dragstart-data"];

    auto result = runDragStartDataTestCase(simulator.get(), @"image");
    EXPECT_NULL(result.text);
    // The URL should be nil here because the image source is a file URL, so we shoud avoid exposing it to the page.
    EXPECT_NULL(result.url);
#if PLATFORM(MAC)
    EXPECT_TRUE([result.html containsString:@"<img"]);
#else
    EXPECT_NULL(result.html);
#endif
    EXPECT_NULL(result.customData);
    EXPECT_TRUE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"image-text");
    EXPECT_WK_STREQ("This is an image of Cupertino.", result.text);
    EXPECT_NULL(result.url);
#if PLATFORM(MAC)
    EXPECT_TRUE([result.html containsString:@"<img"]);
#else
    EXPECT_NULL(result.html);
#endif
    EXPECT_NULL(result.customData);
    EXPECT_TRUE(result.containsFile);
}

TEST(DragAndDropTests, DataTransferTypesOnDragStartForLink)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 500, 800)]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"dragstart-data"];

    auto result = runDragStartDataTestCase(simulator.get(), @"link");
    EXPECT_NULL(result.text);
    EXPECT_WK_STREQ("https://www.apple.com/", result.url);
    EXPECT_NULL(result.html);
    EXPECT_NULL(result.customData);
    EXPECT_FALSE(result.containsFile);

    result = runDragStartDataTestCase(simulator.get(), @"link-custom");
    EXPECT_NULL(result.text);
    EXPECT_WK_STREQ("https://www.apple.com/", result.url);
    EXPECT_NULL(result.html);
    EXPECT_WK_STREQ("bar", result.customData.get()[@"text/foo"]);
    EXPECT_FALSE(result.containsFile);
}

TEST(DragAndDropTests, DoNotCrashWhenRemovingNodeOnDrop)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"remove-node-on-drop"];
    [simulator runFrom:CGPointMake(150, 50) to:CGPointMake(150, 150)];
    EXPECT_TRUE([[webView contentsAsString] containsString:@"Drag me"]);
}

TEST(DragAndDropTests, ColorInputToColorInput)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ColorInputToDisabledColorInput)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").disabled = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, DisabledColorInputToColorInput)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").disabled = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ReadOnlyColorInputToReadOnlyColorInput)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").readOnly = true"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").readOnly = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ColorInputEvents)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];

    __block bool changeEventFired = false;
    [webView performAfterReceivingMessage:@"change" action:^() {
        changeEventFired = true;
    }];

    __block bool inputEventFired = false;
    [webView performAfterReceivingMessage:@"input" action:^() {
        inputEventFired = true;
    }];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    TestWebKitAPI::Util::run(&inputEventFired);
    TestWebKitAPI::Util::run(&changeEventFired);
}

#if ENABLE(IMAGE_ANALYSIS)

TEST(DragAndDropTests, DragElementWithImageOverlay)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    [[configuration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"simple-image-overlay"];

    [simulator runFrom:NSMakePoint(150, 40) to:NSMakePoint(300, 40)];
    EXPECT_FALSE([simulator containsDraggedType:UTTypeJPEG.identifier]);

    [simulator runFrom:NSMakePoint(150, 200) to:NSMakePoint(300, 200)];
    EXPECT_TRUE([simulator containsDraggedType:UTTypeJPEG.identifier]);
}

TEST(DragAndDropTests, DragSelectedTextInImageOverlay)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    [[configuration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"simple-image-overlay"];
    [[simulator webView] stringByEvaluatingJavaScript:@"selectImageOverlay()"];
    [[simulator webView] waitForNextPresentationUpdate];

    [simulator runFrom:NSMakePoint(150, 40) to:NSMakePoint(300, 300)];

    EXPECT_TRUE([simulator containsDraggedType:UTTypeUTF8PlainText.identifier]);
    EXPECT_FALSE([simulator containsDraggedType:UTTypeHTML.identifier]);
    EXPECT_FALSE([simulator containsDraggedType:UTTypeRTF.identifier]);

    RetainPtr<NSString> draggedText;
#if USE(APPKIT)
    draggedText = [[simulator draggingInfo].draggingPasteboard stringForType:UTTypeUTF8PlainText.identifier];
#else
    bool doneLoadingString = false;
    [[simulator sourceItemProviders].firstObject loadObjectOfClass:NSString.class completionHandler:[&](NSString *string, NSError *error) {
        draggedText = adoptNS([string copy]);
        doneLoadingString = true;
    }];
    TestWebKitAPI::Util::run(&doneLoadingString);
#endif
    EXPECT_WK_STREQ(draggedText.get(), "foobar");
}

#endif // ENABLE(IMAGE_ANALYSIS)

TEST(DragAndDropTests, DoNotExposeCrossOriginImageData)
{
    RetainPtr markupData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"drag-drop-cross-origin-image" withExtension:@"html"]];
    RetainPtr imageData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"icon" withExtension:@"png"]];

    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr<NSString> path = task.request.URL.path;
        RetainPtr<NSString> type;
        RetainPtr<NSData> result;
        if ([path isEqualToString:@"/main"]) {
            result = markupData;
            type = @"text/html";
        } else if ([path isEqualToString:@"/image"]) {
            result = imageData;
            type = @"image/png";
        }

        if (!result) {
            [task didFailWithError:[NSError errorWithDomain:@"DoNotExposeCrossOriginImageData" code:1 userInfo:nil]];
            return;
        }

        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:type expectedContentLength:[result length] textEncodingName:nil]);
        [task didReceiveResponse:response];
        [task didReceiveData:result];
        [task didFinish];
    }];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler forURLScheme:@"crossorigin"];
    [configuration setURLSchemeHandler:handler forURLScheme:@"sameorigin"];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 400, 400) configuration:configuration]);
    [[simulator webView] synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sameorigin://test/main"]]];

    auto runDragAndDropTests = [&] {
        // Drag and drop the cross-origin image first.
        [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 250)];
        EXPECT_EQ(0, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('dragenter')"] intValue]);
        EXPECT_EQ(0, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('dragover')"] intValue]);
        EXPECT_EQ(0, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('drop')"] intValue]);
        EXPECT_FALSE([[[simulator webView] objectByEvaluatingJavaScript:@"inconsistentFilesTypeOnDrop"] boolValue]);

        // Next, drag and drop the same-origin image.
        [simulator runFrom:CGPointMake(50, 150) to:CGPointMake(50, 250)];
        EXPECT_EQ(0, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('dragenter')"] intValue]);
        EXPECT_EQ(0, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('dragover')"] intValue]);
        EXPECT_EQ(1, [[[simulator webView] objectByEvaluatingJavaScript:@"numberOfFiles.get('drop')"] intValue]);
        EXPECT_FALSE([[[simulator webView] objectByEvaluatingJavaScript:@"inconsistentFilesTypeOnDrop"] boolValue]);
    };

    runDragAndDropTests();
    [[simulator webView] objectByEvaluatingJavaScript:@"addDraggableAttribute()"];
    runDragAndDropTests();
}

#endif // ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)

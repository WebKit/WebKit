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
#import "Test.h"

#if PLATFORM(MAC) || PLATFORM(IOS)

#import "DragAndDropSimulator.h"
#import "InstanceMethodSwizzler.h"
#import "NSItemProviderAdditions.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <Contacts/Contacts.h>
#import <MapKit/MapKit.h>
#import <QuickLookThumbnailing/QLThumbnailGenerator.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CLASS(Contacts, CNMutableContact)

SOFT_LINK_FRAMEWORK(MapKit)
SOFT_LINK_CLASS(MapKit, MKMapItem)
SOFT_LINK_CLASS(MapKit, MKPlacemark)

SOFT_LINK_FRAMEWORK(QuickLookThumbnailing)
SOFT_LINK_CLASS(QuickLookThumbnailing, QLThumbnailGenerator)

@interface NSArray (AttachmentTestingHelpers)
- (_WKAttachment *)_attachmentWithName:(NSString *)name;
@end

@implementation NSArray (AttachmentTestingHelpers)
- (_WKAttachment *)_attachmentWithName:(NSString *)name
{
    for (_WKAttachment *attachment in self) {
        if ([attachment.info.name isEqualToString:name])
            return attachment;
    }
    return nil;
}
@end

#define USES_MODERN_ATTRIBUTED_STRING_CONVERSION (!PLATFORM(WATCHOS) && !PLATFORM(APPLETV))

#if USE(APPKIT)
using CocoaPasteboard = NSPasteboard;
#else
using CocoaPasteboard = UIPasteboard;
#endif

@interface AttachmentUpdateObserver : NSObject <WKUIDelegatePrivate>
@property (nonatomic, readonly) NSArray *inserted;
@property (nonatomic, readonly) NSArray *removed;
@property (nonatomic, readonly) NSArray *dataInvalidated;
@end

@implementation AttachmentUpdateObserver {
    RetainPtr<NSMutableArray<_WKAttachment *>> _inserted;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removed;
    RetainPtr<NSMutableArray<_WKAttachment *>> _dataInvalidated;
    RetainPtr<NSMutableDictionary<NSString *, NSString *>> _identifierToSourceMap;
}

- (instancetype)init
{
    if (self = [super init]) {
        _inserted = adoptNS([[NSMutableArray alloc] init]);
        _removed = adoptNS([[NSMutableArray alloc] init]);
        _dataInvalidated = adoptNS([[NSMutableArray alloc] init]);
        _identifierToSourceMap = adoptNS([[NSMutableDictionary alloc] init]);
    }
    return self;
}

- (NSArray<_WKAttachment *> *)inserted
{
    return _inserted.get();
}

- (NSArray<_WKAttachment *> *)removed
{
    return _removed.get();
}

- (NSArray<_WKAttachment *> *)dataInvalidated
{
    return _dataInvalidated.get();
}

- (NSString *)sourceForIdentifier:(NSString *)identifier
{
    return [_identifierToSourceMap objectForKey:identifier];
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_inserted addObject:attachment];
    if (source)
        [_identifierToSourceMap setObject:source forKey:attachment.uniqueIdentifier];
}

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment
{
    [_removed addObject:attachment];
}

- (void)_webView:(WKWebView *)webView didInvalidateDataForAttachment:(_WKAttachment *)attachment
{
    [_dataInvalidated addObject:attachment];
}

@end

static NSURL *testiWorkAttachmentFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testiWorkAttachmentData()
{
    return [NSData dataWithContentsOfURL:testiWorkAttachmentFileURL()];
}

#if PLATFORM(MAC)

static NSURL *testDirectoryAttachmentFileURL()
{
    NSString *folderName = [NSString stringWithFormat:@"some.directory-%@", [NSUUID UUID].UUIDString];
    auto temporaryFolder = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:folderName] isDirectory:YES];
    NSError *error = nil;
    [[NSFileManager defaultManager] createDirectoryAtURL:temporaryFolder withIntermediateDirectories:NO attributes:nil error:&error];

    return temporaryFolder;
}

#endif

@interface AttachmentUIDelegate : NSObject<WKUIDelegatePrivate>
@end

@implementation AttachmentUIDelegate

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)wkAttachment withSource:(NSString *)source
{
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testiWorkAttachmentFileURL() options:0 error:NULL]);
    [wkAttachment setFileWrapper:fileWrapper.get() contentType:nil completion:nil];
}

@end

namespace TestWebKitAPI {

class ObserveAttachmentUpdatesForScope {
public:
    ObserveAttachmentUpdatesForScope(TestWKWebView *webView)
        : m_webView(webView)
    {
        m_previousDelegate = webView.UIDelegate;
        m_observer = adoptNS([[AttachmentUpdateObserver alloc] init]);
        webView.UIDelegate = m_observer.get();
    }

    ~ObserveAttachmentUpdatesForScope()
    {
        [m_webView setUIDelegate:m_previousDelegate.get()];
    }

    AttachmentUpdateObserver *observer() const { return m_observer.get(); }

    void expectAttachmentUpdates(NSArray<_WKAttachment *> *removed, NSArray<_WKAttachment *> *inserted)
    {
        BOOL removedAttachmentsMatch = [[NSSet setWithArray:observer().removed] isEqual:[NSSet setWithArray:removed]];
        if (!removedAttachmentsMatch)
            NSLog(@"Expected removed attachments: %@ to match %@.", observer().removed, removed);
        EXPECT_TRUE(removedAttachmentsMatch);

        BOOL insertedAttachmentsMatch = [[NSSet setWithArray:observer().inserted] isEqual:[NSSet setWithArray:inserted]];
        if (!insertedAttachmentsMatch)
            NSLog(@"Expected inserted attachments: %@ to match %@.", observer().inserted, inserted);
        EXPECT_TRUE(insertedAttachmentsMatch);
    }

    void expectAttachmentInvalidation(NSArray<_WKAttachment *> *dataInvalidated)
    {
        BOOL invalidatedAttachmentsMatch = [[NSSet setWithArray:observer().dataInvalidated] isEqual:[NSSet setWithArray:dataInvalidated]];
        if (!invalidatedAttachmentsMatch)
            NSLog(@"Expected attachments with invalidated data: %@ to match %@.", observer().dataInvalidated, dataInvalidated);
        EXPECT_TRUE(invalidatedAttachmentsMatch);
    }

    void expectSourceForIdentifier(NSString *expectedSource, NSString *identifier)
    {
        NSString *observedSource = [observer() sourceForIdentifier:identifier];
        BOOL success = observedSource == expectedSource || [observedSource isEqualToString:expectedSource];
        EXPECT_TRUE(success);
        if (!success)
            NSLog(@"Expected source: %@ but observed: %@", expectedSource, observedSource);
    }

private:
    RetainPtr<AttachmentUpdateObserver> m_observer;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<id> m_previousDelegate;
};

}

@interface TestWKWebView (AttachmentTesting)
@end

static NSString *attachmentEditingTestMarkup = @"<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<script>focus = () => document.body.focus()</script>"
    "<body onload=focus() contenteditable></body>";

static RetainPtr<TestWKWebView> webViewForTestingAttachments(CGSize webViewSize, WKWebViewConfiguration *configuration)
{
    configuration._attachmentElementEnabled = YES;
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[configuration preferences], YES);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, webViewSize.width, webViewSize.height) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    return webView;
}

static RetainPtr<TestWKWebView> webViewForTestingAttachments(CGSize webViewSize)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    return webViewForTestingAttachments(webViewSize, configuration.get());
}

static RetainPtr<TestWKWebView> webViewForTestingAttachments()
{
    return webViewForTestingAttachments(CGSizeMake(500, 500));
}

static NSData *testZIPData()
{
    NSURL *zipFileURL = [[NSBundle mainBundle] URLForResource:@"compressed-files" withExtension:@"zip" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:zipFileURL];
}

static NSData *testHTMLData()
{
    return [@"<a href='#'>This is some HTML data</a>" dataUsingEncoding:NSUTF8StringEncoding];
}

static NSURL *testImageFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testImageData()
{
    return [NSData dataWithContentsOfURL:testImageFileURL()];
}

static NSURL *testGIFFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"apple" withExtension:@"gif" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testGIFData()
{
    return [NSData dataWithContentsOfURL:testGIFFileURL()];
}

static NSURL *testPDFFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testPDFData()
{
    return [NSData dataWithContentsOfURL:testPDFFileURL()];
}

static NSURL *testJPEGFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"sunset-in-cupertino-600px" withExtension:@"jpg" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testJPEGData()
{
    return [NSData dataWithContentsOfURL:testJPEGFileURL()];
}

@implementation TestWKWebView (AttachmentTesting)

- (_WKAttachment *)synchronouslyInsertAttachmentWithFileWrapper:(NSFileWrapper *)fileWrapper contentType:(NSString *)contentType
{
    __block bool done = false;
    RetainPtr<_WKAttachment> attachment = [self _insertAttachmentWithFileWrapper:fileWrapper contentType:contentType completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return attachment.autorelease();
}

- (_WKAttachment *)synchronouslyInsertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data
{
    __block bool done = false;
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (filename)
        [fileWrapper setPreferredFilename:filename];
    RetainPtr<_WKAttachment> attachment = [self _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:contentType completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return attachment.autorelease();
}

- (NSArray<NSValue *> *)allBoundingClientRects:(NSString *)querySelector
{
    auto rects = adoptNS([[NSMutableArray alloc] init]);
    bool doneEvaluatingScript = false;
    NSString *script = [NSString stringWithFormat:@"Array.from(document.querySelectorAll('%@')).map(e => { const r = e.getBoundingClientRect(); return [r.left, r.top, r.width, r.height]; })", querySelector];
    [self evaluateJavaScript:script completionHandler:[rects, &doneEvaluatingScript] (NSArray<NSArray<NSNumber *> *> *result, NSError *) {
        for (NSArray<NSNumber *> *rectInfo in result) {
#if PLATFORM(IOS_FAMILY)
            [rects addObject:[NSValue valueWithCGRect:CGRectMake(rectInfo[0].floatValue, rectInfo[1].floatValue, rectInfo[2].floatValue, rectInfo[3].floatValue)]];
#else
            [rects addObject:[NSValue valueWithRect:NSMakeRect(rectInfo[0].floatValue, rectInfo[1].floatValue, rectInfo[2].floatValue, rectInfo[3].floatValue)]];
#endif
        }
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return rects.autorelease();
}

- (CGPoint)attachmentElementMidPoint
{
    __block CGPoint midPoint;
    __block bool doneEvaluatingScript = false;
    [self evaluateJavaScript:@"r = document.querySelector('attachment').getBoundingClientRect(); [r.left + r.width / 2, r.top + r.height / 2]" completionHandler:^(NSArray<NSNumber *> *result, NSError *) {
        midPoint = CGPointMake(result.firstObject.floatValue, result.lastObject.floatValue);
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return midPoint;
}

- (CGSize)attachmentElementSize
{
    __block CGSize size;
    __block bool doneEvaluatingScript = false;
    [self evaluateJavaScript:@"r = document.querySelector('attachment').getBoundingClientRect(); [r.width, r.height]" completionHandler:^(NSArray<NSNumber *> *sizeResult, NSError *) {
        size = CGSizeMake(sizeResult.firstObject.floatValue, sizeResult.lastObject.floatValue);
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return size;
}

- (CGSize)imageElementSize
{
    __block CGSize size;
    __block bool doneEvaluatingScript = false;
    [self evaluateJavaScript:@"r = document.querySelector('img').getBoundingClientRect(); [r.width, r.height]" completionHandler:^(NSArray<NSNumber *> *sizeResult, NSError *) {
        size = CGSizeMake(sizeResult.firstObject.floatValue, sizeResult.lastObject.floatValue);
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return size;
}

- (void)waitForImageElementSizeToBecome:(CGSize)expectedSize
{
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (CGSizeEqualToSize(self.imageElementSize, expectedSize))
            break;
    }
}

- (BOOL)hasAttribute:(NSString *)attributeName forQuerySelector:(NSString *)querySelector
{
    return [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').hasAttribute('%@')", querySelector, attributeName]].boolValue;
}

- (NSString *)valueOfAttribute:(NSString *)attributeName forQuerySelector:(NSString *)querySelector
{
    return [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').getAttribute('%@')", querySelector, attributeName]];
}

- (void)expectUpdatesAfterCommand:(NSString *)command withArgument:(NSString *)argument expectedRemovals:(NSArray<_WKAttachment *> *)removed expectedInsertions:(NSArray<_WKAttachment *> *)inserted
{
    TestWebKitAPI::ObserveAttachmentUpdatesForScope observer(self);
    EXPECT_TRUE([self _synchronouslyExecuteEditCommand:command argument:argument]);
    observer.expectAttachmentUpdates(removed, inserted);
}

- (NSString *)ensureAttachmentForImageElement
{
    __block bool doneWaitingForAttachmentInsertion = false;
    [self performAfterReceivingMessage:@"inserted" action:^{
        doneWaitingForAttachmentInsertion = true;
    }];

    const char *scriptForEnsuringAttachmentIdentifier = \
        "const identifier = HTMLAttachmentElement.getAttachmentIdentifier(document.querySelector('img'));"
        "setTimeout(() => webkit.messageHandlers.testHandler.postMessage('inserted'), 0);"
        "identifier";

    RetainPtr attachmentIdentifier = [self stringByEvaluatingJavaScript:@(scriptForEnsuringAttachmentIdentifier)];
    TestWebKitAPI::Util::run(&doneWaitingForAttachmentInsertion);

    return attachmentIdentifier.autorelease();
}

@end

@implementation NSData (AttachmentTesting)

- (NSString *)shortDescription
{
    return [NSString stringWithFormat:@"<%tu bytes>", self.length];
}

@end

@implementation _WKAttachment (AttachmentTesting)

- (void)synchronouslySetFileWrapper:(NSFileWrapper *)fileWrapper newContentType:(NSString *)newContentType error:(NSError **)error
{
    __block RetainPtr<NSError> resultError;
    __block bool done = false;

    [self setFileWrapper:fileWrapper contentType:newContentType completion:^(NSError *error) {
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();
}

- (void)synchronouslySetData:(NSData *)data newContentType:(NSString *)newContentType newFilename:(NSString *)newFilename error:(NSError **)error
{
    __block RetainPtr<NSError> resultError;
    __block bool done = false;
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (newFilename)
        [fileWrapper setPreferredFilename:newFilename];

    [self setFileWrapper:fileWrapper.get() contentType:newContentType completion:^(NSError *error) {
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();
}

- (void)expectRequestedDataToBe:(NSData *)expectedData
{
    NSError *requestError = nil;
    _WKAttachmentInfo *info = self.info;

    BOOL observedDataIsEqualToExpectedData = info.data == expectedData || [info.data isEqualToData:expectedData];
    EXPECT_TRUE(observedDataIsEqualToExpectedData);
    if (!observedDataIsEqualToExpectedData) {
        NSLog(@"Expected data: %@ but observed: %@ for %@", [expectedData shortDescription], [info.data shortDescription], self);
        NSLog(@"Observed error: %@ while reading data for %@", requestError, self);
    }
}

@end

static void runTestWithTemporaryFolder(void(^runTest)(NSURL *folderURL))
{
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    auto temporaryFolder = retainPtr([NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"folder-%@", NSUUID.UUID]] isDirectory:YES]);
    [defaultManager removeItemAtURL:temporaryFolder.get() error:nil];
    [defaultManager createDirectoryAtURL:temporaryFolder.get() withIntermediateDirectories:NO attributes:nil error:nil];
    [testImageData() writeToURL:[temporaryFolder.get() URLByAppendingPathComponent:@"image.png" isDirectory:NO] atomically:YES];
    [testZIPData() writeToURL:[temporaryFolder.get() URLByAppendingPathComponent:@"archive.zip" isDirectory:NO] atomically:YES];
    @try {
        runTest(temporaryFolder.get());
    } @finally {
        [[NSFileManager defaultManager] removeItemAtURL:temporaryFolder.get() error:nil];
    }
}

#if PLATFORM(IOS_FAMILY)

static void runTestWithTemporaryImageFile(NSString *fileName, void(^runTest)(NSURL *fileURL))
{
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    auto temporaryFilePath = retainPtr([NSTemporaryDirectory() stringByAppendingPathComponent:fileName]);
    auto temporaryFileURL = retainPtr([NSURL fileURLWithPath:temporaryFilePath.get()]);
    [defaultManager removeItemAtURL:temporaryFileURL.get() error:nil];
    [testImageData() writeToFile:temporaryFilePath.get() atomically:YES];
    @try {
        runTest(temporaryFileURL.get());
    } @finally {
        [defaultManager removeItemAtURL:temporaryFileURL.get() error:nil];
    }
}

#endif // PLATFORM(IOS_FAMILY)

static void simulateFolderDragWithURL(DragAndDropSimulator *simulator, NSURL *folderURL)
{
#if PLATFORM(MAC)
    [simulator writePromisedFiles:@[ folderURL ]];
#else
    auto folderProvider = adoptNS([[NSItemProvider alloc] init]);
    [folderProvider setSuggestedName:folderURL.lastPathComponent];
    [folderProvider setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [folderProvider registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeFolder fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedFolderURL = retainPtr(folderURL)] (void(^completion)(NSURL *, BOOL, NSError *)) -> NSProgress * {
        completion(protectedFolderURL.get(), NO, nil);
        return nil;
    }];
    simulator.externalItemProviders = @[ folderProvider.get() ];
#endif
}

#pragma mark - Platform testing helper functions

#if PLATFORM(MAC)

BOOL isCompletelyTransparent(NSImage *image)
{
    auto representation = adoptNS([[NSBitmapImageRep alloc] initWithData:image.TIFFRepresentation]);
    for (int row = 0; row < image.size.height; ++row) {
        for (int column = 0; column < image.size.width; ++column) {
            if ([representation colorAtX:column y:row].alphaComponent)
                return false;
        }
    }
    return true;
}

static NSString *nonexistentFilePath(NSString *extension)
{
    auto nonexistentFileName = [NSString stringWithFormat:@"%@.%@", NSUUID.UUID.UUIDString, extension];
    return [NSTemporaryDirectory() stringByAppendingPathComponent:nonexistentFileName];
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

typedef void(^ItemProviderDataLoadHandler)(NSData *, NSError *);

@implementation NSItemProvider (AttachmentTesting)

- (void)registerData:(NSData *)data type:(NSString *)type
{
    [self registerDataRepresentationForTypeIdentifier:type visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedData = retainPtr(data)] (ItemProviderDataLoadHandler completionHandler) -> NSProgress * {
        completionHandler(protectedData.get(), nil);
        return nil;
    }];
}

- (void)expectType:(NSString *)type withData:(NSData *)expectedData
{
    BOOL containsType = [self.registeredTypeIdentifiers containsObject:type];
    EXPECT_TRUE(containsType);
    if (!containsType) {
        NSLog(@"Expected: %@ to contain %@", self, type);
        return;
    }

    __block bool done = false;
    [self loadDataRepresentationForTypeIdentifier:type completionHandler:^(NSData *observedData, NSError *error) {
        EXPECT_TRUE([observedData isEqualToData:expectedData]);
        if (![observedData isEqualToData:expectedData])
            NSLog(@"Expected data: <%tu bytes> to be equal to data: <%tu bytes>", observedData.length, expectedData.length);
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error when loading data: %@", error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

#endif // PLATFORM(IOS_FAMILY)

void platformCopyRichTextWithMultipleAttachments()
{
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(__bridge NSString *)kUTTypePNG]);
    auto pdf = adoptNS([[NSTextAttachment alloc] initWithData:testPDFData() ofType:(__bridge NSString *)kUTTypePDF]);
    auto zip = adoptNS([[NSTextAttachment alloc] initWithData:testZIPData() ofType:(__bridge NSString *)kUTTypeZipArchive]);

    auto richText = adoptNS([[NSMutableAttributedString alloc] init]);
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:image.get()]];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:pdf.get()]];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:zip.get()]];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ richText.get() ]];
#elif PLATFORM(IOS_FAMILY)
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [UIPasteboard generalPasteboard].itemProviders = @[ item.get() ];
#endif
}

void platformCopyRichTextWithImage()
{
    auto richText = adoptNS([[NSMutableAttributedString alloc] init]);
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(__bridge NSString *)kUTTypePNG]);

    [richText appendAttributedString:adoptNS([[NSAttributedString alloc] initWithString:@"Lorem ipsum "]).get()];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:image.get()]];
    [richText appendAttributedString:adoptNS([[NSAttributedString alloc] initWithString:@" dolor sit amet."]).get()];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ richText.get() ]];
#elif PLATFORM(IOS_FAMILY)
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [UIPasteboard generalPasteboard].itemProviders = @[ item.get() ];
#endif
}

void platformCopyPNG()
{
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[NSPasteboardTypePNG] owner:nil];
    [pasteboard setData:testImageData() forType:NSPasteboardTypePNG];
#elif PLATFORM(IOS_FAMILY)
    UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [item registerData:testImageData() type:(__bridge NSString *)kUTTypePNG];
    pasteboard.itemProviders = @[ item.get() ];
#endif
}

void platformCopyPDF()
{
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[(__bridge NSString *)kUTTypePDF] owner:nil];
    [pasteboard setData:testPDFData() forType:(__bridge NSString *)kUTTypePDF];
#elif PLATFORM(IOS_FAMILY)
    UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [item registerData:testPDFData() type:(__bridge NSString *)kUTTypePDF];
    pasteboard.itemProviders = @[ item.get() ];
#endif
}

void platformCopyJPEG()
{
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[(__bridge NSString *)kUTTypeJPEG] owner:nil];
    [pasteboard setData:testJPEGData() forType:(__bridge NSString *)kUTTypeJPEG];
#elif PLATFORM(IOS_FAMILY)
    UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [item registerData:testJPEGData() type:(__bridge NSString *)kUTTypeJPEG];
    pasteboard.itemProviders = @[ item.get() ];
#endif
}

#if PLATFORM(MAC)
typedef NSImage PlatformImage;
#else
typedef UIImage PlatformImage;
#endif

PlatformImage *platformImageWithData(NSData *data)
{
#if PLATFORM(MAC)
    return adoptNS([[NSImage alloc] initWithData:data]).autorelease();
#else
    return [UIImage imageWithData:data];
#endif
}

@interface FileWrapper : NSFileWrapper
@end

@implementation FileWrapper
@end

namespace TestWebKitAPI {

#pragma mark - Platform-agnostic tests

TEST(WKAttachmentTests, AttachmentElementInsertion)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> firstAttachment;
    RetainPtr<_WKAttachment> secondAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        // Use the given content type for the attachment element's type.
        firstAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo" contentType:@"text/html" data:testHTMLData()];
        EXPECT_WK_STREQ(@"foo", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/html", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"38 bytes", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ firstAttachment.get() ]);
    }

    _WKAttachmentInfo *info = [firstAttachment info];
    EXPECT_TRUE([info.data isEqualToData:testHTMLData()]);
    EXPECT_TRUE([info.contentType isEqualToString:@"text/html"]);
    EXPECT_TRUE([info.name isEqualToString:@"foo"]);
    EXPECT_EQ(info.filePath.length, 0U);

    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        // Since no content type is explicitly specified, compute it from the file extension.
        [webView _executeEditCommand:@"DeleteBackward" argument:nil completion:nil];
        secondAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"bar.png" contentType:nil data:testImageData()];
        EXPECT_WK_STREQ(@"bar.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"37 KB", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        scope.expectAttachmentUpdates(@[ firstAttachment.get() ], @[ secondAttachment.get() ]);
    }

    [firstAttachment expectRequestedDataToBe:testHTMLData()];
    [secondAttachment expectRequestedDataToBe:testImageData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingAndDeletingNewline)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData()];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body)"];
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    _WKAttachmentInfo *info = [attachment info];
    EXPECT_TRUE([info.data isEqualToData:testHTMLData()]);
    EXPECT_TRUE([info.contentType isEqualToString:@"text/plain"]);
    EXPECT_TRUE([info.name isEqualToString:@"foo.txt"]);
    EXPECT_EQ(info.filePath.length, 0U);

    [webView expectUpdatesAfterCommand:@"DeleteForward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenUndoingAndRedoing)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> htmlData = testHTMLData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData()];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:htmlData.get()];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenChangingFontStyles)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello"];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData()];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"World" expectedRemovals:@[] expectedInsertions:@[]];
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView expectUpdatesAfterCommand:@"ToggleBold" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleItalic" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleUnderline" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);

    // Inserting text should delete the current selection, removing the attachment in the process.
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"foo" expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingLists)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData()];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    // This edit command behaves more like a "toggle", and will actually break us out of the list we just inserted.
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingRichMarkup)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"InsertHTML" argument:@"<div><strong><attachment src='cid:123-4567' title='a'></attachment></strong></div>"];
        attachment = observer.observer().inserted[0];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
        observer.expectSourceForIdentifier(@"cid:123-4567", [attachment uniqueIdentifier]);
    }
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').remove()"];
        [webView waitForNextPresentationUpdate];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenCuttingAndPasting)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData()];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Cut" argument:nil];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentDataForEmptyFile)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"empty.txt" contentType:@"text/plain" data:[NSData data]];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
    [attachment expectRequestedDataToBe:[NSData data]];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        scope.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
    [attachment expectRequestedDataToBe:[NSData data]];
}

TEST(WKAttachmentTests, DropFolderAsAttachmentAndMoveByDragging)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [[simulator webView] synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    runTestWithTemporaryFolder([simulator] (NSURL *folderURL) {
        simulateFolderDragWithURL(simulator.get(), folderURL);
        [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];

        TestWKWebView *webView = [simulator webView];
        auto attachment = retainPtr([simulator insertedAttachments].firstObject);
#if PLATFORM(IOS_FAMILY)
        NSString *expectedType = (__bridge NSString *)kUTTypeFolder;
#else
        NSString *expectedType = (__bridge NSString *)kUTTypeDirectory;
#endif
        EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
        EXPECT_WK_STREQ(expectedType, [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(folderURL.lastPathComponent, [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);

        NSFileWrapper *image = [attachment info].fileWrapper.fileWrappers[@"image.png"];
        NSFileWrapper *archive = [attachment info].fileWrapper.fileWrappers[@"archive.zip"];
        EXPECT_TRUE([image.regularFileContents isEqualToData:testImageData()]);
        EXPECT_TRUE([archive.regularFileContents isEqualToData:testZIPData()]);

        [webView evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
        [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];
        [webView _executeEditCommand:@"InsertHTML" argument:@"<em>foo</em>" completion:nil];
        [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];

        [webView expectElementTag:@"ATTACHMENT" toComeBefore:@"EM"];
        [simulator clearExternalDragInformation];
        [simulator runFrom:webView.attachmentElementMidPoint to:CGPointMake(300, 300)];
        [webView expectElementTag:@"EM" toComeBefore:@"ATTACHMENT"];
    });
}

TEST(WKAttachmentTests, InsertFolderAndFileWithUnknownExtension)
{
    auto webView = webViewForTestingAttachments();
    auto file = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testHTMLData()]);
    [file setPreferredFilename:@"test.foobar"];
    auto image = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testImageData()]);
    auto document = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testPDFData()]);
    auto folder = adoptNS([[NSFileWrapper alloc] initDirectoryWithFileWrappers:@{ @"image.png": image.get(), @"document.pdf": document.get() }]);
    [folder setPreferredFilename:@"folder"];

    RetainPtr<_WKAttachment> firstAttachment;
    RetainPtr<_WKAttachment> secondAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        firstAttachment = [webView synchronouslyInsertAttachmentWithFileWrapper:file.get() contentType:nil];
        observer.expectAttachmentUpdates(@[ ], @[ firstAttachment.get() ]);
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        secondAttachment = [webView synchronouslyInsertAttachmentWithFileWrapper:folder.get() contentType:nil];
        observer.expectAttachmentUpdates(@[ ], @[ secondAttachment.get() ]);
    }

    auto checkAttachmentConsistency = [webView, file, folder] (_WKAttachment *expectedFileAttachment, _WKAttachment *expectedFolderAttachment) {
        [webView expectElementCount:2 querySelector:@"ATTACHMENT"];
        EXPECT_TRUE(UTTypeConformsTo((__bridge CFStringRef)[webView valueOfAttribute:@"type" forQuerySelector:@"attachment[title=folder]"], kUTTypeDirectory));
        EXPECT_TRUE(UTTypeConformsTo((__bridge CFStringRef)[webView valueOfAttribute:@"type" forQuerySelector:@"attachment[title^=test]"], kUTTypeData));
        EXPECT_WK_STREQ(expectedFileAttachment.uniqueIdentifier, [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment[title^=test]').uniqueIdentifier"]);
        EXPECT_WK_STREQ(expectedFolderAttachment.uniqueIdentifier, [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment[title=folder]').uniqueIdentifier"]);
        EXPECT_TRUE([expectedFileAttachment.info.fileWrapper isEqual:file.get()]);
        EXPECT_TRUE([expectedFolderAttachment.info.fileWrapper isEqual:folder.get()]);
    };

    checkAttachmentConsistency(firstAttachment.get(), secondAttachment.get());

    {
        // Swap the two attachments' file wrappers without creating or destroying attachment elements.
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [firstAttachment synchronouslySetFileWrapper:folder.get() newContentType:nil error:nil];
        [secondAttachment synchronouslySetFileWrapper:file.get() newContentType:nil error:nil];
        observer.expectAttachmentUpdates(@[ ], @[ ]);
    }

    checkAttachmentConsistency(secondAttachment.get(), firstAttachment.get());
}

TEST(WKAttachmentTests, ChangeAttachmentDataAndFileInformation)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        RetainPtr<NSData> pdfData = testPDFData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"test.pdf" contentType:@"application/pdf" data:pdfData.get()];
        [attachment expectRequestedDataToBe:pdfData.get()];
        EXPECT_WK_STREQ(@"test.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    {
        RetainPtr<NSData> imageData = testImageData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetData:imageData.get() newContentType:@"image/png" newFilename:@"icon.png" error:nil];
        [attachment expectRequestedDataToBe:imageData.get()];
        EXPECT_WK_STREQ(@"icon.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ ]);
    }
    {
        RetainPtr<NSData> textData = [@"Hello world" dataUsingEncoding:NSUTF8StringEncoding];
        ObserveAttachmentUpdatesForScope observer(webView.get());
        // The new content type should be inferred from the file name.
        [attachment synchronouslySetData:textData.get() newContentType:nil newFilename:@"foo.txt" error:nil];
        [attachment expectRequestedDataToBe:textData.get()];
        EXPECT_WK_STREQ(@"foo.txt", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/plain", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ ]);
    }
    {
        RetainPtr<NSData> htmlData = testHTMLData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetData:htmlData.get() newContentType:@"text/html" newFilename:@"bar" error:nil];
        [attachment expectRequestedDataToBe:htmlData.get()];
        EXPECT_WK_STREQ(@"bar", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/html", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ ]);
    }
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[ ]];
}

TEST(WKAttachmentTests, RemoveNewlinesBeforePastedImage)
{
    platformCopyPNG();

    RetainPtr<_WKAttachment> attachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }

    auto size = platformImageWithData([attachment info].data).size;
    EXPECT_EQ(215., size.width);
    EXPECT_EQ(174., size.height);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);

    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body, 0)"];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
        observer.expectAttachmentUpdates(@[ ], @[ ]);
        [webView expectElementTagsInOrder:@[ @"BR", @"BR", @"IMG" ]];
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[ ], @[ ]);
        [webView expectElementCount:0 querySelector:@"BR"];
    }
}

TEST(WKAttachmentTests, CutAndPastePastedImage)
{
    platformCopyPNG();

    RetainPtr<_WKAttachment> attachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"Cut" argument:nil];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }
}

TEST(WKAttachmentTests, MovePastedImageByDragging)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    platformCopyPNG();
    [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];
    [webView _executeEditCommand:@"InsertHTML" argument:@"<strong>text</strong>" completion:nil];
    [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
    [webView expectElementTag:@"IMG" toComeBefore:@"STRONG"];
    [webView expectElementCount:1 querySelector:@"IMG"];

    // Drag the attachment element to somewhere below the strong text.
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 350)];

    [webView expectElementTag:@"STRONG" toComeBefore:@"IMG"];
    [webView expectElementCount:1 querySelector:@"IMG"];
    EXPECT_EQ([simulator insertedAttachments].count, [simulator removedAttachments].count);

    [simulator endDataTransfer];
}

TEST(WKAttachmentTests, InsertPastedAttributedStringContainingImage)
{
    auto webView = webViewForTestingAttachments();
    platformCopyRichTextWithImage();

    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }

    [attachment expectRequestedDataToBe:testImageData()];
    EXPECT_WK_STREQ("Lorem ipsum  dolor sit amet.", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
}

TEST(WKAttachmentTests, InsertPastedAttributedStringContainingMultipleAttachments)
{
    auto webView = webViewForTestingAttachments();
    platformCopyRichTextWithMultipleAttachments();

    RetainPtr<_WKAttachment> imageAttachment;
    RetainPtr<_WKAttachment> zipAttachment;
    RetainPtr<_WKAttachment> pdfAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(3U, observer.observer().inserted.count);
        for (_WKAttachment *attachment in observer.observer().inserted) {
            NSData *data = attachment.info.data;
            if ([data isEqualToData:testZIPData()])
                zipAttachment = attachment;
            else if ([data isEqualToData:testPDFData()])
                pdfAttachment = attachment;
            else if ([data isEqualToData:testImageData()])
                imageAttachment = attachment;
        }
    }

    EXPECT_TRUE(zipAttachment && imageAttachment && pdfAttachment);
    [webView expectElementCount:2 querySelector:@"ATTACHMENT"];
    [webView expectElementCount:1 querySelector:@"IMG"];
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);

    NSString *zipAttachmentType = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"];
#if USES_MODERN_ATTRIBUTED_STRING_CONVERSION
    EXPECT_WK_STREQ("application/zip", zipAttachmentType);
#else
    EXPECT_WK_STREQ("application/octet-stream", zipAttachmentType);
#endif

    EXPECT_WK_STREQ([imageAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);
    EXPECT_WK_STREQ([pdfAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].uniqueIdentifier"]);
    EXPECT_WK_STREQ([zipAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].uniqueIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        NSArray<_WKAttachment *> *removedAttachments = [observer.observer() removed];
        EXPECT_EQ(3U, removedAttachments.count);
        EXPECT_TRUE([removedAttachments containsObject:zipAttachment.get()]);
        EXPECT_TRUE([removedAttachments containsObject:imageAttachment.get()]);
        EXPECT_TRUE([removedAttachments containsObject:pdfAttachment.get()]);
    }
}

TEST(WKAttachmentTests, InsertDataURLImagesAsAttachments)
{
    auto webContentSourceView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    [webContentSourceView synchronouslyLoadTestPageNamed:@"apple-data-url"];
    [webContentSourceView selectAll:nil];
    [webContentSourceView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    RetainPtr<_WKAttachment> pastedAttachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        pastedAttachment = observer.observer().inserted.firstObject;
    }

    RetainPtr info = [pastedAttachment info];
    EXPECT_WK_STREQ("image/gif", [info contentType]);
    EXPECT_GT([info data].length, 0U);
    EXPECT_WK_STREQ("This is an apple", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(WKAttachmentTests, InsertAndRemoveDuplicateAttachment)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> data = testHTMLData();
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
    RetainPtr<_WKAttachment> originalAttachment;
    RetainPtr<_WKAttachment> pastedAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        originalAttachment = [webView synchronouslyInsertAttachmentWithFileWrapper:fileWrapper.get() contentType:@"text/plain"];
        EXPECT_EQ(0U, observer.observer().removed.count);
        observer.expectAttachmentUpdates(@[ ], @[ originalAttachment.get() ]);
    }
    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];
    [webView evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        pastedAttachment = observer.observer().inserted.firstObject;
        EXPECT_FALSE([pastedAttachment isEqual:originalAttachment.get()]);
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[ pastedAttachment.get() ], @[ ]);
        [originalAttachment expectRequestedDataToBe:data.get()];
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView evaluateJavaScript:@"getSelection().setPosition(document.body)" completionHandler:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteForward" argument:nil];
        observer.expectAttachmentUpdates(@[ originalAttachment.get() ], @[ ]);
    }

    EXPECT_FALSE([originalAttachment isEqual:pastedAttachment.get()]);
    EXPECT_TRUE([[originalAttachment info].fileWrapper isEqual:[pastedAttachment info].fileWrapper]);
    EXPECT_TRUE([[originalAttachment info].fileWrapper isEqual:fileWrapper.get()]);
}

TEST(WKAttachmentTests, InsertDuplicateAttachmentAndUpdateData)
{
    auto webView = webViewForTestingAttachments();
    auto originalData = retainPtr(testHTMLData());
    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:originalData.get()]);
    RetainPtr<_WKAttachment> originalAttachment;
    RetainPtr<_WKAttachment> pastedAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        originalAttachment = [webView synchronouslyInsertAttachmentWithFileWrapper:fileWrapper.get() contentType:@"text/plain"];
        EXPECT_EQ(0U, observer.observer().removed.count);
        observer.expectAttachmentUpdates(@[ ], @[ originalAttachment.get() ]);
    }
    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];
    [webView evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        pastedAttachment = observer.observer().inserted.firstObject;
        EXPECT_FALSE([pastedAttachment isEqual:originalAttachment.get()]);
    }
    auto updatedData = retainPtr([@"HELLO WORLD" dataUsingEncoding:NSUTF8StringEncoding]);
    [originalAttachment synchronouslySetData:updatedData.get() newContentType:nil newFilename:nil error:nil];
    [originalAttachment expectRequestedDataToBe:updatedData.get()];
    [pastedAttachment expectRequestedDataToBe:originalData.get()];

    EXPECT_FALSE([originalAttachment isEqual:pastedAttachment.get()]);
    EXPECT_FALSE([[originalAttachment info].fileWrapper isEqual:[pastedAttachment info].fileWrapper]);
    EXPECT_FALSE([[originalAttachment info].fileWrapper isEqual:fileWrapper.get()]);
}

TEST(WKAttachmentTests, InsertAttachmentUsingFileWrapperWithFilePath)
{
    auto webView = webViewForTestingAttachments();
    auto originalFileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testImageFileURL() options:0 error:nil]);
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFileWrapper:originalFileWrapper.get() contentType:nil];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }

    _WKAttachmentInfo *infoBeforeUpdate = [attachment info];
    EXPECT_WK_STREQ("image/png", infoBeforeUpdate.contentType);
    EXPECT_WK_STREQ("icon.png", infoBeforeUpdate.name);
    EXPECT_TRUE([originalFileWrapper isEqual:infoBeforeUpdate.fileWrapper]);
    [attachment expectRequestedDataToBe:testImageData()];

    auto newFileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testPDFFileURL() options:0 error:nil]);
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetFileWrapper:newFileWrapper.get() newContentType:nil error:nil];
        observer.expectAttachmentUpdates(@[ ], @[ ]);
    }

    _WKAttachmentInfo *infoAfterUpdate = [attachment info];
    EXPECT_WK_STREQ("application/pdf", infoAfterUpdate.contentType);
    EXPECT_WK_STREQ("test.pdf", infoAfterUpdate.name);
    EXPECT_TRUE([newFileWrapper isEqual:infoAfterUpdate.fileWrapper]);
    [attachment expectRequestedDataToBe:testPDFData()];
}

TEST(WKAttachmentTests, InvalidateAttachmentsAfterMainFrameNavigation)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> pdfAttachment;
    RetainPtr<_WKAttachment> htmlAttachment;
    {
        ObserveAttachmentUpdatesForScope insertionObserver(webView.get());
        pdfAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"attachment.pdf" contentType:@"application/pdf" data:testPDFData()];
        htmlAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"index.html" contentType:@"text/html" data:testHTMLData()];
        insertionObserver.expectAttachmentUpdates(@[ ], @[ pdfAttachment.get(), htmlAttachment.get() ]);
        EXPECT_TRUE([pdfAttachment isConnected]);
        EXPECT_TRUE([htmlAttachment isConnected]);
    }

    ObserveAttachmentUpdatesForScope removalObserver(webView.get());
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    removalObserver.expectAttachmentUpdates(@[ pdfAttachment.get(), htmlAttachment.get() ], @[ ]);
    EXPECT_FALSE([pdfAttachment isConnected]);
    EXPECT_FALSE([htmlAttachment isConnected]);
    [pdfAttachment expectRequestedDataToBe:nil];
    [htmlAttachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, InvalidateAttachmentsAfterWebProcessTermination)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> pdfAttachment;
    RetainPtr<_WKAttachment> htmlAttachment;
    {
        ObserveAttachmentUpdatesForScope insertionObserver(webView.get());
        pdfAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"attachment.pdf" contentType:@"application/pdf" data:testPDFData()];
        htmlAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"index.html" contentType:@"text/html" data:testHTMLData()];
        insertionObserver.expectAttachmentUpdates(@[ ], @[ pdfAttachment.get(), htmlAttachment.get() ]);
        EXPECT_TRUE([pdfAttachment isConnected]);
        EXPECT_TRUE([htmlAttachment isConnected]);
    }
    {
        ObserveAttachmentUpdatesForScope removalObserver(webView.get());
        [webView stringByEvaluatingJavaScript:@"getSelection().collapseToEnd()"];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        removalObserver.expectAttachmentUpdates(@[ htmlAttachment.get() ], @[ ]);
        EXPECT_TRUE([pdfAttachment isConnected]);
        EXPECT_FALSE([htmlAttachment isConnected]);
        [htmlAttachment expectRequestedDataToBe:testHTMLData()];
    }

    __block bool webProcessTerminated = false;
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
        webProcessTerminated = true;
    }];

    ObserveAttachmentUpdatesForScope observer(webView.get());
    [webView _killWebContentProcess];
    TestWebKitAPI::Util::run(&webProcessTerminated);

    observer.expectAttachmentUpdates(@[ pdfAttachment.get() ], @[ ]);
    EXPECT_FALSE([pdfAttachment isConnected]);
    EXPECT_FALSE([htmlAttachment isConnected]);
    [pdfAttachment expectRequestedDataToBe:nil];
    [htmlAttachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, MoveAttachmentElementAsIconByDragging)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    auto data = retainPtr(testPDFData());
    auto attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"document.pdf" contentType:@"application/pdf" data:data.get()]);

    [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];
    [webView _executeEditCommand:@"InsertHTML" argument:@"<strong>text</strong>" completion:nil];
    [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
    [webView expectElementTag:@"ATTACHMENT" toComeBefore:@"STRONG"];

    // Drag the attachment element to somewhere below the strong text.
    [simulator runFrom:[webView attachmentElementMidPoint] to:CGPointMake(50, 300)];

    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_EQ([simulator insertedAttachments].count, [simulator removedAttachments].count);
#if PLATFORM(MAC)
    EXPECT_FALSE(isCompletelyTransparent([simulator draggingInfo].draggedImage));
#endif

    [webView expectElementTag:@"STRONG" toComeBefore:@"ATTACHMENT"];
    [simulator endDataTransfer];
}

TEST(WKAttachmentTests, PasteWebArchiveContainingImages)
{
    NSData *markupData = [@"<img src='1.png' alt='foo'><div><br></div><img src='2.gif' alt='bar'>" dataUsingEncoding:NSUTF8StringEncoding];

    auto mainResource = adoptNS([[WebResource alloc] initWithData:markupData URL:[NSURL URLWithString:@"foo.html"] MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto pngResource = adoptNS([[WebResource alloc] initWithData:testImageData() URL:[NSURL URLWithString:@"1.png"] MIMEType:@"image/png" textEncodingName:nil frameName:nil]);
    auto gifResource = adoptNS([[WebResource alloc] initWithData:testGIFData() URL:[NSURL URLWithString:@"2.gif"] MIMEType:@"image/gif" textEncodingName:nil frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:@[ pngResource.get(), gifResource.get() ] subframeArchives:@[ ]]);

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[WebArchivePboardType] owner:nil];
    [pasteboard setData:[archive data] forType:WebArchivePboardType];
#else
    UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
    [pasteboard setData:[archive data] forPasteboardType:WebArchivePboardType];
#endif

    RetainPtr<_WKAttachment> gifAttachment;
    RetainPtr<_WKAttachment> pngAttachment;
    auto webView = webViewForTestingAttachments();

    ObserveAttachmentUpdatesForScope observer(webView.get());
    [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    [webView expectElementCount:2 querySelector:@"IMG"];

    for (_WKAttachment *attachment in observer.observer().inserted) {
        if ([attachment.info.contentType isEqualToString:@"image/png"])
            pngAttachment = attachment;
        else if ([attachment.info.contentType isEqualToString:@"image/gif"])
            gifAttachment = attachment;
    }

    EXPECT_WK_STREQ("foo", [pngAttachment info].name);
    EXPECT_WK_STREQ("bar", [gifAttachment info].name);
    [pngAttachment expectRequestedDataToBe:testImageData()];
    [gifAttachment expectRequestedDataToBe:testGIFData()];
    observer.expectAttachmentUpdates(@[ ], @[ pngAttachment.get(), gifAttachment.get() ]);
}

TEST(WKAttachmentTests, ChangeFileWrapperForPastedImage)
{
    platformCopyPNG();
    auto webView = webViewForTestingAttachments();

    ObserveAttachmentUpdatesForScope observer(webView.get());
    [webView paste:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];

    auto attachment = retainPtr(observer.observer().inserted.firstObject);
    auto originalImageData = retainPtr([attachment info].fileWrapper);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"HTMLAttachmentElement.getAttachmentIdentifier(document.querySelector('img'))"]);

    auto newImage = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testGIFData()]);
    [newImage setPreferredFilename:@"foo.gif"];
    [attachment synchronouslySetFileWrapper:newImage.get() newContentType:nil error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(52, 64)];

    [attachment synchronouslySetFileWrapper:originalImageData.get() newContentType:@"image/png" error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];
}

TEST(WKAttachmentTests, AddAttachmentToConnectedImageElement)
{
    auto webView = webViewForTestingAttachments();
    [webView _synchronouslyExecuteEditCommand:@"InsertHTML" argument:@"<img></img>"];

    ObserveAttachmentUpdatesForScope observer(webView.get());
    NSString *attachmentIdentifier = [webView ensureAttachmentForImageElement];
    auto attachment = retainPtr([webView _attachmentForIdentifier:attachmentIdentifier]);
    EXPECT_WK_STREQ(attachmentIdentifier, [attachment uniqueIdentifier]);
    EXPECT_WK_STREQ(attachmentIdentifier, [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);
    observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);

    auto firstImage = adoptNS([[NSFileWrapper alloc] initWithURL:testImageFileURL() options:0 error:nil]);
    auto secondImage = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testGIFData()]);
    [secondImage setPreferredFilename:@"foo.gif"];

    [attachment synchronouslySetFileWrapper:firstImage.get() newContentType:@"image/png" error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];

    [attachment synchronouslySetFileWrapper:secondImage.get() newContentType:(__bridge NSString *)kUTTypeGIF error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(52, 64)];

    [attachment synchronouslySetFileWrapper:firstImage.get() newContentType:nil error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];
}

TEST(WKAttachmentTests, ConnectImageWithAttachmentToDocument)
{
    auto webView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(webView.get());

    NSString *identifier = [webView stringByEvaluatingJavaScript:@"image = document.createElement('img'); HTMLAttachmentElement.getAttachmentIdentifier(image)"];
    auto image = adoptNS([[NSFileWrapper alloc] initWithURL:testImageFileURL() options:0 error:nil]);
    auto attachment = retainPtr([webView _attachmentForIdentifier:identifier]);
    [attachment synchronouslySetFileWrapper:image.get() newContentType:nil error:nil];
    EXPECT_FALSE([attachment isConnected]);
    observer.expectAttachmentUpdates(@[ ], @[ ]);

    [webView evaluateJavaScript:@"document.body.appendChild(image)" completionHandler:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];
    EXPECT_TRUE([attachment isConnected]);
    observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);

    auto newImage = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testGIFData()]);
    [newImage setPreferredFilename:@"test.gif"];
    [attachment synchronouslySetFileWrapper:newImage.get() newContentType:nil error:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(52, 64)];
}

TEST(WKAttachmentTests, CustomFileWrapperSubclass)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    RetainPtr<NSException> exception;
    @try {
        [configuration _setAttachmentFileWrapperClass:[NSArray class]];
    } @catch(NSException *caught) {
        exception = caught;
    }
    EXPECT_TRUE(exception);

    [configuration _setAttachmentFileWrapperClass:[FileWrapper class]];

    auto webView = webViewForTestingAttachments(CGSizeZero, configuration.get());

    ObserveAttachmentUpdatesForScope observer(webView.get());
    platformCopyPNG();
    [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    NSArray<_WKAttachment *> * insertedAttachments = observer.observer().inserted;

    EXPECT_EQ(1U, insertedAttachments.count);
    EXPECT_EQ([FileWrapper class], [insertedAttachments.firstObject.info.fileWrapper class]);
}

TEST(WKAttachmentTests, CopyAndPasteBetweenWebViews)
{
    auto file = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testHTMLData()]);
    [file setPreferredFilename:@"test.foobar"];
    auto image = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testImageData()]);
    auto document = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testPDFData()]);
    auto folder = adoptNS([[NSFileWrapper alloc] initDirectoryWithFileWrappers:@{ @"image.png": image.get(), @"document.pdf": document.get() }]);
    [folder setPreferredFilename:@"folder"];
    auto archive = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testZIPData()]);
    [archive setPreferredFilename:@"archive"];

    @autoreleasepool {
        auto firstWebView = webViewForTestingAttachments();
        [firstWebView synchronouslyInsertAttachmentWithFileWrapper:file.get() contentType:@"application/octet-stream"];
        [firstWebView synchronouslyInsertAttachmentWithFileWrapper:folder.get() contentType:(__bridge NSString *)kUTTypeFolder];
        [firstWebView synchronouslyInsertAttachmentWithFileWrapper:archive.get() contentType:@"application/zip"];
        [firstWebView selectAll:nil];
        [firstWebView _executeEditCommand:@"Copy" argument:nil completion:nil];
    }

    auto secondWebView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(secondWebView.get());
    [secondWebView paste:nil];
    [secondWebView expectElementCount:3 querySelector:@"attachment"];
    EXPECT_EQ(3U, observer.observer().inserted.count);

    NSString *plainFileIdentifier = [secondWebView stringByEvaluatingJavaScript:@"document.querySelector('attachment[title^=test]').uniqueIdentifier"];
    NSString *folderIdentifier = [secondWebView stringByEvaluatingJavaScript:@"document.querySelector('attachment[title=folder]').uniqueIdentifier"];
    NSString *archiveIdentifier = [secondWebView stringByEvaluatingJavaScript:@"document.querySelector('attachment[title=archive]').uniqueIdentifier"];

    _WKAttachmentInfo *pastedFileInfo = [secondWebView _attachmentForIdentifier:plainFileIdentifier].info;
    _WKAttachmentInfo *pastedFolderInfo = [secondWebView _attachmentForIdentifier:folderIdentifier].info;
    _WKAttachmentInfo *pastedArchiveInfo = [secondWebView _attachmentForIdentifier:archiveIdentifier].info;

    NSDictionary<NSString *, NSFileWrapper *> *pastedFolderContents = pastedFolderInfo.fileWrapper.fileWrappers;
    NSFileWrapper *documentFromFolder = [pastedFolderContents objectForKey:@"document.pdf"];
    NSFileWrapper *imageFromFolder = [pastedFolderContents objectForKey:@"image.png"];
    EXPECT_TRUE([[document regularFileContents] isEqualToData:documentFromFolder.regularFileContents]);
    EXPECT_TRUE([[image regularFileContents] isEqualToData:imageFromFolder.regularFileContents]);
    EXPECT_TRUE([[file regularFileContents] isEqualToData:pastedFileInfo.fileWrapper.regularFileContents]);
    EXPECT_TRUE([[archive regularFileContents] isEqualToData:pastedArchiveInfo.fileWrapper.regularFileContents]);
    EXPECT_WK_STREQ("application/octet-stream", pastedFileInfo.contentType);
    EXPECT_WK_STREQ("public.directory", pastedFolderInfo.contentType);
    EXPECT_WK_STREQ("application/zip", pastedArchiveInfo.contentType);
}

TEST(WKAttachmentTests, CopyAndPasteImageBetweenWebViews)
{
    auto image = adoptNS([[NSFileWrapper alloc] initWithURL:testImageFileURL() options:0 error:nil]);
    [image setPreferredFilename:@"icon.png"];
    RetainPtr originalData = [image regularFileContents];

    @autoreleasepool {
        auto sourceView = webViewForTestingAttachments();
        [sourceView _synchronouslyExecuteEditCommand:@"InsertHTML" argument:@"<img></img>"];

        auto attachment = [sourceView _attachmentForIdentifier:[sourceView ensureAttachmentForImageElement]];
        [attachment synchronouslySetFileWrapper:image.get() newContentType:@"image/png" error:nil];
        [sourceView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];

        EXPECT_WK_STREQ("icon.png", [sourceView valueOfAttribute:@"alt" forQuerySelector:@"img"]);
        [sourceView selectAll:nil];
        [sourceView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];
        [sourceView waitForNextPresentationUpdate];
    }

    auto destinationView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(destinationView.get());
    [destinationView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    EXPECT_EQ(1U, observer.observer().inserted.count);

    auto pastedAttachmentInfo = [observer.observer().inserted.firstObject info];
    EXPECT_WK_STREQ("image/png", pastedAttachmentInfo.contentType);
    EXPECT_WK_STREQ("icon.png", pastedAttachmentInfo.name);
    EXPECT_TRUE([originalData isEqualToData:pastedAttachmentInfo.data]);
}

#if HAVE(QUICKLOOK_THUMBNAILING)

static bool triedToLoadThumbnail;
static void _generateBestRepresentationForRequest(id, SEL)
{
    triedToLoadThumbnail = true;
}

#endif

TEST(WKAttachmentTests, CutAndPasteAttachmentBetweenWebViews)
{
    auto webView = webViewForTestingAttachments();
    [webView synchronouslyInsertAttachmentWithFilename:@"test.pages" contentType:nil data:testiWorkAttachmentData()];
    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Cut" argument:nil];

#if HAVE(QUICKLOOK_THUMBNAILING)
    InstanceMethodSwizzler quickLookSwizzler {
        getQLThumbnailGeneratorClass(),
        @selector(generateBestRepresentationForRequest:completionHandler:),
        reinterpret_cast<IMP>(_generateBestRepresentationForRequest)
    };
    triedToLoadThumbnail = false;
#endif

    auto destinationView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(destinationView.get());
    [destinationView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    EXPECT_EQ(1U, observer.observer().inserted.count);

#if HAVE(QUICKLOOK_THUMBNAILING)
    Util::run(&triedToLoadThumbnail);
#endif
}

#if HAVE(QUICKLOOK_THUMBNAILING)

TEST(WKAttachmentTests, iWorkAttachmentWithoutPasteboardActionLoadsThumbnail)
{
    triedToLoadThumbnail = false;
    InstanceMethodSwizzler quickLookSwizzler {
        getQLThumbnailGeneratorClass(),
        @selector(generateBestRepresentationForRequest:completionHandler:),
        reinterpret_cast<IMP>(_generateBestRepresentationForRequest)
    };

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._attachmentElementEnabled = YES;
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[configuration preferences], YES);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 500, 500) configuration:configuration.get()]);

    auto uiDelegate = adoptNS([[AttachmentUIDelegate alloc] init]);
    webView.get().UIDelegate = uiDelegate.get();

    static NSString *htmlWithEmbeddedAttachment = @"<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<script>focus = () => document.body.focus()</script>"
        "<body onload=focus() contenteditable>"
        "<attachment src='test.pages' title='test.pages'></attachment></body>";

    [webView synchronouslyLoadHTMLString:htmlWithEmbeddedAttachment];

    Util::run(&triedToLoadThumbnail);
}

#endif // HAVE(QUICKLOOK_THUMBNAILING)

TEST(WKAttachmentTests, AttachmentIdentifierOfClonedAttachment)
{
    auto webView = webViewForTestingAttachments();
    auto attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"attachment.pdf" contentType:@"application/pdf" data:testPDFData()]);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.body.cloneNode(true).querySelector('attachment').uniqueIdentifier"]);
}

TEST(WKAttachmentTests, CloneImageWithAttachment)
{
    auto webView = webViewForTestingAttachments();
    platformCopyPNG();

    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);

        attachment = observer.observer().inserted.firstObject;
        EXPECT_WK_STREQ("image/png", [attachment info].contentType);
    }

    NSString *clonedAttachmentIdentifier = [webView stringByEvaluatingJavaScript:@"const original = document.querySelector('img');"
        "const clone = original.cloneNode();"
        "original.remove();"
        "document.body.appendChild(clone);"
        "HTMLAttachmentElement.getAttachmentIdentifier(clone);"];

    EXPECT_WK_STREQ([attachment uniqueIdentifier], clonedAttachmentIdentifier);
}

TEST(WKAttachmentTests, DuplicateImageWithAttachment)
{
    platformCopyPNG();
    auto webView = webViewForTestingAttachments();

    RetainPtr originalAttachment = [&] {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        return observer.observer().inserted.firstObject;
    }();

    RetainPtr newAttachment = [&] {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView stringByEvaluatingJavaScript:@"(function() {"
            "    const original = document.querySelector('img');"
            "    const clone = original.cloneNode();"
            "    clone.id = 'clone';"
            "    document.body.appendChild(clone);"
            "})()"];
        [webView waitForNextPresentationUpdate];
        return observer.observer().inserted.firstObject;
    }();

    RetainPtr newAttachmentID = [webView stringByEvaluatingJavaScript:@"document.getElementById('clone').attachmentIdentifier"];
    EXPECT_WK_STREQ([newAttachment uniqueIdentifier], newAttachmentID.get());
    EXPECT_FALSE([[originalAttachment uniqueIdentifier] isEqualToString:newAttachmentID.get()]);

    auto originalInfo = [originalAttachment info];
    auto newInfo = [newAttachment info];
    EXPECT_TRUE([originalInfo.data isEqualToData:newInfo.data]);
    EXPECT_WK_STREQ(originalInfo.contentType, newInfo.contentType);
    EXPECT_WK_STREQ(originalInfo.name, newInfo.name);
}

TEST(WKAttachmentTests, SetFileWrapperForPDFImageAttachment)
{
    auto webView = webViewForTestingAttachments();
    NSString *identifier = [webView stringByEvaluatingJavaScript:@"const i = document.createElement('img'); document.body.appendChild(i); HTMLAttachmentElement.getAttachmentIdentifier(i)"];
    auto attachment = retainPtr([webView _attachmentForIdentifier:identifier]);

    auto pdfFile = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testPDFData()]);
    [attachment setFileWrapper:pdfFile.get() contentType:(__bridge NSString *)kUTTypePDF completion:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(130, 29)];

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto zipFile = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testZIPData()]);
    [attachment setFileWrapper:zipFile.get() contentType:(__bridge NSString *)kUTTypeZipArchive completion:nil];
    EXPECT_FALSE([attachment isConnected]);
}

TEST(WKAttachmentTests, PastingPreservesImageFormat)
{
    auto webView = webViewForTestingAttachments();
    {
        platformCopyPNG();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        _WKAttachment *attachment = observer.observer().inserted[0];
        EXPECT_WK_STREQ("image/png", attachment.info.contentType);
    }
    {
        platformCopyPDF();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        _WKAttachment *attachment = observer.observer().inserted[0];
        EXPECT_WK_STREQ("application/pdf", attachment.info.contentType);
    }
    {
        platformCopyJPEG();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        _WKAttachment *attachment = observer.observer().inserted[0];
        EXPECT_WK_STREQ("image/jpeg", attachment.info.contentType);
    }
}

TEST(WKAttachmentTests, CopyAndPasteRemoteImages)
{
    {
        [TestProtocol registerWithScheme:@"https"];

        auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
        auto sourceView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [sourceView setNavigationDelegate:navigationDelegate.get()];
        [sourceView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/multiple-images.html"]]];
        [navigationDelegate waitForDidFinishNavigation];

        auto pasteboard = [CocoaPasteboard generalPasteboard];
        auto changeCountBeforeCopying = pasteboard.changeCount;

        [sourceView selectAll:nil];
        [sourceView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

        Util::waitForConditionWithLogging([changeCountBeforeCopying, pasteboard] {
            return changeCountBeforeCopying != pasteboard.changeCount;
        }, 2, @"Expected %@ to change.", pasteboard);
    }
    {
        auto webView = webViewForTestingAttachments();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        [webView expectElementCount:5 querySelector:@"IMG"];
        EXPECT_EQ(5U, observer.observer().inserted.count);
        Vector attachmentInfo { {
            RetainPtr { [observer.observer().inserted[0] info] },
            RetainPtr { [observer.observer().inserted[1] info] },
            RetainPtr { [observer.observer().inserted[2] info] },
            RetainPtr { [observer.observer().inserted[3] info] },
            RetainPtr { [observer.observer().inserted[4] info] },
        } };

        std::sort(attachmentInfo.begin(), attachmentInfo.end(), [] (auto& a, auto& b) {
            return [[a name] compare:[b name]] == NSOrderedAscending;
        });

        EXPECT_WK_STREQ("400x400-green.png", [attachmentInfo[0] name]);
        EXPECT_WK_STREQ("image/png", [attachmentInfo[0] contentType]);
        EXPECT_WK_STREQ("large-red-square.png", [attachmentInfo[1] name]);
        EXPECT_WK_STREQ("image/png", [attachmentInfo[1] contentType]);
        EXPECT_WK_STREQ("sunset-in-cupertino-100px.tiff", [attachmentInfo[2] name]);
        EXPECT_WK_STREQ("image/tiff", [attachmentInfo[2] contentType]);
        EXPECT_WK_STREQ("sunset-in-cupertino-200px.png", [attachmentInfo[3] name]);
        EXPECT_WK_STREQ("image/png", [attachmentInfo[3] contentType]);
        EXPECT_WK_STREQ("test.jpg", [attachmentInfo[4] name]);
        EXPECT_WK_STREQ("image/jpeg", [attachmentInfo[4] contentType]);
    }
}

TEST(WKAttachmentTests, CreateAttachmentsFromExistingImage)
{
    auto webView = webViewForTestingAttachments();
    NSString *asyncScript = @"return new Promise(resolve => {"
        "const image = document.createElement('img');"
        "image.addEventListener('load', () => resolve(HTMLAttachmentElement.getAttachmentIdentifier(image)), { once: true });"
        "image.src = 'icon.png';"
        "document.body.appendChild(image);"
        "})";

    __block RetainPtr<NSString> attachmentIdentifier;
    __block bool doneWithScript = false;
    [webView callAsyncJavaScript:asyncScript arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(NSString *result, NSError *error) {
        EXPECT_NULL(error);
        attachmentIdentifier = result;
        doneWithScript = true;
    }];
    Util::run(&doneWithScript);

    EXPECT_NOT_NULL(attachmentIdentifier);
    RetainPtr info = [[webView _attachmentForIdentifier:attachmentIdentifier.get()] info];
    EXPECT_WK_STREQ("image/png", [info contentType]);
    EXPECT_WK_STREQ("icon.png", [info name]);
    EXPECT_TRUE([[info data] isEqualToData:testImageData()]);
}

TEST(WKAttachmentTests, UserDragNonePreventsDragOnAttachmentElement)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto styleSheet = adoptNS([[_WKUserStyleSheet alloc] initWithSource:@"attachment { -webkit-user-drag: none; }" forMainFrameOnly:YES]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet.get()];
    [configuration _setAttachmentElementEnabled:YES];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    auto file = adoptNS([[NSFileWrapper alloc] initWithURL:testPDFFileURL() options:0 error:nil]);
    [webView synchronouslyInsertAttachmentWithFileWrapper:file.get() contentType:nil];
    [simulator runFrom:NSMakePoint(15, 15) to:NSMakePoint(50, 50)];

#if PLATFORM(MAC)
    EXPECT_NULL([simulator draggingInfo]);
#else
    EXPECT_EQ(0U, [simulator sourceItemProviders].count);
#endif
}

#pragma mark - Platform-specific tests

#if PLATFORM(MAC)

TEST(WKAttachmentTestsMac, DraggingAttachmentBackedImagePreservesRangedSelection)
{
    platformCopyPNG();

    RetainPtr<_WKAttachment> attachment;
    auto webView = webViewForTestingAttachments(NSMakeSize(250, 250));
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }

    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];

    [[webView window] orderFrontRegardless];
    [webView mouseEnterAtPoint:CGPointMake(125, 125)];
    [webView mouseMoveToPoint:CGPointMake(125, 125) withFlags:0];
    [webView mouseDownAtPoint:CGPointMake(125, 125) simulatePressure:NO];
    [webView waitForPendingMouseEvents];

    [webView mouseDragToPoint:CGPointMake(150, 150)];
    [webView mouseDragToPoint:CGPointMake(200, 150)];
    [webView waitForPendingMouseEvents];

    [webView mouseUpAtPoint:CGPointMake(200, 150)];
    [webView waitForPendingMouseEvents];

    EXPECT_WK_STREQ("Range", [webView stringByEvaluatingJavaScript:@"getSelection().type"]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"getSelection().containsNode(document.querySelector('img'))"] boolValue]);
}

TEST(WKAttachmentTestsMac, InsertPastedFileURLsAsAttachments)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[testPDFFileURL().path, testImageFileURL().path] forType:NSFilenamesPboardType];

    RetainPtr<NSArray<_WKAttachment *>> insertedAttachments;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        insertedAttachments = [observer.observer() inserted];
        EXPECT_EQ(2U, [insertedAttachments count]);
    }

    [webView expectElementCount:1 querySelector:@"ATTACHMENT"];
    [webView expectElementCount:1 querySelector:@"IMG"];
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('type')"]);
    EXPECT_WK_STREQ("test.pdf", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('title')"]);

    NSString *imageAttachmentIdentifier = [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"];
    if ([testImageData() isEqualToData:[insertedAttachments firstObject].info.data])
        EXPECT_WK_STREQ([insertedAttachments firstObject].uniqueIdentifier, imageAttachmentIdentifier);
    else
        EXPECT_WK_STREQ([insertedAttachments lastObject].uniqueIdentifier, imageAttachmentIdentifier);

    for (_WKAttachment *attachment in insertedAttachments.get())
        EXPECT_GT(attachment.info.filePath.length, 0U);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        NSArray<_WKAttachment *> *removedAttachments = [observer.observer() removed];
        EXPECT_EQ(2U, removedAttachments.count);
        EXPECT_TRUE([removedAttachments containsObject:[insertedAttachments firstObject]]);
        EXPECT_TRUE([removedAttachments containsObject:[insertedAttachments lastObject]]);
    }
}

TEST(WKAttachmentTestsMac, InsertNonExistentImageFileAsAttachment)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];

    RetainPtr filePath = nonexistentFilePath(@"jpg");
    [pasteboard setPropertyList:@[filePath.get()] forType:NSFilenamesPboardType];

    RetainPtr<NSArray<_WKAttachment *>> insertedAttachments;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        insertedAttachments = [observer.observer() inserted];
        EXPECT_EQ(1U, [insertedAttachments count]);
    }

    [webView expectElementCount:1 querySelector:@"ATTACHMENT"];
    [webView expectElementCount:0 querySelector:@"IMG"];
    EXPECT_WK_STREQ("0", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('progress')"]);
    EXPECT_WK_STREQ([filePath lastPathComponent], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('title')"]);
    EXPECT_WK_STREQ("image/jpeg", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('type')"]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"document.querySelector('attachment').hasAttribute('subtitle')"] boolValue]);
}

TEST(WKAttachmentTestsMac, InsertDroppedFilePromisesAsAttachments)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];
    [simulator writePromisedFiles:@[ testPDFFileURL(), testImageFileURL() ]];

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];

    [webView expectElementCount:1 querySelector:@"ATTACHMENT"];
    [webView expectElementCount:1 querySelector:@"IMG"];
    EXPECT_EQ(2U, [simulator insertedAttachments].count);

    auto insertedAttachments = retainPtr([simulator insertedAttachments]);
    NSArray<NSData *> *expectedData = @[ testPDFData(), testImageData() ];
    for (_WKAttachment *attachment in insertedAttachments.get()) {
        EXPECT_GT(attachment.info.filePath.length, 0U);
        EXPECT_TRUE([expectedData containsObject:attachment.info.data]);
        if ([testPDFData() isEqualToData:attachment.info.data])
            EXPECT_WK_STREQ("application/pdf", attachment.info.contentType);
        else if ([testImageData() isEqualToData:attachment.info.data]) {
            EXPECT_WK_STREQ("image/png", attachment.info.contentType);
            EXPECT_WK_STREQ(attachment.uniqueIdentifier, [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);
        }
    }

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
    auto removedAttachments = retainPtr([simulator removedAttachments]);
    EXPECT_EQ(2U, [removedAttachments count]);
    [webView expectElementCount:0 querySelector:@"ATTACHMENT"];
    [webView expectElementCount:0 querySelector:@"IMG"];
    EXPECT_TRUE([removedAttachments containsObject:[insertedAttachments firstObject]]);
    EXPECT_TRUE([removedAttachments containsObject:[insertedAttachments lastObject]]);
}

TEST(WKAttachmentTestsMac, DragAttachmentAsFilePromise)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testPDFFileURL() options:0 error:nil]);
    auto attachment = retainPtr([webView synchronouslyInsertAttachmentWithFileWrapper:fileWrapper.get() contentType:nil]);
    [simulator runFrom:[webView attachmentElementMidPoint] to:CGPointMake(300, 300)];

    NSArray<NSURL *> *urls = [simulator receivePromisedFiles];
    EXPECT_EQ(1U, urls.count);
    EXPECT_WK_STREQ("test.pdf", urls.lastObject.lastPathComponent);
    EXPECT_TRUE([[NSData dataWithContentsOfURL:urls.firstObject] isEqualToData:testPDFData()]);
    EXPECT_FALSE(isCompletelyTransparent([simulator draggingInfo].draggedImage));
}

TEST(WKAttachmentTestsMac, DragAttachmentWithNoTypeShouldNotCrash)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    [webView stringByEvaluatingJavaScript:@"document.body.appendChild(document.createElement('attachment')); 0"];

    [simulator runFrom:[webView attachmentElementMidPoint] to:CGPointMake(300, 300)];

    NSArray<NSURL *> *urls = [simulator receivePromisedFiles];
    EXPECT_EQ(0U, urls.count);
}

TEST(WKAttachmentTestsMac, DropImageOverImageWithControls)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    [configuration _setImageControlsEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    platformCopyPNG();
    [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];

    [simulator writePromisedFiles:@[ testImageFileURL() ]];
    [simulator runFrom:NSMakePoint(400, 400) to:NSMakePoint(50, 50)];
    [webView waitForImageElementSizeToBecome:CGSizeMake(215, 174)];
    [webView expectElementCount:2 querySelector:@"IMG"];
}

static bool didLoadIcon;
static NSImage *_icon(id, SEL)
{
    didLoadIcon = true;
    return nil;
}

TEST(WKAttachmentTestsMac, DragDirectoryAttachment)
{
    didLoadIcon = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    InstanceMethodSwizzler fileWrapperSwizzler {
        [NSFileWrapper class],
        @selector(icon),
        reinterpret_cast<IMP>(_icon)
    };

    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testDirectoryAttachmentFileURL() options:0 error:nil]);
    auto attachment = retainPtr([webView synchronouslyInsertAttachmentWithFileWrapper:fileWrapper.get() contentType:nil]);
    [simulator runFrom:[webView attachmentElementMidPoint] to:CGPointMake(300, 300)];
    TestWebKitAPI::Util::run(&didLoadIcon);
}

TEST(WKAttachmentTestsMac, CallAcceptsFirstMouseWhileDraggingAttachment)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:attachmentEditingTestMarkup];

    auto fileWrapper = adoptNS([[NSFileWrapper alloc] initWithURL:testPDFFileURL() options:0 error:nil]);
    RetainPtr attachment = [webView synchronouslyInsertAttachmentWithFileWrapper:fileWrapper.get() contentType:@"application/pdf"];
    [simulator setWillBeginDraggingHandler:^{
        [webView acceptsFirstMouseAtPoint:NSMakePoint(100, 100)];
    }];
    [simulator runFrom:webView.attachmentElementMidPoint to:CGPointMake(300, 300)];

    auto pasteboard = [simulator draggingInfo].draggingPasteboard;
    EXPECT_WK_STREQ("com.adobe.pdf", [pasteboard stringForType:(__bridge NSString *)kPasteboardTypeFilePromiseContent]);
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

static RetainPtr<UITargetedDragPreview> targetedImageDragPreview(WKWebView *webView, NSData *imageData, CGSize size)
{
    auto imageView = adoptNS([[UIImageView alloc] initWithImage:[UIImage imageWithData:imageData]]);
    [imageView setBounds:CGRectMake(0, 0, size.width, size.height)];
    auto defaultDropTarget = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:webView center:CGPointMake(450, 450)]);
    auto parameters = adoptNS([[UIDragPreviewParameters alloc] init]);
    return adoptNS([[UITargetedDragPreview alloc] initWithView:imageView.get() parameters:parameters.get() target:defaultDropTarget.get()]);
}

TEST(WKAttachmentTestsIOS, TargetedPreviewsWhenDroppingImages)
{
    auto webView = webViewForTestingAttachments();
    [webView _setEditable:YES];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    // The first item preview should be scaled down by a factor of 2.
    auto firstPreview = targetedImageDragPreview(webView.get(), testImageData(), CGSizeMake(430, 348));
    auto firstItem = adoptNS([[NSItemProvider alloc] init]);
    [firstItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePNG withData:testImageData() loadingDelay:0.5];
    [firstItem setPreferredPresentationSize:CGSizeMake(215, 174)];
    [firstItem setSuggestedName:@"icon"];

    // The second item preview should be scaled up by a factor of 2.
    auto secondPreview = targetedImageDragPreview(webView.get(), testGIFData(), CGSizeMake(26, 32));
    auto secondItem = adoptNS([[NSItemProvider alloc] init]);
    [secondItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeGIF withData:testGIFData() loadingDelay:0.5];
    [secondItem setPreferredPresentationSize:CGSizeMake(52, 64)];
    [secondItem setSuggestedName:@"apple"];

    [simulator setDropAnimationTiming:DropAnimationShouldFinishBeforeHandlingDrop];
    [simulator setExternalItemProviders:@[firstItem.get(), secondItem.get()] defaultDropPreviews:@[firstPreview.get(), secondPreview.get()]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(450, 450)];

    EXPECT_EQ([simulator delayedDropPreviews].count, 2U);
    UITargetedDragPreview *firstDelayedPreview = [simulator delayedDropPreviews].firstObject;
    UITargetedDragPreview *secondDelayedPreview = [simulator delayedDropPreviews].lastObject;
    auto imageElementBounds = retainPtr([webView allBoundingClientRects:@"IMG"]);

    EXPECT_TRUE(CGAffineTransformEqualToTransform(CGAffineTransformMakeScale(0.5, 0.5), firstDelayedPreview.target.transform));
    EXPECT_TRUE(CGRectEqualToRect(CGRectMake(0, 0, 430, 348), firstDelayedPreview.parameters.visiblePath.bounds));
    EXPECT_TRUE(CGRectContainsPoint([imageElementBounds firstObject].CGRectValue, firstDelayedPreview.target.center));

    EXPECT_TRUE(CGAffineTransformEqualToTransform(CGAffineTransformMakeScale(2, 2), secondDelayedPreview.target.transform));
    EXPECT_TRUE(CGRectEqualToRect(CGRectMake(0, 0, 26, 32), secondDelayedPreview.parameters.visiblePath.bounds));
    EXPECT_TRUE(CGRectContainsPoint([imageElementBounds lastObject].CGRectValue, secondDelayedPreview.target.center));

    [webView expectElementCount:2 querySelector:@"IMG"];
    _WKAttachment *pngAttachment = [[simulator insertedAttachments] _attachmentWithName:@"icon.png"];
    _WKAttachment *gifAttachment = [[simulator insertedAttachments] _attachmentWithName:@"apple.gif"];
    EXPECT_WK_STREQ(pngAttachment.info.contentType, @"image/png");
    EXPECT_WK_STREQ(gifAttachment.info.contentType, @"image/gif");
}

TEST(WKAttachmentTestsIOS, TargetedPreviewIsClippedWhenDroppingTallImage)
{
    auto webView = webViewForTestingAttachments(CGSizeMake(800, 200));
    [webView stringByEvaluatingJavaScript:@"document.body.style.margin = '0'"];
    [webView _setEditable:YES];

    auto imageData = retainPtr([NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto preview = targetedImageDragPreview(webView.get(), imageData.get(), CGSizeMake(100, 100));
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePNG withData:imageData.get() loadingDelay:0.5];
    [item setPreferredPresentationSize:CGSizeMake(400, 400)];
    [item setSuggestedName:@"green"];

    [simulator setDropAnimationTiming:DropAnimationShouldFinishBeforeHandlingDrop];
    [simulator setExternalItemProviders:@[item.get()] defaultDropPreviews:@[preview.get()]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(350, 350)];

    EXPECT_EQ([simulator delayedDropPreviews].count, 1U);
    UITargetedDragPreview *delayedPreview = [simulator delayedDropPreviews].firstObject;
    EXPECT_TRUE(CGAffineTransformEqualToTransform(CGAffineTransformMakeScale(4, 4), delayedPreview.target.transform));
    EXPECT_TRUE(CGRectEqualToRect(CGRectMake(0, 0, 100, 50), delayedPreview.parameters.visiblePath.bounds));
    EXPECT_TRUE(CGPointEqualToPoint(CGPointMake(200, 100), delayedPreview.target.center));

    [webView expectElementCount:1 querySelector:@"IMG"];
    _WKAttachment *attachment = [[simulator insertedAttachments] _attachmentWithName:@"green.png"];
    EXPECT_WK_STREQ(attachment.info.contentType, @"image/png");
}

TEST(WKAttachmentTestsIOS, InsertDroppedImageAsAttachment)
{
    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerData:testImageData() type:(__bridge NSString *)kUTTypePNG];
    [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [dragAndDropSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [dragAndDropSimulator removedAttachments].count);
    auto attachment = retainPtr([dragAndDropSimulator insertedAttachments].firstObject);
    [attachment expectRequestedDataToBe:testImageData()];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
}

TEST(WKAttachmentTestsIOS, InsertDroppedImageWithPreferredPresentationSize)
{
    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerData:testImageData() type:(__bridge NSString *)kUTTypePNG];
    [item setPreferredPresentationSize:CGSizeMake(200, 100)];
    [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    CGSize imageElementSize = [webView imageElementSize];
    EXPECT_EQ(200, imageElementSize.width);
    EXPECT_EQ(100, imageElementSize.height);
}

TEST(WKAttachmentTestsIOS, InsertDroppedAttributedStringContainingAttachment)
{
    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(__bridge NSString *)kUTTypePNG]);
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:[NSAttributedString attributedStringWithAttachment:image.get()] visibility:NSItemProviderRepresentationVisibilityAll];

    [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [dragAndDropSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [dragAndDropSimulator removedAttachments].count);
    auto attachment = retainPtr([dragAndDropSimulator insertedAttachments].firstObject);

    auto size = platformImageWithData([attachment info].data).size;
    EXPECT_EQ(215., size.width);
    EXPECT_EQ(174., size.height);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[ attachment.get() ], @[ ]);
    }
}

TEST(WKAttachmentTestsIOS, InsertDroppedRichAndPlainTextFilesAsAttachments)
{
    // Here, both rich text and plain text are content types that WebKit already understands how to insert in editable
    // areas in the absence of attachment elements. However, due to the explicitly set attachment presentation style
    // on the item providers, we should instead treat them as dropped files and insert attachment elements.
    // This exercises the scenario of dragging rich and plain text files from Files to Mail.
    auto richTextItem = adoptNS([[NSItemProvider alloc] init]);
    auto richText = adoptNS([[NSAttributedString alloc] initWithString:@"Hello world" attributes:@{ NSFontAttributeName: [UIFont boldSystemFontOfSize:12] }]);
    [richTextItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [richTextItem registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [richTextItem setSuggestedName:@"hello.rtf"];

    auto plainTextItem = adoptNS([[NSItemProvider alloc] init]);
    [plainTextItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [plainTextItem registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];
    [plainTextItem setSuggestedName:@"world.txt"];

    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [dragAndDropSimulator setExternalItemProviders:@[ richTextItem.get(), plainTextItem.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(2U, [dragAndDropSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [dragAndDropSimulator removedAttachments].count);

    for (_WKAttachment *attachment in [dragAndDropSimulator insertedAttachments])
        EXPECT_GT([attachment info].data.length, 0U);

    [webView expectElementCount:2 querySelector:@"ATTACHMENT"];
    EXPECT_WK_STREQ("hello.rtf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('title')"]);
    EXPECT_WK_STREQ((__bridge NSString *)kUTTypeFlatRTFD, [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);
    EXPECT_WK_STREQ("world.txt", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('title')"]);
    auto contentType = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"];
    EXPECT_TRUE([contentType isEqualToString:(__bridge NSString *)kUTTypeUTF8PlainText] || [contentType containsString:@"text/plain"]);
}

TEST(WKAttachmentTestsIOS, InsertDroppedZipArchiveAsAttachment)
{
    // Since WebKit doesn't have any default DOM representation for ZIP archives, we should fall back to inserting
    // attachment elements. This exercises the flow of dragging a ZIP file from an app that doesn't specify a preferred
    // presentation style (e.g. Notes) into Mail.
    auto item = adoptNS([[NSItemProvider alloc] init]);
    NSData *data = testZIPData();
    [item registerData:data type:(__bridge NSString *)kUTTypeZipArchive];
    [item setSuggestedName:@"archive.zip"];

    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [dragAndDropSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [dragAndDropSimulator removedAttachments].count);
    [[dragAndDropSimulator insertedAttachments].firstObject expectRequestedDataToBe:data];
    [webView expectElementCount:1 querySelector:@"ATTACHMENT"];
    EXPECT_WK_STREQ("archive.zip", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/zip", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTestsIOS, InsertDroppedItemProvidersInOrder)
{
    // Tests that item providers are inserted in the order they are specified. In this case, the two inserted attachments
    // should be separated by a link.
    auto firstAttachmentItem = adoptNS([[NSItemProvider alloc] init]);
    [firstAttachmentItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [firstAttachmentItem registerObject:@"FIRST" visibility:NSItemProviderRepresentationVisibilityAll];
    [firstAttachmentItem setSuggestedName:@"first.txt"];

    auto inlineTextItem = adoptNS([[NSItemProvider alloc] init]);
    auto appleURL = retainPtr([NSURL URLWithString:@"https://www.apple.com/"]);
    [inlineTextItem registerObject:appleURL.get() visibility:NSItemProviderRepresentationVisibilityAll];

    auto secondAttachmentItem = adoptNS([[NSItemProvider alloc] init]);
    [secondAttachmentItem registerData:testPDFData() type:(__bridge NSString *)kUTTypePDF];
    [secondAttachmentItem setSuggestedName:@"second.pdf"];

    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [dragAndDropSimulator setExternalItemProviders:@[ firstAttachmentItem.get(), inlineTextItem.get(), secondAttachmentItem.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(2U, [dragAndDropSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [dragAndDropSimulator removedAttachments].count);

    for (_WKAttachment *attachment in [dragAndDropSimulator insertedAttachments])
        EXPECT_GT([attachment info].data.length, 0U);

    [webView expectElementTagsInOrder:@[ @"ATTACHMENT", @"A", @"ATTACHMENT" ]];

    EXPECT_WK_STREQ("first.txt", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('title')"]);
    auto contentType = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"];
    EXPECT_TRUE([contentType isEqualToString:(__bridge NSString *)kUTTypeUTF8PlainText] || [contentType containsString:@"text/plain"]);
    EXPECT_WK_STREQ([appleURL absoluteString], [webView valueOfAttribute:@"href" forQuerySelector:@"a"]);
    EXPECT_WK_STREQ("second.pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('title')"]);
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"]);
}

TEST(WKAttachmentTestsIOS, DragAttachmentInsertedAsFile)
{
    auto item = adoptNS([[NSItemProvider alloc] init]);
    auto data = retainPtr(testPDFData());
    [item registerData:data.get() type:(__bridge NSString *)kUTTypePDF];
    [item setSuggestedName:@"document.pdf"];

    auto webView = webViewForTestingAttachments();
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
    [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    // First, verify that the attachment was successfully dropped.
    EXPECT_EQ(1U, [dragAndDropSimulator insertedAttachments].count);
    _WKAttachment *attachment = [dragAndDropSimulator insertedAttachments].firstObject;
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    [dragAndDropSimulator setExternalItemProviders:@[ ]];
    [dragAndDropSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(-100, -100)];

    // Next, verify that dragging the attachment produces an item provider with a PDF attachment.
    EXPECT_EQ(1U, [dragAndDropSimulator sourceItemProviders].count);
    NSItemProvider *itemProvider = [dragAndDropSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    [itemProvider expectType:(__bridge NSString *)kUTTypePDF withData:data.get()];
    EXPECT_WK_STREQ("document.pdf", [itemProvider suggestedName]);
    [dragAndDropSimulator endDataTransfer];
}

TEST(WKAttachmentTestsIOS, DragAttachmentInsertedAsData)
{
    auto webView = webViewForTestingAttachments();
    auto data = retainPtr(testPDFData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"document.pdf" contentType:@"application/pdf" data:data.get()];
        observer.expectAttachmentUpdates(@[ ], @[ attachment.get() ]);
    }

    // First, verify that the attachment was successfully inserted from raw data.
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [dragAndDropSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(-100, -100)];

    // Next, verify that dragging the attachment produces an item provider with a PDF attachment.
    EXPECT_EQ(1U, [dragAndDropSimulator sourceItemProviders].count);
    NSItemProvider *itemProvider = [dragAndDropSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    [itemProvider expectType:(__bridge NSString *)kUTTypePDF withData:data.get()];
    EXPECT_WK_STREQ("document.pdf", [itemProvider suggestedName]);
    [dragAndDropSimulator endDataTransfer];
}

static RetainPtr<NSItemProvider> mapItemForTesting()
{
    auto placemark = adoptNS([allocMKPlacemarkInstance() initWithCoordinate:CLLocationCoordinate2DMake(37.3327, -122.0053)]);
    auto mapItem = adoptNS([allocMKMapItemInstance() initWithPlacemark:placemark.get()]);
    [mapItem setName:@"Apple Park.vcf"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:mapItem.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider setSuggestedName:[mapItem name]];
    return itemProvider;
}

static RetainPtr<NSItemProvider> contactItemForTesting()
{
    auto contact = adoptNS([allocCNMutableContactInstance() init]);
    [contact setGivenName:@"Foo"];
    [contact setFamilyName:@"Bar"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:contact.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider setSuggestedName:@"Foo Bar.vcf"];
    return itemProvider;
}

TEST(WKAttachmentTestsIOS, InsertDroppedMapItemAsAttachment)
{
    auto itemProvider = mapItemForTesting();
    auto webView = webViewForTestingAttachments();
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(25, 25) to:CGPointMake(100, 100)];

    NSURL *droppedLinkURL = [NSURL URLWithString:[webView valueOfAttribute:@"href" forQuerySelector:@"a"]];
    [webView expectElementTag:@"A" toComeBefore:@"ATTACHMENT"];
    EXPECT_WK_STREQ("maps.apple.com", droppedLinkURL.host);
    EXPECT_WK_STREQ("Apple Park.vcf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("text/vcard", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_EQ(1U, [simulator insertedAttachments].count);
    _WKAttachmentInfo *info = [simulator insertedAttachments].firstObject.info;
    EXPECT_WK_STREQ("Apple Park.vcf", info.name);
    EXPECT_WK_STREQ("text/vcard", info.contentType);
}

TEST(WKAttachmentTestsIOS, InsertDroppedContactAsAttachment)
{
    auto itemProvider = contactItemForTesting();
    auto webView = webViewForTestingAttachments();
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(25, 25) to:CGPointMake(100, 100)];

    [webView expectElementCount:0 querySelector:@"a"];
    EXPECT_WK_STREQ("Foo Bar.vcf", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').title"]);
    EXPECT_WK_STREQ("text/vcard", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_EQ(1U, [simulator insertedAttachments].count);
    _WKAttachmentInfo *info = [simulator insertedAttachments].firstObject.info;
    EXPECT_WK_STREQ("Foo Bar.vcf", info.name);
    EXPECT_WK_STREQ("text/vcard", info.contentType);
}

TEST(WKAttachmentTestsIOS, InsertPastedContactAsAttachment)
{
    UIPasteboard.generalPasteboard.itemProviders = @[ contactItemForTesting().get() ];
    auto webView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(webView.get());
    [webView paste:nil];

    [webView expectElementCount:0 querySelector:@"a"];
    EXPECT_WK_STREQ("Foo Bar.vcf", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').title"]);
    EXPECT_WK_STREQ("text/vcard", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_EQ(1U, observer.observer().inserted.count);
    _WKAttachment *attachment = observer.observer().inserted.firstObject;
    EXPECT_WK_STREQ("Foo Bar.vcf", attachment.info.name);
    EXPECT_WK_STREQ("text/vcard", attachment.info.contentType);
}

TEST(WKAttachmentTestsIOS, InsertPastedMapItemAsAttachment)
{
    UIApplicationInitialize();
    UIPasteboard.generalPasteboard.itemProviders = @[ mapItemForTesting().get() ];
    auto webView = webViewForTestingAttachments();
    ObserveAttachmentUpdatesForScope observer(webView.get());
    [webView paste:nil];

    NSURL *pastedLinkURL = [NSURL URLWithString:[webView valueOfAttribute:@"href" forQuerySelector:@"a"]];
    [webView expectElementTag:@"A" toComeBefore:@"ATTACHMENT"];
    EXPECT_WK_STREQ("maps.apple.com", pastedLinkURL.host);
    EXPECT_WK_STREQ("Apple Park.vcf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("text/vcard", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_EQ(1U, observer.observer().inserted.count);
    _WKAttachment *attachment = observer.observer().inserted.firstObject;
    EXPECT_WK_STREQ("Apple Park.vcf", attachment.info.name);
    EXPECT_WK_STREQ("text/vcard", attachment.info.contentType);
}

TEST(WKAttachmentTestsIOS, InsertPastedFilesAsAttachments)
{
    auto pdfItem = adoptNS([[NSItemProvider alloc] init]);
    [pdfItem setSuggestedName:@"doc"];
    [pdfItem registerData:testPDFData() type:(__bridge NSString *)kUTTypePDF];

    auto textItem = adoptNS([[NSItemProvider alloc] init]);
    [textItem setSuggestedName:@"hello"];
    [textItem registerData:[@"helloworld" dataUsingEncoding:NSUTF8StringEncoding] type:(__bridge NSString *)kUTTypePlainText];

    UIPasteboard.generalPasteboard.itemProviders = @[ pdfItem.get(), textItem.get() ];

    RetainPtr<_WKAttachment> textAttachment;
    RetainPtr<_WKAttachment> pdfAttachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(2U, observer.observer().inserted.count);
        _WKAttachment *firstAttachment = observer.observer().inserted.firstObject;
        if ([firstAttachment.info.contentType isEqualToString:@"text/plain"]) {
            textAttachment = firstAttachment;
            pdfAttachment = observer.observer().inserted.lastObject;
        } else {
            EXPECT_WK_STREQ(firstAttachment.info.contentType, @"application/pdf");
            textAttachment = observer.observer().inserted.lastObject;
            pdfAttachment = firstAttachment;
        }
        observer.expectAttachmentUpdates(@[ ], @[ pdfAttachment.get(), textAttachment.get() ]);
    }

    [webView expectElementCount:2 querySelector:@"attachment"];
    EXPECT_WK_STREQ("doc", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment[type^=application]"]);
    EXPECT_WK_STREQ("doc", [pdfAttachment info].name);
    EXPECT_WK_STREQ("hello", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment[type^=text]"]);
    EXPECT_WK_STREQ("hello", [textAttachment info].name);
    [pdfAttachment expectRequestedDataToBe:testPDFData()];
    [textAttachment expectRequestedDataToBe:[@"helloworld" dataUsingEncoding:NSUTF8StringEncoding]];
    EXPECT_TRUE([webView canPerformAction:@selector(paste:) withSender:nil]);
}

TEST(WKAttachmentTestsIOS, InsertDroppedImageWithNonImageFileExtension)
{
    runTestWithTemporaryImageFile(@"image.hello", ^(NSURL *fileURL) {
        auto item = adoptNS([[NSItemProvider alloc] init]);
        [item setSuggestedName:@"image.hello"];
        [item registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePNG fileOptions:NSItemProviderFileOptionOpenInPlace visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(void (^callback)(NSURL *, BOOL, NSError *))
        {
            callback(fileURL, YES, nil);
            return nil;
        }];

        auto webView = webViewForTestingAttachments();
        auto dragAndDropSimulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
        [dragAndDropSimulator setExternalItemProviders:@[ item.get() ]];
        [dragAndDropSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

        EXPECT_EQ(1U, [dragAndDropSimulator insertedAttachments].count);
        _WKAttachment *attachment = [dragAndDropSimulator insertedAttachments].firstObject;
        _WKAttachmentInfo *info = attachment.info;
        EXPECT_WK_STREQ("image/png", info.contentType);
        EXPECT_WK_STREQ("image.hello.png", info.filePath.lastPathComponent);
        EXPECT_WK_STREQ("image.hello.png", info.name);
        [webView expectElementCount:1 querySelector:@"IMG"];
    });
}

TEST(WKAttachmentTestsIOS, CopyAttachmentUsingElementAction)
{
    UIPasteboard.generalPasteboard.items = @[ ];

    auto webView = webViewForTestingAttachments();
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<body></body><script>document.body.focus();</script>"];

    auto document = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:testPDFData()]);
    [document setPreferredFilename:@"hello.pdf"];

    auto attachment = retainPtr([webView synchronouslyInsertAttachmentWithFileWrapper:document.get() contentType:(__bridge NSString *)kUTTypePDF]);
    NSString *identifier = [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"];
    EXPECT_WK_STREQ(identifier, [attachment uniqueIdentifier]);

    // Ensure that we can hit-test to the attachment element when simulating a context menu element by adding a click event handler.
    [webView objectByEvaluatingJavaScript:@"document.querySelector('attachment').addEventListener('click', () => { })"];
    [webView _setEditable:NO];

    [webView _simulateElementAction:_WKElementActionTypeCopy atLocation:CGPointMake(20, 20)];

    // It takes two IPC round trips between the UI process and web process until the pasteboard data is written,
    // since we first need to hit-test to discover the activated element, and then use the activated element to
    // simulate the "copy" action.
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    NSArray<NSItemProvider *> *itemProviders = UIPasteboard.generalPasteboard.itemProviders;
    EXPECT_EQ(1U, itemProviders.count);

    NSItemProvider *itemProvider = itemProviders.firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    EXPECT_WK_STREQ("hello.pdf", itemProvider.suggestedName);

    __block bool done = false;
    [itemProvider loadDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePDF completionHandler:^(NSData *data, NSError *) {
        EXPECT_TRUE([[document serializedRepresentation] isEqualToData:data]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS)

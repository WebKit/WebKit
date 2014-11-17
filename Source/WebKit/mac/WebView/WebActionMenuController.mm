/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WebActionMenuController.h"

#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebDocumentInternal.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebSystemInterface.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <ImageIO/ImageIO.h>
#import <ImageKit/ImageKit.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editor.h>
#import <WebCore/Element.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSSharingServiceSPI.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/Page.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextCheckerClient.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-class.h>
#import <objc/objc.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, QuickLookUI)
SOFT_LINK_CLASS(QuickLookUI, QLPreviewBubble)

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, ImageKit)
SOFT_LINK_CLASS(ImageKit, IKSlideshow)

@class QLPreviewBubble;
@interface NSObject (WKQLPreviewBubbleDetails)
@property (copy) NSArray * controls;
@property NSSize maximumSize;
@property NSRectEdge preferredEdge;
@property (retain) IBOutlet NSWindow* parentWindow;
- (void)showPreviewItem:(id)previewItem itemFrame:(NSRect)frame;
- (void)setAutomaticallyCloseWithMask:(NSEventMask)autocloseMask filterMask:(NSEventMask)filterMask block:(void (^)(void))block;
@end

using namespace WebCore;

struct DictionaryPopupInfo {
    NSPoint origin;
    RetainPtr<NSDictionary> options;
    RetainPtr<NSAttributedString> attributedString;
};

@implementation WebActionMenuController

- (id)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _type = WebActionMenuNone;

    return self;
}

- (void)webViewClosed
{
    _webView = nil;
}

- (WebElementDictionary *)performHitTestAtPoint:(NSPoint)windowPoint
{
    WebHTMLView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    NSPoint point = [documentView convertPoint:windowPoint fromView:nil];

    Frame* coreFrame = core([documentView _frame]);
    if (!coreFrame)
        return nil;
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active;
    _hitTestResult = coreFrame->eventHandler().hitTestResultAtPoint(IntPoint(point), hitType);

    return [[[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult] autorelease];
}

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (!_webView)
        return;

    NSMenu *actionMenu = _webView.actionMenu;
    if (menu != actionMenu)
        return;

    [actionMenu removeAllItems];

    WebElementDictionary *hitTestResult = [self performHitTestAtPoint:event.locationInWindow];
    NSArray *menuItems = [self _defaultMenuItemsForHitTestResult:hitTestResult];

    // Allow clients to customize the menu items.
    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:actionMenuItemsForHitTestResult:withType:defaultActionMenuItems:)])
        menuItems = [[_webView UIDelegate] _webView:_webView actionMenuItemsForHitTestResult:hitTestResult withType:_type defaultActionMenuItems:menuItems];

    for (NSMenuItem *item in menuItems)
        [actionMenu addItem:item];
}

- (BOOL)isMenuForTextContent
{
    return _type == WebActionMenuReadOnlyText || _type == WebActionMenuEditableText || _type == WebActionMenuEditableTextWithSuggestions || _type == WebActionMenuWhitespaceInEditableArea;
}

- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _webView.actionMenu)
        return;

    if (_type == WebActionMenuDataDetectedItem) {
        if (![getDDActionsManagerClass() shouldUseActionsWithContext:_currentActionContext.get()]) {
            [menu cancelTracking];
            return;
        }

        if (menu.numberOfItems == 1)
            [[_webView _selectedOrMainFrame] _clearSelection];
        else
            [self _selectDataDetectedText];
        return;
    }

    if (![self isMenuForTextContent]) {
        [[_webView _selectedOrMainFrame] _clearSelection];
        return;
    }

    // Action menus for text should highlight the text so that it is clear what the action menu actions
    // will apply to. If the text is already selected, the menu will use the existing selection.
    if (!_hitTestResult.isSelected())
        [self _selectLookupText];
}

- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _webView.actionMenu)
        return;

    if (_type == WebActionMenuDataDetectedItem && _currentActionContext)
        [getDDActionsManagerClass() didUseActions];

    _type = WebActionMenuNone;
    _sharingServicePicker = nil;
}

#pragma mark Link actions

- (void)_openURLFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSURL *url = [sender representedObject];
    if (!url)
        return;

    ASSERT([url isKindOfClass:[NSURL class]]);

    [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)_addToReadingListFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSURL *url = [sender representedObject];
    if (!url)
        return;

    ASSERT([url isKindOfClass:[NSURL class]]);

    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
    [service performWithItems:@[ url ]];
}

- (NSRect)_elementBoundingBoxFromDOMElement:(DOMElement *)domElement
{
    if (!domElement)
        return NSZeroRect;

    Node* node = core(domElement);
    Frame* frame = node->document().frame();
    if (!frame)
        return NSZeroRect;

    FrameView* view = frame->view();
    if (!view)
        return NSZeroRect;

    return view->contentsToWindow(node->pixelSnappedBoundingBox());
}

- (void)_quickLookURLFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSDictionary *hitTestResult = [sender representedObject];
    if (!hitTestResult)
        return;

    ASSERT([hitTestResult isKindOfClass:[NSDictionary class]]);

    NSURL *url = [hitTestResult objectForKey:WebElementLinkURLKey];
    if (!url)
        return;

    DOMElement *domElement = [hitTestResult objectForKey:WebElementDOMNodeKey];
    if (!domElement)
        return;

    NSRect itemFrame = [_webView convertRect:[self _elementBoundingBoxFromDOMElement:domElement] toView:nil];
    NSSize maximumPreviewSize = NSMakeSize(_webView.bounds.size.width * 0.75, _webView.bounds.size.height * 0.75);

    RetainPtr<QLPreviewBubble> bubble = adoptNS([[getQLPreviewBubbleClass() alloc] init]);
    [bubble setParentWindow:_webView.window];
    [bubble setMaximumSize:maximumPreviewSize];
    [bubble setPreferredEdge:NSMaxYEdge];
    [bubble setControls:@[ ]];
    NSEventMask filterMask = NSAnyEventMask & ~(NSAppKitDefinedMask | NSSystemDefinedMask | NSApplicationDefinedMask | NSMouseEnteredMask | NSMouseExitedMask);
    NSEventMask autocloseMask = NSLeftMouseDownMask | NSRightMouseDownMask | NSKeyDownMask;
    [bubble setAutomaticallyCloseWithMask:autocloseMask filterMask:filterMask block:[bubble] {
        [bubble close];
    }];
    [bubble showPreviewItem:url itemFrame:itemFrame];
}

- (NSArray *)_defaultMenuItemsForLink:(WebElementDictionary *)hitTestResult
{
    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:WebActionMenuItemTagOpenLinkInDefaultBrowser withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> previewLinkItem = [self _createActionMenuItemForTag:WebActionMenuItemTagPreviewLink withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> readingListItem = [self _createActionMenuItemForTag:WebActionMenuItemTagAddLinkToSafariReadingList withHitTestResult:hitTestResult];

    return @[ openLinkItem.get(), previewLinkItem.get(), [NSMenuItem separatorItem], readingListItem.get() ];
}

#pragma mark Mailto Link actions

- (NSArray *)_defaultMenuItemsForMailtoLink
{
    Node* node = _hitTestResult.innerNode();
    if (!node)
        return @[ ];

    RetainPtr<DDActionContext> actionContext = [[getDDActionContextClass() alloc] init];
    [actionContext setForActionMenuContent:YES];

    // FIXME: Should this show a yellow highlight?
    [actionContext setHighlightFrame:elementBoundingBoxInWindowCoordinatesFromNode(node)];
    return [[getDDActionsManagerClass() sharedManager] menuItemsForTargetURL:_hitTestResult.absoluteLinkURL() actionContext:actionContext.get()];
}

#pragma mark Image actions

- (NSArray *)_defaultMenuItemsForImage:(WebElementDictionary *)hitTestResult
{
    RetainPtr<NSMenuItem> copyImageItem = [self _createActionMenuItemForTag:WebActionMenuItemTagCopyImage withHitTestResult:hitTestResult];

    RetainPtr<NSMenuItem> addToPhotosItem;
    if ([self _canAddMediaToPhotos])
        addToPhotosItem = [self _createActionMenuItemForTag:WebActionMenuItemTagAddImageToPhotos withHitTestResult:hitTestResult];
    else
        addToPhotosItem = [NSMenuItem separatorItem];

    RetainPtr<NSMenuItem> saveToDownloadsItem = [self _createActionMenuItemForTag:WebActionMenuItemTagSaveImageToDownloads withHitTestResult:hitTestResult];
    if (!_webView.downloadDelegate)
        [saveToDownloadsItem setEnabled:NO];


    RetainPtr<NSMenuItem> shareItem = [self _createActionMenuItemForTag:WebActionMenuItemTagShareImage withHitTestResult:hitTestResult];
    if (Image* image = _hitTestResult.image()) {
        RetainPtr<CGImageRef> cgImage = image->getCGImageRef();
        RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:NSZeroSize]);
        _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ nsImage.get() ]]);
        [_sharingServicePicker setDelegate:self];
        [shareItem setSubmenu:[_sharingServicePicker menu]];
    }

    return @[ copyImageItem.get(), addToPhotosItem.get(), saveToDownloadsItem.get(), shareItem.get() ];
}

- (void)_copyImage:(id)sender
{
    Frame* frame = core([_webView _selectedOrMainFrame]);
    if (!frame)
        return;
    frame->editor().copyImage(_hitTestResult);
}

static NSString *temporaryPhotosDirectoryPath()
{
    static NSString *temporaryPhotosDirectoryPath;

    if (!temporaryPhotosDirectoryPath) {
        NSString *temporaryDirectoryTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitPhotos-XXXXXX"];
        CString templateRepresentation = [temporaryDirectoryTemplate fileSystemRepresentation];

        if (mkdtemp(templateRepresentation.mutableData()))
            temporaryPhotosDirectoryPath = [[[NSFileManager defaultManager] stringWithFileSystemRepresentation:templateRepresentation.data() length:templateRepresentation.length()] copy];
    }

    return temporaryPhotosDirectoryPath;
}

static NSString *pathToPhotoOnDisk(NSString *suggestedFilename)
{
    NSString *photoDirectoryPath = temporaryPhotosDirectoryPath();
    if (!photoDirectoryPath) {
        WTFLogAlways("Cannot create temporary photo download directory.");
        return nil;
    }

    NSString *path = [photoDirectoryPath stringByAppendingPathComponent:suggestedFilename];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path]) {
        NSString *pathTemplatePrefix = [photoDirectoryPath stringByAppendingPathComponent:@"XXXXXX-"];
        NSString *pathTemplate = [pathTemplatePrefix stringByAppendingString:suggestedFilename];
        CString pathTemplateRepresentation = [pathTemplate fileSystemRepresentation];

        int fd = mkstemps(pathTemplateRepresentation.mutableData(), pathTemplateRepresentation.length() - strlen([pathTemplatePrefix fileSystemRepresentation]) + 1);
        if (fd < 0) {
            WTFLogAlways("Cannot create photo file in the temporary directory (%@).", suggestedFilename);
            return nil;
        }

        close(fd);
        path = [fileManager stringWithFileSystemRepresentation:pathTemplateRepresentation.data() length:pathTemplateRepresentation.length()];
    }

    return path;
}

- (BOOL)_canAddMediaToPhotos
{
    return [getIKSlideshowClass() canExportToApplication:@"com.apple.Photos"];
}

- (void)_addImageToPhotos:(id)sender
{
    if (![self _canAddMediaToPhotos])
        return;

    Image* image = _hitTestResult.image();
    if (!image)
        return;

    RetainPtr<CGImageRef> cgImage = image->getCGImageRef();

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString * const suggestedFilename = @"image.jpg";

        NSString *filePath = pathToPhotoOnDisk(suggestedFilename);
        if (!filePath)
            return;

        NSURL *fileURL = [NSURL fileURLWithPath:filePath];
        auto dest = adoptCF(CGImageDestinationCreateWithURL((CFURLRef)fileURL, kUTTypeJPEG, 1, nullptr));
        CGImageDestinationAddImage(dest.get(), cgImage.get(), nullptr);
        CGImageDestinationFinalize(dest.get());

        dispatch_async(dispatch_get_main_queue(), ^{
            // This API provides no way to report failure, but if 18420778 is fixed so that it does, we should handle this.
            [getIKSlideshowClass() exportSlideshowItem:filePath toApplication:@"com.apple.Photos"];
        });
    });
}

- (void)_saveImageToDownloads:(id)sender
{
    [_webView _downloadURL:_hitTestResult.absoluteImageURL()];
}

#pragma mark Text actions

- (NSArray *)_defaultMenuItemsForText:(WebElementDictionary *)hitTestResult
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagCopyText withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagLookupText withHitTestResult:hitTestResult];

    return @[ copyTextItem.get(), lookupTextItem.get() ];
}

- (NSArray *)_defaultMenuItemsForEditableText:(WebElementDictionary *)hitTestResult
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagCopyText withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagLookupText withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:WebActionMenuItemTagPaste withHitTestResult:hitTestResult];

    return @[ copyTextItem.get(), lookupTextItem.get(), pasteItem.get() ];
}

- (NSArray *)_defaultMenuItemsForEditableTextWithSuggestions:(WebElementDictionary *)hitTestResult
{
    Frame* frame = core([_webView _selectedOrMainFrame]);
    if (!frame)
        return @[ ];

    NSDictionary *options = nil;
    RefPtr<Range> lookupRange = rangeForDictionaryLookupAtHitTestResult(_hitTestResult, &options);
    if (!lookupRange)
        return @[ ];

    String lookupText = lookupRange->text();
    TextCheckerClient* textChecker = frame->editor().textChecker();
    if (!textChecker)
        return @[ ];

    Vector<TextCheckingResult> results = textChecker->checkTextOfParagraph(lookupText, NSTextCheckingTypeSpelling);
    if (results.isEmpty())
        return @[ ];

    Vector<String> guesses;
    frame->editor().textChecker()->getGuessesForWord(lookupText, String(), guesses);
    if (guesses.isEmpty())
        return @[ ];

    RetainPtr<NSMenu> spellingSubMenu = adoptNS([[NSMenu alloc] init]);
    for (const auto& guess : guesses) {
        RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:guess action:@selector(_changeSelectionToSuggestion:) keyEquivalent:@""]);
        [item setRepresentedObject:guess];
        [item setTarget:self];
        [spellingSubMenu addItem:item.get()];
    }

    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagCopyText withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:WebActionMenuItemTagLookupText withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:WebActionMenuItemTagPaste withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> textSuggestionsItem = [self _createActionMenuItemForTag:WebActionMenuItemTagTextSuggestions withHitTestResult:hitTestResult];

    [textSuggestionsItem setSubmenu:spellingSubMenu.get()];

    return @[ copyTextItem.get(), lookupTextItem.get(), pasteItem.get(), textSuggestionsItem.get() ];
}

- (void)_selectDataDetectedText
{
    [_webView _mainCoreFrame]->selection().setSelectedRange(_currentDetectedDataRange.get(), DOWNSTREAM, true);
}

- (NSArray *)_defaultMenuItemsForDataDetectedText
{
    RefPtr<Range> detectedDataRange;
    FloatRect detectedDataBoundingBox;
    RetainPtr<DDActionContext> actionContext;

    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:actionContextForHitTestResult:range:)]) {
        RetainPtr<WebElementDictionary> hitTestDictionary = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult]);

        DOMRange *customDataDetectorsRange;
        actionContext = [[_webView UIDelegate] _webView:_webView actionContextForHitTestResult:hitTestDictionary.get() range:&customDataDetectorsRange];

        if (actionContext && customDataDetectorsRange)
            detectedDataRange = core(customDataDetectorsRange);
    }

    // If the client didn't give us an action context, try to scan around the hit point.
    if (!actionContext || !detectedDataRange)
        actionContext = DataDetection::detectItemAroundHitTestResult(_hitTestResult, detectedDataBoundingBox, detectedDataRange);

    if (!actionContext || !detectedDataRange)
        return @[ ];

    // FIXME: We should hide/show the yellow highlight here.
    _currentActionContext = [actionContext contextForView:_webView altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
    } interactionStoppedHandler:^() {
    }];
    _currentDetectedDataRange = detectedDataRange;

    [_currentActionContext setHighlightFrame:[_webView.window convertRectToScreen:[_webView convertRect:detectedDataBoundingBox toView:nil]]];

    return [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_currentActionContext mainResult] actionContext:_currentActionContext.get()];
}

- (void)_copySelection:(id)sender
{
    [_webView _executeCoreCommandByName:@"copy" value:nil];
}

- (void)_lookupText:(id)sender
{
    Frame* frame = core([_webView _selectedOrMainFrame]);
    if (!frame)
        return;

    DictionaryPopupInfo popupInfo = performDictionaryLookupForSelection(frame, frame->selection().selection());
    if (!popupInfo.attributedString)
        return;

    NSPoint textBaselineOrigin = popupInfo.origin;

    // Convert to screen coordinates.
    textBaselineOrigin = [_webView.window convertRectToScreen:NSMakeRect(textBaselineOrigin.x, textBaselineOrigin.y, 0, 0)].origin;

    WKShowWordDefinitionWindow(popupInfo.attributedString.get(), textBaselineOrigin, popupInfo.options.get());
}

- (void)_paste:(id)sender
{
    [_webView _executeCoreCommandByName:@"paste" value:nil];
}

- (void)_selectLookupText
{
    NSDictionary *options = nil;
    RefPtr<Range> lookupRange = rangeForDictionaryLookupAtHitTestResult(_hitTestResult, &options);
    if (!lookupRange)
        return;

    Frame* frame = _hitTestResult.innerNode()->document().frame();
    if (!frame)
        return;

    frame->selection().setSelectedRange(lookupRange.get(), DOWNSTREAM, true);
}

- (void)_changeSelectionToSuggestion:(id)sender
{
    NSString *selectedCorrection = [sender representedObject];
    if (!selectedCorrection)
        return;

    ASSERT([selectedCorrection isKindOfClass:[NSString class]]);

    WebHTMLView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    [documentView _changeSpellingToWord:selectedCorrection];
}

static DictionaryPopupInfo performDictionaryLookupForSelection(Frame* frame, const VisibleSelection& selection)
{
    NSDictionary *options = nil;
    DictionaryPopupInfo popupInfo;
    RefPtr<Range> selectedRange = rangeForDictionaryLookupForSelection(selection, &options);
    if (selectedRange)
        popupInfo = performDictionaryLookupForRange(frame, *selectedRange, options);
    return popupInfo;
}

static DictionaryPopupInfo performDictionaryLookupForRange(Frame* frame, Range& range, NSDictionary *options)
{
    DictionaryPopupInfo popupInfo;
    if (range.text().stripWhiteSpace().isEmpty())
        return popupInfo;
    
    RenderObject* renderer = range.startContainer()->renderer();
    const RenderStyle& style = renderer->style();

    Vector<FloatQuad> quads;
    range.textQuads(quads);
    if (quads.isEmpty())
        return popupInfo;

    IntRect rangeRect = frame->view()->contentsToWindow(quads[0].enclosingBoundingBox());

    popupInfo.origin = NSMakePoint(rangeRect.x(), rangeRect.y() + (style.fontMetrics().descent() * frame->page()->pageScaleFactor()));
    popupInfo.options = options;

    NSAttributedString *nsAttributedString = editingAttributedStringFromRange(range);
    RetainPtr<NSMutableAttributedString> scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    [nsAttributedString enumerateAttributesInRange:NSMakeRange(0, [nsAttributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);

        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font) {
            font = [fontManager convertFont:font toSize:[font pointSize] * frame->page()->pageScaleFactor()];
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        }

        [scaledNSAttributedString addAttributes:scaledAttributes.get() range:range];
    }];

    popupInfo.attributedString = scaledNSAttributedString.get();
    return popupInfo;
}

#pragma mark Whitespace actions

- (NSArray *)_defaultMenuItemsForWhitespaceInEditableArea:(WebElementDictionary *)hitTestResult
{
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:WebActionMenuItemTagPaste withHitTestResult:hitTestResult];

    return @[ [NSMenuItem separatorItem], [NSMenuItem separatorItem], pasteItem.get() ];
}

#pragma mark NSSharingServicePickerDelegate implementation

- (NSArray *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)mask proposedSharingServices:(NSArray *)proposedServices
{
    RetainPtr<NSMutableArray> services = adoptNS([[NSMutableArray alloc] initWithCapacity:proposedServices.count]);

    for (NSSharingService *service in proposedServices) {
        if ([service.name isEqualToString:NSSharingServiceNameAddToIPhoto])
            continue;
        [services addObject:service];
    }

    return services.autorelease();
}

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

#pragma mark NSSharingServiceDelegate implementation

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return _webView.window;
}

#pragma mark Menu Items

- (RetainPtr<NSMenuItem>)_createActionMenuItemForTag:(uint32_t)tag withHitTestResult:(WebElementDictionary *)hitTestResult
{
    SEL selector = nullptr;
    NSString *title = nil;
    NSImage *image = nil;
    id representedObject = nil;

    switch (tag) {
    case WebActionMenuItemTagOpenLinkInDefaultBrowser:
        selector = @selector(_openURLFromActionMenu:);
        title = WEB_UI_STRING_KEY("Open", "Open (action menu item)", "action menu item");
        image = webKitBundleImageNamed(@"OpenInNewWindowTemplate");
        representedObject = [hitTestResult objectForKey:WebElementLinkURLKey];
        break;

    case WebActionMenuItemTagPreviewLink:
        selector = @selector(_quickLookURLFromActionMenu:);
        title = WEB_UI_STRING_KEY("Preview", "Preview (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuQuickLook"];
        representedObject = hitTestResult;
        break;

    case WebActionMenuItemTagAddLinkToSafariReadingList:
        selector = @selector(_addToReadingListFromActionMenu:);
        title = WEB_UI_STRING_KEY("Add to Reading List", "Add to Reading List (action menu item)", "action menu item");
        image = [NSImage imageNamed:NSImageNameBookmarksTemplate];
        representedObject = [hitTestResult objectForKey:WebElementLinkURLKey];
        break;

    case WebActionMenuItemTagCopyText:
        selector = @selector(_copySelection:);
        title = WEB_UI_STRING_KEY("Copy", "Copy (text action menu item)", "text action menu item");
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case WebActionMenuItemTagLookupText:
        selector = @selector(_lookupText:);
        title = WEB_UI_STRING_KEY("Look Up", "Look Up (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuLookup"];
        break;

    case WebActionMenuItemTagPaste:
        selector = @selector(_paste:);
        title = WEB_UI_STRING_KEY("Paste", "Paste (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuPaste"];
        break;

    case WebActionMenuItemTagTextSuggestions:
        title = WEB_UI_STRING_KEY("Suggestions", "Suggestions (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuSpelling"];
        break;

    case WebActionMenuItemTagCopyImage:
        selector = @selector(_copyImage:);
        title = WEB_UI_STRING_KEY("Copy", "Copy (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case WebActionMenuItemTagAddImageToPhotos:
        selector = @selector(_addImageToPhotos:);
        title = WEB_UI_STRING_KEY("Add to Photos", "Add to Photos (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuAddToPhotos"];
        break;

    case WebActionMenuItemTagSaveImageToDownloads:
        selector = @selector(_saveImageToDownloads:);
        title = WEB_UI_STRING_KEY("Save to Downloads", "Save to Downloads (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuSaveToDownloads"];
        break;

    case WebActionMenuItemTagShareImage:
        title = WEB_UI_STRING_KEY("Share (image action menu item)", "Share (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuShare"];
        break;

    default:
        ASSERT_NOT_REACHED();
        return nil;
    }

    RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:@""]);
    [item setImage:image];
    [item setTarget:self];
    [item setTag:tag];
    [item setRepresentedObject:representedObject];
    return item;
}

static NSImage *webKitBundleImageNamed(NSString *name)
{
    return [[NSBundle bundleForClass:NSClassFromString(@"WKView")] imageForResource:name];
}

- (NSArray *)_defaultMenuItemsForHitTestResult:(WebElementDictionary *)hitTestResult
{
    NSURL *url = [hitTestResult objectForKey:WebElementLinkURLKey];
    if (url && WebCore::protocolIsInHTTPFamily([url absoluteString])) {
        _type = WebActionMenuLink;
        return [self _defaultMenuItemsForLink:hitTestResult];
    }

    if (url && WebCore::protocolIs([url absoluteString], "mailto")) {
        _type = WebActionMenuMailtoLink;
        return [self _defaultMenuItemsForMailtoLink];
    }

    if (_hitTestResult.image() && !_hitTestResult.absoluteImageURL().isEmpty()) {
        _type = WebActionMenuImage;
        return [self _defaultMenuItemsForImage:hitTestResult];
    }

    Node* node = _hitTestResult.innerNode();
    if (node && node->isTextNode()) {
        NSArray *dataDetectorMenuItems = [self _defaultMenuItemsForDataDetectedText];
        if (dataDetectorMenuItems.count) {
            _type = WebActionMenuDataDetectedItem;
            return dataDetectorMenuItems;
        }

        if (_hitTestResult.isContentEditable()) {
            NSArray *editableTextWithSuggestions = [self _defaultMenuItemsForEditableTextWithSuggestions:hitTestResult];
            if (editableTextWithSuggestions.count) {
                _type = WebActionMenuEditableTextWithSuggestions;
                return editableTextWithSuggestions;
            }

            _type = WebActionMenuEditableText;
            return [self _defaultMenuItemsForEditableText:hitTestResult];
        }

        _type = WebActionMenuReadOnlyText;
        return [self _defaultMenuItemsForText:hitTestResult];
    }

    if (_hitTestResult.isContentEditable()) {
        _type = WebActionMenuWhitespaceInEditableArea;
        return [self _defaultMenuItemsForWhitespaceInEditableArea:hitTestResult];
    }

    _type = WebActionMenuNone;
    return @[ ];
}

@end

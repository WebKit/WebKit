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

#import "config.h"
#import "WKActionMenuController.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import "TextIndicator.h"
#import "WKNSURLExtras.h"
#import "WKViewInternal.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"
#import "WebContext.h"
#import "WebKitSystemInterface.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <ImageKit/ImageKit.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/NSSharingServiceSPI.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/URL.h>

enum class MenuUpdateStage {
    PrepareForMenu,
    MenuNeedsUpdate
};

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, ImageKit)
SOFT_LINK_CLASS(ImageKit, IKSlideshow)

using namespace WebCore;
using namespace WebKit;

@interface WKActionMenuController () <NSSharingServiceDelegate, NSSharingServicePickerDelegate, NSPopoverDelegate>
- (void)_updateActionMenuItemsForStage:(MenuUpdateStage)stage;
- (BOOL)_canAddImageToPhotos;
- (void)_showTextIndicator;
- (void)_hideTextIndicator;
@end

@interface WKView (WKDeprecatedSPI)
- (NSArray *)_actionMenuItemsForHitTestResult:(WKHitTestResultRef)hitTestResult defaultActionMenuItems:(NSArray *)defaultMenuItems;
@end

#if WK_API_ENABLED

@class WKPagePreviewViewController;

@protocol WKPagePreviewViewControllerDelegate <NSObject>
- (NSView *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController viewForPreviewingURL:(NSURL *)url initialFrameSize:(NSSize)initialFrameSize;
- (void)pagePreviewViewControllerWasClicked:(WKPagePreviewViewController *)pagePreviewViewController;
@end

@interface WKPagePreviewViewController : NSViewController {
@public
    NSSize _mainViewSize;
    RetainPtr<NSURL> _url;
    id <WKPagePreviewViewControllerDelegate> _delegate;
    CGFloat _popoverToViewScale;
}

- (instancetype)initWithPageURL:(NSURL *)URL mainViewSize:(NSSize)size popoverToViewScale:(CGFloat)scale;

@end

@implementation WKPagePreviewViewController

- (instancetype)initWithPageURL:(NSURL *)URL mainViewSize:(NSSize)size popoverToViewScale:(CGFloat)scale
{
    if (!(self = [super init]))
        return nil;

    _url = URL;
    _mainViewSize = size;
    _popoverToViewScale = scale;

    return self;
}

- (void)loadView
{
    NSRect defaultFrame = NSMakeRect(0, 0, _mainViewSize.width, _mainViewSize.height);
    RetainPtr<NSView> previewView = [_delegate pagePreviewViewController:self viewForPreviewingURL:_url.get() initialFrameSize:defaultFrame.size];
    if (!previewView) {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:defaultFrame]);
        [webView _setIgnoresNonWheelMouseEvents:YES];
        if (_url) {
            NSURLRequest *request = [NSURLRequest requestWithURL:_url.get()];
            [webView loadRequest:request];
        }
        previewView = webView;
    }

    // Setting the webView bounds will scale it to 75% of the _mainViewSize.
    [previewView setBounds:NSMakeRect(0, 0, _mainViewSize.width / _popoverToViewScale, _mainViewSize.height / _popoverToViewScale)];

    RetainPtr<NSClickGestureRecognizer> clickRecognizer = adoptNS([[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(_clickRecognized:)]);
    [previewView addGestureRecognizer:clickRecognizer.get()];
    self.view = previewView.get();
}

- (void)_clickRecognized:(NSGestureRecognizer *)gestureRecognizer
{
    [_delegate pagePreviewViewControllerWasClicked:self];
}

@end

@interface WKActionMenuController () <WKPagePreviewViewControllerDelegate>
@end

#endif

@implementation WKActionMenuController

- (instancetype)initWithPage:(WebPageProxy&)page view:(WKView *)wkView
{
    self = [super init];

    if (!self)
        return nil;

    _page = &page;
    _wkView = wkView;
    _type = kWKActionMenuNone;

    return self;
}

- (void)willDestroyView:(WKView *)view
{
    _page = nullptr;
    _wkView = nullptr;
    _hitTestResult = ActionMenuHitTestResult();

}

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    if (_wkView._shouldIgnoreMouseEvents) {
        [menu cancelTracking];
        return;
    }

    _page->performActionMenuHitTestAtLocation([_wkView convertPoint:event.locationInWindow fromView:nil]);

    _state = ActionMenuState::Pending;
    [self _updateActionMenuItemsForStage:MenuUpdateStage::PrepareForMenu];

    [self _hideTextIndicator];
    _shouldKeepPreviewPopoverOpen = NO;
}

- (BOOL)isMenuForTextContent
{
    return _type == kWKActionMenuReadOnlyText || _type == kWKActionMenuEditableText || _type == kWKActionMenuEditableTextWithSuggestions;
}

- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    if (_type == kWKActionMenuDataDetectedItem) {
        if (menu.numberOfItems == 1) {
            _page->clearSelection();
            [self _showTextIndicator];
        } else
            _page->selectLastActionMenuRange();
        return;
    }

    if (![self isMenuForTextContent]) {
        _page->clearSelection();
        return;
    }

    // Action menus for text should highlight the text so that it is clear what the action menu actions
    // will apply to. If the text is already selected, the menu will use the existing selection.
    RefPtr<WebHitTestResult> hitTestResult = [self _hitTestResultForStage:MenuUpdateStage::MenuNeedsUpdate];
    if (!hitTestResult->isSelected())
        _page->selectLastActionMenuRange();
}

- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    if (_type == kWKActionMenuDataDetectedItem && menu.numberOfItems > 1)
        [self _hideTextIndicator];

    if (!_shouldKeepPreviewPopoverOpen)
        [self _clearPreviewPopover];

    _state = ActionMenuState::None;
    _hitTestResult = ActionMenuHitTestResult();
    _type = kWKActionMenuNone;
    _sharingServicePicker = nil;
}

- (void)didPerformActionMenuHitTest:(const ActionMenuHitTestResult&)hitTestResult userData:(API::Object*)userData
{
    // FIXME: This needs to use the WebKit2 callback mechanism to avoid out-of-order replies.
    _state = ActionMenuState::Ready;
    _hitTestResult = hitTestResult;
    _userData = userData;
}

- (void)dismissActionMenuDataDetectorPopovers
{
    // Ideally, this would actually dismiss the popovers. We don't currently have the ability to request
    // that and know whether or not it succeeded, so we'll settle for unanchoring.
    [[getDDActionsManagerClass() sharedManager] unanchorBubbles];
    [self _hideTextIndicator];
}

#pragma mark Text Indicator

- (void)_showTextIndicator
{
    if (_isShowingTextIndicator)
        return;

    if (_hitTestResult.detectedDataTextIndicator) {
        _page->setTextIndicator(_hitTestResult.detectedDataTextIndicator->data(), false, true);
        _isShowingTextIndicator = YES;
    }
}

- (void)_hideTextIndicator
{
    if (!_isShowingTextIndicator)
        return;

    _page->clearTextIndicator(false, true);
    _isShowingTextIndicator = NO;
}

#pragma mark Link actions

- (NSArray *)_defaultMenuItemsForLink
{
    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagOpenLinkInDefaultBrowser];
#if WK_API_ENABLED
    RetainPtr<NSMenuItem> previewLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPreviewLink];
#else
    RetainPtr<NSMenuItem> previewLinkItem = [NSMenuItem separatorItem];
#endif
    RetainPtr<NSMenuItem> readingListItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddLinkToSafariReadingList];

    return @[ openLinkItem.get(), previewLinkItem.get(), [NSMenuItem separatorItem], readingListItem.get() ];
}

- (void)_openURLFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->lastMouseMoveHitTestResult();
    [[NSWorkspace sharedWorkspace] openURL:[NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()]];
}

- (void)_addToReadingListFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->lastMouseMoveHitTestResult();
    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
    [service performWithItems:@[ [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()] ]];
}

#if WK_API_ENABLED
- (void)_keepPreviewOpenFromActionMenu:(id)sender
{
    _shouldKeepPreviewPopoverOpen = YES;
}

- (void)_previewURLFromActionMenu:(id)sender
{
    // We might already have a preview showing if the menu item was highlighted earlier.
    if (_previewPopover)
        return;

    WebHitTestResult* hitTestResult = _page->lastMouseMoveHitTestResult();
    NSURL *url = [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()];
    NSRect originRect = hitTestResult->elementBoundingBox();
    [self _createPreviewPopoverForURL:url originRect:originRect];
    [_previewPopover showRelativeToRect:originRect ofView:_wkView preferredEdge:NSMaxYEdge];
}

- (void)_createPreviewPopoverForURL:(NSURL *)url originRect:(NSRect)originRect
{
    NSSize popoverSize = [self _preferredSizeForPopoverPresentedFromOriginRect:originRect];
    CGFloat actualPopoverToViewScale = popoverSize.width / NSWidth(_wkView.bounds);
    RetainPtr<WKPagePreviewViewController> previewViewController = adoptNS([[WKPagePreviewViewController alloc] initWithPageURL:url mainViewSize:_wkView.bounds.size popoverToViewScale:actualPopoverToViewScale]);
    previewViewController->_delegate = self;

    _previewPopover = adoptNS([[NSPopover alloc] init]);
    [_previewPopover setBehavior:NSPopoverBehaviorTransient];
    [_previewPopover setContentSize:popoverSize];
    [_previewPopover setContentViewController:previewViewController.get()];
    [_previewPopover setDelegate:self];
}

static bool targetSizeFitsInAvailableSpace(NSSize targetSize, NSSize availableSpace)
{
    return targetSize.width <= availableSpace.width && targetSize.height <= availableSpace.height;
}

- (NSSize)_preferredSizeForPopoverPresentedFromOriginRect:(NSRect)originRect
{
    static const CGFloat preferredPopoverToViewScale = 0.75;
    static const CGFloat screenPadding = 40;

    NSWindow *window = _wkView.window;
    NSRect originScreenRect = [window convertRectToScreen:[_wkView convertRect:originRect toView:nil]];
    NSRect screenFrame = window.screen.visibleFrame;

    NSRect wkViewBounds = _wkView.bounds;
    NSSize targetSize = NSMakeSize(NSWidth(wkViewBounds) * preferredPopoverToViewScale, NSHeight(wkViewBounds) * preferredPopoverToViewScale);

    CGFloat availableSpaceAbove = NSMaxY(screenFrame) - NSMaxY(originScreenRect);
    CGFloat availableSpaceBelow = NSMinY(originScreenRect) - NSMinY(screenFrame);
    CGFloat maxAvailableVerticalSpace = fmax(availableSpaceAbove, availableSpaceBelow) - screenPadding;
    NSSize maxSpaceAvailableOnYEdge = NSMakeSize(screenFrame.size.width - screenPadding, maxAvailableVerticalSpace);
    if (targetSizeFitsInAvailableSpace(targetSize, maxSpaceAvailableOnYEdge))
        return targetSize;

    CGFloat availableSpaceAtLeft = NSMinX(originScreenRect) - NSMinX(screenFrame);
    CGFloat availableSpaceAtRight = NSMaxX(screenFrame) - NSMaxX(originScreenRect);
    CGFloat maxAvailableHorizontalSpace = fmax(availableSpaceAtLeft, availableSpaceAtRight) - screenPadding;
    NSSize maxSpaceAvailableOnXEdge = NSMakeSize(maxAvailableHorizontalSpace, screenFrame.size.height - screenPadding);
    if (targetSizeFitsInAvailableSpace(targetSize, maxSpaceAvailableOnXEdge))
        return targetSize;

    // If the target size doesn't fit anywhere, we'll find the largest rect that does fit that also maintains the original view's aspect ratio.
    CGFloat aspectRatio = wkViewBounds.size.width / wkViewBounds.size.height;
    FloatRect maxVerticalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(0, 0, maxSpaceAvailableOnYEdge.width, maxSpaceAvailableOnYEdge.height));
    FloatRect maxHorizontalTargetSizePreservingAspectRatioRect = largestRectWithAspectRatioInsideRect(aspectRatio, FloatRect(0, 0, maxSpaceAvailableOnXEdge.width, maxSpaceAvailableOnXEdge.height));

    NSSize maxVerticalTargetSizePreservingAspectRatio = NSMakeSize(maxVerticalTargetSizePreservingAspectRatioRect.width(), maxVerticalTargetSizePreservingAspectRatioRect.height());
    NSSize maxHortizontalTargetSizePreservingAspectRatio = NSMakeSize(maxHorizontalTargetSizePreservingAspectRatioRect.width(), maxHorizontalTargetSizePreservingAspectRatioRect.height());

    if ((maxVerticalTargetSizePreservingAspectRatio.width * maxVerticalTargetSizePreservingAspectRatio.height) > (maxHortizontalTargetSizePreservingAspectRatio.width * maxHortizontalTargetSizePreservingAspectRatio.height))
        return maxVerticalTargetSizePreservingAspectRatio;
    return maxHortizontalTargetSizePreservingAspectRatio;
}

#endif // WK_API_ENABLED

- (void)_clearPreviewPopover
{
#if WK_API_ENABLED
    if (WKPagePreviewViewController *pagePreviewViewController = (WKPagePreviewViewController *)[_previewPopover contentViewController])
        pagePreviewViewController->_delegate = nil;
#endif

    [_previewPopover close];
    [_previewPopover setDelegate:nil];
    _previewPopover = nil;
}

#pragma mark Image actions

- (NSArray *)_defaultMenuItemsForImage
{
    RetainPtr<NSMenuItem> copyImageItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyImage];
    RetainPtr<NSMenuItem> addToPhotosItem;
    if ([self _canAddImageToPhotos])
        addToPhotosItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddImageToPhotos];
    else
        addToPhotosItem = [NSMenuItem separatorItem];
    RetainPtr<NSMenuItem> saveToDownloadsItem = [self _createActionMenuItemForTag:kWKContextActionItemTagSaveImageToDownloads];
    RetainPtr<NSMenuItem> shareItem = [self _createActionMenuItemForTag:kWKContextActionItemTagShareImage];

    if (RefPtr<ShareableBitmap> bitmap = _hitTestResult.image) {
        RetainPtr<CGImageRef> image = bitmap->makeCGImage();
        RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithCGImage:image.get() size:NSZeroSize]);
        _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ nsImage.get() ]]);
        [_sharingServicePicker setDelegate:self];
        [shareItem setSubmenu:[_sharingServicePicker menu]];
    }

    return @[ copyImageItem.get(), addToPhotosItem.get(), saveToDownloadsItem.get(), shareItem.get() ];
}

- (void)_copyImage:(id)sender
{
    RefPtr<ShareableBitmap> bitmap = _hitTestResult.image;
    if (!bitmap)
        return;

    RetainPtr<CGImageRef> image = bitmap->makeCGImage();
    RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithCGImage:image.get() size:NSZeroSize]);
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] writeObjects:@[ nsImage.get() ]];
}

- (void)_saveImageToDownloads:(id)sender
{
    WebHitTestResult* hitTestResult = _page->lastMouseMoveHitTestResult();
    _page->process().context().download(_page, hitTestResult->absoluteImageURL());
}

// FIXME: We should try to share this with WebPageProxyMac's similar PDF functions.
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

- (BOOL)_canAddImageToPhotos
{
    return [getIKSlideshowClass() canExportToApplication:@"com.apple.Photos"];
}

- (void)_addImageToPhotos:(id)sender
{
    if (![self _canAddImageToPhotos])
        return;

    RefPtr<ShareableBitmap> bitmap = _hitTestResult.image;
    if (!bitmap)
        return;
    RetainPtr<CGImageRef> image = bitmap->makeCGImage();

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString * const suggestedFilename = @"image.jpg";

        NSString *filePath = pathToPhotoOnDisk(suggestedFilename);
        if (!filePath)
            return;

        NSURL *fileURL = [NSURL fileURLWithPath:filePath];
        auto dest = adoptCF(CGImageDestinationCreateWithURL((CFURLRef)fileURL, kUTTypeJPEG, 1, nullptr));
        CGImageDestinationAddImage(dest.get(), image.get(), nullptr);
        CGImageDestinationFinalize(dest.get());

        dispatch_async(dispatch_get_main_queue(), ^{
            // This API provides no way to report failure, but if 18420778 is fixed so that it does, we should handle this.
            [getIKSlideshowClass() exportSlideshowItem:filePath toApplication:@"com.apple.Photos"];
        });
    });
}

#pragma mark Text actions

- (NSArray *)_defaultMenuItemsForDataDetectedText
{
    DDActionContext *actionContext = _hitTestResult.actionContext.get();
    if (!actionContext)
        return @[ ];

    actionContext.completionHandler = ^() {
        [self _hideTextIndicator];
    };

    WKSetDDActionContextIsForActionMenu(actionContext);
    actionContext.highlightFrame = [_wkView.window convertRectToScreen:[_wkView convertRect:_hitTestResult.detectedDataBoundingBox toView:nil]];
    return [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_hitTestResult.actionContext mainResult] actionContext:actionContext];
}

- (NSArray *)_defaultMenuItemsForText
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagLookupText];

    return @[ copyTextItem.get(), lookupTextItem.get() ];
}

- (NSArray *)_defaultMenuItemsForEditableText
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagLookupText];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];

    return @[ copyTextItem.get(), lookupTextItem.get(), pasteItem.get() ];
}

- (NSArray *)_defaultMenuItemsForEditableTextWithSuggestions
{
    if (_hitTestResult.lookupText.isEmpty())
        return @[ ];

    Vector<TextCheckingResult> results;
    _page->checkTextOfParagraph(_hitTestResult.lookupText, NSTextCheckingTypeSpelling, results);
    if (results.isEmpty())
        return @[ ];

    Vector<String> guesses;
    _page->getGuessesForWord(_hitTestResult.lookupText, String(), guesses);
    if (guesses.isEmpty())
        return @[ ];

    RetainPtr<NSMenu> spellingSubMenu = adoptNS([[NSMenu alloc] init]);
    for (const auto& guess : guesses) {
        RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:guess action:@selector(_changeSelectionToSuggestion:) keyEquivalent:@""]);
        [item setRepresentedObject:guess];
        [item setTarget:self];
        [spellingSubMenu addItem:item.get()];
    }

    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> lookupTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagLookupText];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];
    RetainPtr<NSMenuItem> textSuggestionsItem = [self _createActionMenuItemForTag:kWKContextActionItemTagTextSuggestions];

    [textSuggestionsItem setSubmenu:spellingSubMenu.get()];

    return @[ copyTextItem.get(), lookupTextItem.get(), pasteItem.get(), textSuggestionsItem.get() ];
}

- (void)_copySelection:(id)sender
{
    _page->executeEditCommand("copy");
}

- (void)_paste:(id)sender
{
    _page->executeEditCommand("paste");
}

- (void)_lookupText:(id)sender
{
    _page->performDictionaryLookupOfCurrentSelection();
}

- (void)_changeSelectionToSuggestion:(id)sender
{
    NSString *selectedCorrection = [sender representedObject];
    if (!selectedCorrection)
        return;

    ASSERT([selectedCorrection isKindOfClass:[NSString class]]);

    _page->changeSpellingToWord(selectedCorrection);
}

#pragma mark NSMenuDelegate implementation

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    if (menu != _wkView.actionMenu)
        return;

    ASSERT(_state != ActionMenuState::None);

    // FIXME: We need to be able to cancel this if the menu goes away.
    // FIXME: Connection can be null if the process is closed; we should clean up better in that case.
    if (_state == ActionMenuState::Pending) {
        if (auto* connection = _page->process().connection())
            connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidPerformActionMenuHitTest>(_page->pageID(), std::chrono::milliseconds(500));
    }

    [self _updateActionMenuItemsForStage:MenuUpdateStage::MenuNeedsUpdate];
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
#if WK_API_ENABLED
    if (item.tag != kWKContextActionItemTagPreviewLink)
        return;
    [self _previewURLFromActionMenu:item];
#endif
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
    return _wkView.window;
}

#pragma mark NSPopoverDelegate implementation

- (void)popoverWillClose:(NSNotification *)notification
{
    _shouldKeepPreviewPopoverOpen = NO;
    [self _clearPreviewPopover];
}

#pragma mark Menu Items

- (RetainPtr<NSMenuItem>)_createActionMenuItemForTag:(uint32_t)tag
{
    SEL selector = nullptr;
    NSString *title = nil;
    NSImage *image = nil;

    // FIXME: These titles need to be localized.
    switch (tag) {
    case kWKContextActionItemTagOpenLinkInDefaultBrowser:
        selector = @selector(_openURLFromActionMenu:);
        title = @"Open";
        image = webKitBundleImageNamed(@"OpenInNewWindowTemplate");
        break;

#if WK_API_ENABLED
    case kWKContextActionItemTagPreviewLink:
        selector = @selector(_keepPreviewOpenFromActionMenu:);
        title = @"Preview";
        image = [NSImage imageNamed:@"NSActionMenuQuickLook"];
        break;
#endif

    case kWKContextActionItemTagAddLinkToSafariReadingList:
        selector = @selector(_addToReadingListFromActionMenu:);
        title = @"Add to Safari Reading List";
        image = [NSImage imageNamed:NSImageNameBookmarksTemplate];
        break;

    case kWKContextActionItemTagCopyImage:
        selector = @selector(_copyImage:);
        title = @"Copy";
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case kWKContextActionItemTagAddImageToPhotos:
        selector = @selector(_addImageToPhotos:);
        title = @"Add to Photos";
        image = [NSImage imageNamed:@"NSActionMenuAddToPhotos"];
        break;

    case kWKContextActionItemTagSaveImageToDownloads:
        selector = @selector(_saveImageToDownloads:);
        title = @"Save to Downloads";
        image = [NSImage imageNamed:@"NSActionMenuSaveToDownloads"];
        break;

    case kWKContextActionItemTagShareImage:
        title = @"Share";
        image = [NSImage imageNamed:@"NSActionMenuShare"];
        break;

    case kWKContextActionItemTagCopyText:
        selector = @selector(_copySelection:);
        title = @"Copy";
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case kWKContextActionItemTagLookupText:
        selector = @selector(_lookupText:);
        title = @"Lookup";
        image = [NSImage imageNamed:@"NSActionMenuLookup"];
        break;

    case kWKContextActionItemTagPaste:
        selector = @selector(_paste:);
        title = @"Paste";
        image = [NSImage imageNamed:@"NSActionMenuPaste"];
        break;

    case kWKContextActionItemTagTextSuggestions:
        title = @"Suggestions";
        image = [NSImage imageNamed:@"NSActionMenuSpelling"];
        break;

    default:
        ASSERT_NOT_REACHED();
        return nil;
    }

    RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:@""]);
    [item setImage:image];
    [item setTarget:self];
    [item setTag:tag];
    return item;
}

static NSImage *webKitBundleImageNamed(NSString *name)
{
    return [[NSBundle bundleForClass:[WKView class]] imageForResource:name];
}

- (PassRefPtr<WebHitTestResult>)_hitTestResultForStage:(MenuUpdateStage)stage
{
    RefPtr<WebHitTestResult> hitTestResult;
    switch (stage) {
    case MenuUpdateStage::PrepareForMenu:
        hitTestResult = _page->lastMouseMoveHitTestResult();
        break;
    case MenuUpdateStage::MenuNeedsUpdate:
        if (_state == ActionMenuState::Ready)
            hitTestResult = WebHitTestResult::create(_hitTestResult.hitTestResult);
        else
            hitTestResult = _page->lastMouseMoveHitTestResult();
        break;
    }

        return hitTestResult.release(); 
}

- (NSArray *)_defaultMenuItems:(MenuUpdateStage)stage
{
    RefPtr<WebHitTestResult> hitTestResult = [self _hitTestResultForStage:stage];
    if (!hitTestResult) {
        _type = kWKActionMenuNone;
        return _state != ActionMenuState::Ready ? @[ [NSMenuItem separatorItem] ] : @[ ];
    }

    String absoluteLinkURL = hitTestResult->absoluteLinkURL();
    if (!absoluteLinkURL.isEmpty() && WebCore::protocolIsInHTTPFamily(absoluteLinkURL)) {
       _type = kWKActionMenuLink;
       return [self _defaultMenuItemsForLink];
    }

    if (!hitTestResult->absoluteImageURL().isEmpty() && _hitTestResult.image) {
        _type = kWKActionMenuImage;
        return [self _defaultMenuItemsForImage];
    }

    if (hitTestResult->isTextNode()) {
        NSArray *dataDetectorMenuItems = [self _defaultMenuItemsForDataDetectedText];
        if (dataDetectorMenuItems.count) {
            _type = kWKActionMenuDataDetectedItem;
            return dataDetectorMenuItems;
        }

        if (hitTestResult->isContentEditable()) {
            NSArray *editableTextWithSuggestions = [self _defaultMenuItemsForEditableTextWithSuggestions];
            if (editableTextWithSuggestions.count) {
                _type = kWKActionMenuEditableTextWithSuggestions;
                return editableTextWithSuggestions;
            }

            _type = kWKActionMenuEditableText;
            return [self _defaultMenuItemsForEditableText];
        }

        _type = kWKActionMenuReadOnlyText;
        return [self _defaultMenuItemsForText];
    }

    _type = kWKActionMenuNone;
    return _state != ActionMenuState::Ready ? @[ [NSMenuItem separatorItem] ] : @[ ];
}

- (void)_updateActionMenuItemsForStage:(MenuUpdateStage)stage
{
    [_wkView.actionMenu removeAllItems];

    NSArray *menuItems = [self _defaultMenuItems:stage];
    RefPtr<WebHitTestResult> hitTestResult = [self _hitTestResultForStage:stage];

    if ([_wkView respondsToSelector:@selector(_actionMenuItemsForHitTestResult:defaultActionMenuItems:)])
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(hitTestResult.get()) defaultActionMenuItems:menuItems];
    else
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(hitTestResult.get()) withType:_type defaultActionMenuItems:menuItems userData:toAPI(_userData.get())];

    for (NSMenuItem *item in menuItems)
        [_wkView.actionMenu addItem:item];
}

#if WK_API_ENABLED

#pragma mark WKPagePreviewViewControllerDelegate

- (NSView *)pagePreviewViewController:(WKPagePreviewViewController *)pagePreviewViewController viewForPreviewingURL:(NSURL *)url initialFrameSize:(NSSize)initialFrameSize
{
    return [_wkView _viewForPreviewingURL:url initialFrameSize:initialFrameSize];
}

- (void)pagePreviewViewControllerWasClicked:(WKPagePreviewViewController *)pagePreviewViewController
{
    if (NSURL *url = pagePreviewViewController->_url.get())
        [[NSWorkspace sharedWorkspace] openURL:url];
}

#endif

@end

#endif // PLATFORM(MAC)

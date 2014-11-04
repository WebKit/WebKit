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

#import "WKNSURLExtras.h"
#import "WKViewInternal.h"
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
#import <WebCore/NSSharingServiceSPI.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/URL.h>

enum class MenuUpdateStage {
    PrepareForMenu,
    MenuNeedsUpdate
};

// FIXME: This should move into an SPI header if it stays.
@class QLPreviewBubble;
@interface NSObject (WKQLPreviewBubbleDetails)
@property (copy) NSArray * controls;
@property NSSize maximumSize;
@property NSRectEdge preferredEdge;
@property (retain) IBOutlet NSWindow* parentWindow;
- (void)showPreviewItem:(id)previewItem itemFrame:(NSRect)frame;
- (void)setAutomaticallyCloseWithMask:(NSEventMask)autocloseMask filterMask:(NSEventMask)filterMask block:(void (^)(void))block;
@end

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, ImageKit)
SOFT_LINK_CLASS(ImageKit, IKSlideshow)

using namespace WebCore;
using namespace WebKit;

@interface WKActionMenuController () <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
- (void)_updateActionMenuItemsForStage:(MenuUpdateStage)stage;
- (BOOL)_canAddImageToPhotos;
@end

@interface WKView (WKDeprecatedSPI)
- (NSArray *)_actionMenuItemsForHitTestResult:(WKHitTestResultRef)hitTestResult defaultActionMenuItems:(NSArray *)defaultMenuItems;
@end

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
}

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    _page->performActionMenuHitTestAtLocation([_wkView convertPoint:event.locationInWindow fromView:nil]);

    _state = ActionMenuState::Pending;
    [self _updateActionMenuItemsForStage:MenuUpdateStage::PrepareForMenu];
}

- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    if (_type != kWKActionMenuReadOnlyText && _type != kWKActionMenuEditableText)
        return;

    // Action menus for text should highlight the text so that it is clear what the action menu actions
    // will apply to. If the text is already selected, the menu will use the existing selection.
    RefPtr<WebHitTestResult> hitTestResult = [self _hitTestResultForStage:MenuUpdateStage::MenuNeedsUpdate];
    if (!hitTestResult->isSelected())
        _page->selectLookupTextAtLocation([_wkView convertPoint:event.locationInWindow fromView:nil]);
}

- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;
    
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

#pragma mark Link actions

- (NSArray *)_defaultMenuItemsForLink
{
    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagOpenLinkInDefaultBrowser];
    RetainPtr<NSMenuItem> previewLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPreviewLink];
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

- (void)_quickLookURLFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->lastMouseMoveHitTestResult();
    NSRect itemFrame = [_wkView convertRect:hitTestResult->elementBoundingBox() toView:nil];
    NSSize maximumPreviewSize = NSMakeSize(_wkView.bounds.size.width * 0.75, _wkView.bounds.size.height * 0.75);

    RetainPtr<QLPreviewBubble> bubble = adoptNS([[NSClassFromString(@"QLPreviewBubble") alloc] init]);
    [bubble setParentWindow:_wkView.window];
    [bubble setMaximumSize:maximumPreviewSize];
    [bubble setPreferredEdge:NSMaxYEdge];
    [bubble setControls:@[ ]];
    NSEventMask filterMask = NSAnyEventMask & ~(NSAppKitDefinedMask | NSSystemDefinedMask | NSApplicationDefinedMask | NSMouseEnteredMask | NSMouseExitedMask);
    NSEventMask autocloseMask = NSLeftMouseDownMask | NSRightMouseDownMask | NSKeyDownMask;
    [bubble setAutomaticallyCloseWithMask:autocloseMask filterMask:filterMask block:[bubble] {
        [bubble close];
    }];
    [bubble showPreviewItem:[NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()] itemFrame:itemFrame];
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

    WKSetDDActionContextIsForActionMenu(actionContext);
    actionContext.highlightFrame = [_wkView.window convertRectToScreen:[_wkView convertRect:_hitTestResult.actionBoundingBox toView:nil]];
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

-(void)_copySelection:(id)sender
{
    _page->executeEditCommand("copy");
}

-(void)_paste:(id)sender
{
    _page->executeEditCommand("paste");
}

-(void)_lookupText:(id)sender
{
    _page->performDictionaryLookupOfCurrentSelection();
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

    case kWKContextActionItemTagPreviewLink:
        selector = @selector(_quickLookURLFromActionMenu:);
        title = @"Preview";
        image = [NSImage imageNamed:@"NSActionMenuQuickLook"];
        break;

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

    if (!hitTestResult->absoluteImageURL().isEmpty() && _hitTestResult.image) {
        _type = kWKActionMenuImage;
        return [self _defaultMenuItemsForImage];
    }

    String absoluteLinkURL = hitTestResult->absoluteLinkURL();
    if (!absoluteLinkURL.isEmpty() && WebCore::protocolIsInHTTPFamily(absoluteLinkURL)) {
       _type = kWKActionMenuLink;
       return [self _defaultMenuItemsForLink];
    }

    if (hitTestResult->isTextNode()) {
        NSArray *dataDetectorMenuItems = [self _defaultMenuItemsForDataDetectedText];
        if (dataDetectorMenuItems.count) {
            _type = kWKActionMenuDataDetectedItem;
            return dataDetectorMenuItems;
        }

        if (hitTestResult->isContentEditable()) {
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

@end

#endif // PLATFORM(MAC)

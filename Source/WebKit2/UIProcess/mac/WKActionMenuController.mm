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

#if PLATFORM(MAC)

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
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/URL.h>

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
- (void)_updateActionMenuItems;
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
    [self _updateActionMenuItems];
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

- (void)didPerformActionMenuHitTest:(const ActionMenuHitTestResult&)hitTestResult
{
    // FIXME: This needs to use the WebKit2 callback mechanism to avoid out-of-order replies.
    _state = ActionMenuState::Ready;
    _hitTestResult = hitTestResult;
}

#pragma mark Link actions

- (NSArray *)_defaultMenuItemsForLink
{
    WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult();
    if (!WebCore::protocolIsInHTTPFamily(hitTestResult->absoluteLinkURL()))
        return @[ ];

    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagOpenLinkInDefaultBrowser];
    RetainPtr<NSMenuItem> previewLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPreviewLink];
    RetainPtr<NSMenuItem> readingListItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddLinkToSafariReadingList];

    return @[ openLinkItem.get(), previewLinkItem.get(), [NSMenuItem separatorItem], readingListItem.get() ];
}

- (void)_openURLFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult();
    [[NSWorkspace sharedWorkspace] openURL:[NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()]];
}

- (void)_addToReadingListFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult();
    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
    [service performWithItems:@[ [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()] ]];
}

- (void)_quickLookURLFromActionMenu:(id)sender
{
    WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult();
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
    RetainPtr<NSMenuItem> addToPhotosItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddImageToPhotos];
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
    WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult();
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

- (void)_addImageToPhotos:(id)sender
{
    // FIXME: We shouldn't even add the menu item if this is the case, for now.
    if (![getIKSlideshowClass() canExportToApplication:@"com.apple.Photos"])
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

    [self _updateActionMenuItems];
}

#pragma mark NSSharingServicePickerDelegate implementation

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
        image = [NSImage imageNamed:NSImageNameQuickLookTemplate];
        break;

    case kWKContextActionItemTagAddLinkToSafariReadingList:
        selector = @selector(_addToReadingListFromActionMenu:);
        title = @"Add to Safari Reading List";
        image = [NSImage imageNamed:NSImageNameBookmarksTemplate];
        break;

    case kWKContextActionItemTagCopyImage:
        selector = @selector(_copyImage:);
        title = @"Copy";
        image = webKitBundleImageNamed(@"CopyImageTemplate");
        break;

    case kWKContextActionItemTagAddImageToPhotos:
        selector = @selector(_addImageToPhotos:);
        title = @"Add to Photos";
        image = webKitBundleImageNamed(@"AddImageToPhotosTemplate");
        break;

    case kWKContextActionItemTagSaveImageToDownloads:
        selector = @selector(_saveImageToDownloads:);
        title = @"Save to Downloads";
        image = webKitBundleImageNamed(@"SaveImageToDownloadsTemplate");
        break;

    case kWKContextActionItemTagShareImage:
        title = @"Share";
        image = webKitBundleImageNamed(@"ShareImageTemplate");
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

- (NSArray *)_defaultMenuItems
{
    if (WebHitTestResult* hitTestResult = _page->activeActionMenuHitTestResult()) {
        if (!hitTestResult->absoluteImageURL().isEmpty() && _hitTestResult.image) {
            _type = kWKActionMenuImage;
            return [self _defaultMenuItemsForImage];
        }

        if (!hitTestResult->absoluteLinkURL().isEmpty()) {
            _type = kWKActionMenuLink;
            return [self _defaultMenuItemsForLink];
        }

        if (hitTestResult->isTextNode()) {
            if (DDActionContext *actionContext = _hitTestResult.actionContext.get()) {
                WKSetDDActionContextIsForActionMenu(actionContext);
                actionContext.highlightFrame = [_wkView.window convertRectToScreen:[_wkView convertRect:_hitTestResult.actionBoundingBox toView:nil]];
                NSArray *dataDetectorMenuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_hitTestResult.actionContext mainResult] actionContext:actionContext];
                if (dataDetectorMenuItems.count) {
                    _type = kWKActionMenuDataDetectedItem;
                    return dataDetectorMenuItems;
                }
            }
        }
    }

    _type = kWKActionMenuNone;
    return _state != ActionMenuState::Ready ? @[ [NSMenuItem separatorItem] ] : @[ ];
}

- (void)_updateActionMenuItems
{
    [_wkView.actionMenu removeAllItems];

    NSArray *menuItems = [self _defaultMenuItems];
    if ([_wkView respondsToSelector:@selector(_actionMenuItemsForHitTestResult:defaultActionMenuItems:)])
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(_page->activeActionMenuHitTestResult()) defaultActionMenuItems:menuItems];
    else
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(_page->activeActionMenuHitTestResult()) withType:_type defaultActionMenuItems:menuItems];

    for (NSMenuItem *item in menuItems)
        [_wkView.actionMenu addItem:item];
}

@end

#endif // PLATFORM(MAC)

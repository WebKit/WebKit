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
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NSMenuSPI.h>
#import <WebCore/NSSharingServiceSPI.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/URL.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, ImageKit)
SOFT_LINK_CLASS(ImageKit, IKSlideshow)

using namespace WebCore;
using namespace WebKit;

@interface WKActionMenuController () <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
- (void)_updateActionMenuItems;
- (BOOL)_canAddMediaToPhotos;
- (void)_clearActionMenuState;
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
    _wkView = nil;
    _hitTestResult = ActionMenuHitTestResult();
    _currentActionContext = nil;
}

- (void)wkView:(WKView *)wkView willHandleMouseDown:(NSEvent *)event
{
    [self _clearActionMenuState];
}

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    [_wkView _dismissContentRelativeChildWindows];

    _page->performActionMenuHitTestAtLocation([_wkView convertPoint:event.locationInWindow fromView:nil], false);

    _state = ActionMenuState::Pending;
    [self _updateActionMenuItems];
}

- (BOOL)isMenuForTextContent
{
    return _type == kWKActionMenuReadOnlyText || _type == kWKActionMenuEditableText || _type == kWKActionMenuEditableTextWithSuggestions;
}

- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    if (!menu.numberOfItems)
        return;

    if (_type == kWKActionMenuDataDetectedItem) {
        if (menu.numberOfItems == 1)
            _page->clearSelection();
        else
            _page->selectLastActionMenuRange();
        return;
    }

    if (_type == kWKActionMenuWhitespaceInEditableArea) {
        _page->focusAndSelectLastActionMenuHitTestResult();
        return;
    }

    if (![self isMenuForTextContent]) {
        _page->clearSelection();
        return;
    }

    // Action menus for text should highlight the text so that it is clear what the action menu actions
    // will apply to. If the text is already selected, the menu will use the existing selection.
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult->isSelected())
        _page->selectLastActionMenuRange();
}

- (void)didCloseMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (menu != _wkView.actionMenu)
        return;

    [self _clearActionMenuState];
}

- (void)_clearActionMenuState
{
    if (_currentActionContext && _hasActivatedActionContext) {
        [getDDActionsManagerClass() didUseActions];
        _hasActivatedActionContext = NO;
    }

    _state = ActionMenuState::None;
    _hitTestResult = ActionMenuHitTestResult();
    _type = kWKActionMenuNone;
    _sharingServicePicker = nil;
    _currentActionContext = nil;
    _userData = nil;
}

- (void)didPerformActionMenuHitTest:(const ActionMenuHitTestResult&)hitTestResult userData:(API::Object*)userData
{
    // FIXME: This needs to use the WebKit2 callback mechanism to avoid out-of-order replies.
    _state = ActionMenuState::Ready;
    _hitTestResult = hitTestResult;
    _userData = userData;

    [self _updateActionMenuItems];
}

#pragma mark Link actions

- (NSArray *)_defaultMenuItemsForLink
{
    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:kWKContextActionItemTagOpenLinkInDefaultBrowser];
    RetainPtr<NSMenuItem> readingListItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddLinkToSafariReadingList];

    return @[ openLinkItem.get(), [NSMenuItem separatorItem], [NSMenuItem separatorItem], readingListItem.get() ];
}

- (void)_openURLFromActionMenu:(id)sender
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()]];
}

- (void)_addToReadingListFromActionMenu:(id)sender
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
    [service performWithItems:@[ [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()] ]];
}

#pragma mark Video actions

- (NSArray *)_defaultMenuItemsForVideo
{
    RetainPtr<NSMenuItem> copyVideoURLItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyVideoURL];

    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    RetainPtr<NSMenuItem> saveToDownloadsItem = [self _createActionMenuItemForTag:kWKContextActionItemTagSaveVideoToDownloads];
    RetainPtr<NSMenuItem> shareItem = [self _createActionMenuItemForTag:kWKContextActionItemTagShareVideo];

    String urlToShare = hitTestResult->absoluteMediaURL();
    if (!hitTestResult->isDownloadableMedia()) {
        [saveToDownloadsItem setEnabled:NO];
        urlToShare = _page->mainFrame()->url();
    }

    if (!urlToShare.isEmpty()) {
        _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ urlToShare ]]);
        [_sharingServicePicker setDelegate:self];
        [shareItem setSubmenu:[_sharingServicePicker menu]];
    }

    return @[ copyVideoURLItem.get(), [NSMenuItem separatorItem], saveToDownloadsItem.get(), shareItem.get() ];
}

- (void)_copyVideoURL:(id)sender
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    String urlToCopy = hitTestResult->absoluteMediaURL();
    if (!hitTestResult->isDownloadableMedia())
        urlToCopy = _page->mainFrame()->url();

    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] writeObjects:@[ urlToCopy ]];
}

- (void)_saveVideoToDownloads:(id)sender
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    _page->process().context().download(_page, hitTestResult->absoluteMediaURL());
}

#pragma mark Image actions

- (NSImage *)_hitTestResultImage
{
    RefPtr<SharedMemory> imageSharedMemory = _hitTestResult.imageSharedMemory;
    if (!imageSharedMemory)
        return nil;

    RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithData:[NSData dataWithBytes:imageSharedMemory->data() length:imageSharedMemory->size()]]);
    return nsImage.autorelease();
}

- (NSArray *)_defaultMenuItemsForImage
{
    RetainPtr<NSMenuItem> copyImageItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyImage];
    RetainPtr<NSMenuItem> addToPhotosItem;
    if ([self _canAddMediaToPhotos])
        addToPhotosItem = [self _createActionMenuItemForTag:kWKContextActionItemTagAddImageToPhotos];
    else
        addToPhotosItem = [NSMenuItem separatorItem];
    RetainPtr<NSMenuItem> saveToDownloadsItem = [self _createActionMenuItemForTag:kWKContextActionItemTagSaveImageToDownloads];
    RetainPtr<NSMenuItem> shareItem = [self _createActionMenuItemForTag:kWKContextActionItemTagShareImage];

    if (RetainPtr<NSImage> image = [self _hitTestResultImage]) {
        _sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ image.get() ]]);
        [_sharingServicePicker setDelegate:self];
        [shareItem setSubmenu:[_sharingServicePicker menu]];
    }

    return @[ copyImageItem.get(), addToPhotosItem.get(), saveToDownloadsItem.get(), shareItem.get() ];
}

- (void)_copyImage:(id)sender
{
    RetainPtr<NSImage> image = [self _hitTestResultImage];
    if (!image)
        return;

    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] writeObjects:@[ image.get() ]];
}

- (void)_saveImageToDownloads:(id)sender
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
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

- (BOOL)_canAddMediaToPhotos
{
    return [getIKSlideshowClass() canExportToApplication:@"com.apple.Photos"];
}

- (void)_addImageToPhotos:(id)sender
{
    if (![self _canAddMediaToPhotos])
        return;

    RefPtr<SharedMemory> imageSharedMemory = _hitTestResult.imageSharedMemory;
    if (!imageSharedMemory->size() || _hitTestResult.imageExtension.isEmpty())
        return;

    RetainPtr<NSData> imageData = adoptNS([[NSData alloc] initWithBytes:imageSharedMemory->data() length:imageSharedMemory->size()]);
    RetainPtr<NSString> suggestedFilename = [[[NSProcessInfo processInfo] globallyUniqueString] stringByAppendingPathExtension:_hitTestResult.imageExtension];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString *filePath = pathToPhotoOnDisk(suggestedFilename.get());
        if (!filePath)
            return;

        NSURL *fileURL = [NSURL fileURLWithPath:filePath];
        [imageData writeToURL:fileURL atomically:NO];

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

    actionContext.altMode = YES;
    if ([[getDDActionsManagerClass() sharedManager] respondsToSelector:@selector(hasActionsForResult:actionContext:)]) {
        if (![[getDDActionsManagerClass() sharedManager] hasActionsForResult:actionContext.mainResult actionContext:actionContext])
            return @[ ];
    }

    // Ref our WebPageProxy for use in the blocks below.
    RefPtr<WebPageProxy> page = _page;
    PageOverlay::PageOverlayID overlayID = _hitTestResult.detectedDataOriginatingPageOverlay;
    _currentActionContext = [actionContext contextForView:_wkView altMode:YES interactionStartedHandler:^() {
        page->send(Messages::WebPage::DataDetectorsDidPresentUI(overlayID));
    } interactionChangedHandler:^() {
        if (_hitTestResult.detectedDataTextIndicator)
            page->setTextIndicator(_hitTestResult.detectedDataTextIndicator->data(), false);
        page->send(Messages::WebPage::DataDetectorsDidChangeUI(overlayID));
    } interactionStoppedHandler:^() {
        page->send(Messages::WebPage::DataDetectorsDidHideUI(overlayID));
        page->clearTextIndicator();
    }];

    [_currentActionContext setHighlightFrame:[_wkView.window convertRectToScreen:[_wkView convertRect:_hitTestResult.detectedDataBoundingBox toView:nil]]];

    NSArray *menuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_currentActionContext mainResult] actionContext:_currentActionContext.get()];
    return menuItems;
}

- (NSArray *)_defaultMenuItemsForText
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];
    [pasteItem setEnabled:NO];

    return @[ copyTextItem.get(), [NSMenuItem separatorItem], pasteItem.get() ];
}

- (NSArray *)_defaultMenuItemsForEditableText
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];

    return @[ copyTextItem.get(), [NSMenuItem separatorItem], pasteItem.get() ];
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
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];
    RetainPtr<NSMenuItem> textSuggestionsItem = [self _createActionMenuItemForTag:kWKContextActionItemTagTextSuggestions];

    [textSuggestionsItem setSubmenu:spellingSubMenu.get()];

    return @[ copyTextItem.get(), [NSMenuItem separatorItem], pasteItem.get(), textSuggestionsItem.get() ];
}

- (void)_copySelection:(id)sender
{
    _page->executeEditCommand("copy");
}

- (void)_paste:(id)sender
{
    _page->executeEditCommand("paste");
}

- (void)_changeSelectionToSuggestion:(id)sender
{
    NSString *selectedCorrection = [sender representedObject];
    if (!selectedCorrection)
        return;

    ASSERT([selectedCorrection isKindOfClass:[NSString class]]);

    _page->changeSpellingToWord(selectedCorrection);
}

#pragma mark Whitespace actions

- (NSArray *)_defaultMenuItemsForWhitespaceInEditableArea
{
    RetainPtr<NSMenuItem> copyTextItem = [self _createActionMenuItemForTag:kWKContextActionItemTagCopyText];
    RetainPtr<NSMenuItem> pasteItem = [self _createActionMenuItemForTag:kWKContextActionItemTagPaste];
    [copyTextItem setEnabled:NO];

    return @[ copyTextItem.get(), [NSMenuItem separatorItem], pasteItem.get() ];
}

#pragma mark mailto: and tel: Link actions

- (NSArray *)_defaultMenuItemsForDataDetectableLink
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    RetainPtr<DDActionContext> actionContext = [[getDDActionContextClass() alloc] init];

    // FIXME: Should this show a yellow highlight?
    _currentActionContext = [actionContext contextForView:_wkView altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
    } interactionStoppedHandler:^() {
    }];

    [_currentActionContext setHighlightFrame:[_wkView.window convertRectToScreen:[_wkView convertRect:_hitTestResult.detectedDataBoundingBox toView:nil]]];

    return [[getDDActionsManagerClass() sharedManager] menuItemsForTargetURL:hitTestResult->absoluteLinkURL() actionContext:_currentActionContext.get()];
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
        if (auto* connection = _page->process().connection()) {
            bool receivedReply = connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidPerformActionMenuHitTest>(_page->pageID(), std::chrono::milliseconds(500));
            if (!receivedReply)
                _state = ActionMenuState::TimedOut;
        }
    }

    if (_state != ActionMenuState::Ready)
        [self _updateActionMenuItems];

    if (_currentActionContext) {
        _hasActivatedActionContext = YES;
        if (![getDDActionsManagerClass() shouldUseActionsWithContext:_currentActionContext.get()]) {
            [menu cancelTracking];
            [menu removeAllItems];
        }
    }
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
    bool enabled = true;
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];

    switch (tag) {
    case kWKContextActionItemTagOpenLinkInDefaultBrowser:
        selector = @selector(_openURLFromActionMenu:);
        title = WEB_UI_STRING_KEY("Open", "Open (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuOpenInNewWindow"];
        break;

    case kWKContextActionItemTagPreviewLink:
        ASSERT_NOT_REACHED();
        break;

    case kWKContextActionItemTagAddLinkToSafariReadingList:
        selector = @selector(_addToReadingListFromActionMenu:);
        title = WEB_UI_STRING_KEY("Add to Reading List", "Add to Reading List (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuAddToReadingList"];
        break;

    case kWKContextActionItemTagCopyImage:
        selector = @selector(_copyImage:);
        title = WEB_UI_STRING_KEY("Copy", "Copy (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case kWKContextActionItemTagAddImageToPhotos:
        selector = @selector(_addImageToPhotos:);
        title = WEB_UI_STRING_KEY("Add to Photos", "Add to Photos (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuAddToPhotos"];
        break;

    case kWKContextActionItemTagSaveImageToDownloads:
        selector = @selector(_saveImageToDownloads:);
        title = WEB_UI_STRING_KEY("Save to Downloads", "Save to Downloads (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuSaveToDownloads"];
        break;

    case kWKContextActionItemTagShareImage:
        title = WEB_UI_STRING_KEY("Share (image action menu item)", "Share (image action menu item)", "image action menu item");
        image = [NSImage imageNamed:@"NSActionMenuShare"];
        break;

    case kWKContextActionItemTagCopyText:
        selector = @selector(_copySelection:);
        title = WEB_UI_STRING_KEY("Copy", "Copy (text action menu item)", "text action menu item");
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        enabled = hitTestResult->allowsCopy();
        break;

    case kWKContextActionItemTagPaste:
        selector = @selector(_paste:);
        title = WEB_UI_STRING_KEY("Paste", "Paste (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuPaste"];
        break;

    case kWKContextActionItemTagTextSuggestions:
        title = WEB_UI_STRING_KEY("Suggestions", "Suggestions (action menu item)", "action menu item");
        image = [NSImage imageNamed:@"NSActionMenuSpelling"];
        break;

    case kWKContextActionItemTagCopyVideoURL:
        selector = @selector(_copyVideoURL:);
        title = WEB_UI_STRING_KEY("Copy", "Copy (video action menu item)", "video action menu item");
        image = [NSImage imageNamed:@"NSActionMenuCopy"];
        break;

    case kWKContextActionItemTagSaveVideoToDownloads:
        selector = @selector(_saveVideoToDownloads:);
        title = WEB_UI_STRING_KEY("Save to Downloads", "Save to Downloads (video action menu item)", "video action menu item");
        image = [NSImage imageNamed:@"NSActionMenuSaveToDownloads"];
        break;

    case kWKContextActionItemTagShareVideo:
        title = WEB_UI_STRING_KEY("Share", "Share (video action menu item)", "video action menu item");
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
    [item setEnabled:enabled];
    return item;
}

- (PassRefPtr<WebHitTestResult>)_webHitTestResult
{
    RefPtr<WebHitTestResult> hitTestResult;
    if (_state == ActionMenuState::Ready)
        hitTestResult = WebHitTestResult::create(_hitTestResult.hitTestResult);
    else
        hitTestResult = _page->lastMouseMoveHitTestResult();

        return hitTestResult.release(); 
}

- (NSArray *)_defaultMenuItems
{
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult) {
        _type = kWKActionMenuNone;
        return _state == ActionMenuState::Pending ? @[ [NSMenuItem separatorItem] ] : @[ ];
    }

    String absoluteLinkURL = hitTestResult->absoluteLinkURL();
    if (!absoluteLinkURL.isEmpty()) {
        if (WebCore::protocolIsInHTTPFamily(absoluteLinkURL)) {
            _type = kWKActionMenuLink;
            return [self _defaultMenuItemsForLink];
        }

        if (protocolIs(absoluteLinkURL, "mailto")) {
            _type = kWKActionMenuMailtoLink;
            return [self _defaultMenuItemsForDataDetectableLink];
        }

        if (protocolIs(absoluteLinkURL, "tel")) {
            _type = kWKActionMenuTelLink;
            return [self _defaultMenuItemsForDataDetectableLink];
        }
    }

    if (!hitTestResult->absoluteMediaURL().isEmpty()) {
        _type = kWKActionMenuVideo;
        return [self _defaultMenuItemsForVideo];
    }

    if (!hitTestResult->absoluteImageURL().isEmpty() && _hitTestResult.imageSharedMemory && !_hitTestResult.imageExtension.isEmpty()) {
        _type = kWKActionMenuImage;
        return [self _defaultMenuItemsForImage];
    }

    if (hitTestResult->isTextNode() || hitTestResult->isOverTextInsideFormControlElement()) {
        NSArray *dataDetectorMenuItems = [self _defaultMenuItemsForDataDetectedText];
        if (_currentActionContext) {
            // If this is a data detected item with no menu items, we should not fall back to regular text options.
            if (!dataDetectorMenuItems.count) {
                _type = kWKActionMenuNone;
                return @[ ];
            }
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

    if (hitTestResult->isContentEditable()) {
        _type = kWKActionMenuWhitespaceInEditableArea;
        return [self _defaultMenuItemsForWhitespaceInEditableArea];
    }

    if (hitTestResult->isSelected()) {
        // A selection should present the read-only text menu. It might make more sense to present a new
        // type of menu with just copy, but for the time being, we should stay consistent with text.
        _type = kWKActionMenuReadOnlyText;
        return [self _defaultMenuItemsForText];
    }

    _type = kWKActionMenuNone;
    return _state == ActionMenuState::Pending ? @[ [NSMenuItem separatorItem] ] : @[ ];
}

- (void)_updateActionMenuItems
{
    [_wkView.actionMenu removeAllItems];

    NSArray *menuItems = [self _defaultMenuItems];
    RefPtr<WebHitTestResult> hitTestResult = [self _webHitTestResult];

    if ([_wkView respondsToSelector:@selector(_actionMenuItemsForHitTestResult:defaultActionMenuItems:)])
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(hitTestResult.get()) defaultActionMenuItems:menuItems];
    else
        menuItems = [_wkView _actionMenuItemsForHitTestResult:toAPI(hitTestResult.get()) withType:_type defaultActionMenuItems:menuItems userData:toAPI(_userData.get())];

    for (NSMenuItem *item in menuItems)
        [_wkView.actionMenu addItem:item];

    if (!_wkView.actionMenu.numberOfItems)
        [_wkView.actionMenu cancelTracking];
}

@end

#endif // PLATFORM(MAC)

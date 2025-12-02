/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#import "WebContextMenuProxyMac.h"

#if PLATFORM(MAC)

#import "APIAttachment.h"
#import "APIContextMenuClient.h"
#import "CocoaImage.h"
#import "EditorState.h"
#import "ImageAnalysisUtilities.h"
#import "MenuUtilities.h"
#import "PageClientImplMac.h"
#import "PlatformWritingToolsUtilities.h"
#import "ServicesController.h"
#import "WKMenuItemIdentifiersPrivate.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuItem.h"
#import "WebContextMenuItemData.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "_WKCaptionStyleMenuController.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/ShareableBitmap.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/SpanCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS)
#import "WebExtensionController.h"
#endif

@interface WKUserDataWrapper : NSObject {
    RefPtr<API::Object> _webUserData;
}
- (id)initWithUserData:(API::Object*)userData;
- (API::Object*)userData;
@end

@implementation WKUserDataWrapper

- (id)initWithUserData:(API::Object*)userData
{
    self = [super init];
    if (!self)
        return nil;
    
    _webUserData = userData;
    return self;
}

- (API::Object*)userData
{
    return _webUserData.get();
}

@end

@interface WKSelectionHandlerWrapper : NSObject {
    WTF::Function<void ()> _selectionHandler;
}
- (id)initWithSelectionHandler:(WTF::Function<void ()>&&)selectionHandler;
- (void)executeSelectionHandler;
@end

@implementation WKSelectionHandlerWrapper
- (id)initWithSelectionHandler:(WTF::Function<void ()>&&)selectionHandler
{
    self = [super init];
    if (!self)
        return nil;
    
    _selectionHandler = WTFMove(selectionHandler);
    return self;
}

- (void)executeSelectionHandler
{
    if (_selectionHandler)
        _selectionHandler();
}
@end

@interface WKMenuTarget : NSObject {
    WeakPtr<WebKit::WebContextMenuProxyMac> _menuProxy;
}
+ (WKMenuTarget *)sharedMenuTarget;
- (WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)forwardContextMenuAction:(id)sender;

#if ENABLE(WRITING_TOOLS)
- (void)showWritingTools:(id)sender;
#endif

- (void)performShare:(id)sender;

@end

@implementation WKMenuTarget

+ (WKMenuTarget*)sharedMenuTarget
{
    static NeverDestroyed<RetainPtr<WKMenuTarget>> target = adoptNS([[WKMenuTarget alloc] init]);
    return target.get().get();
}

- (WebKit::WebContextMenuProxyMac*)menuProxy
{
    return _menuProxy.get();
}

- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy
{
    _menuProxy = menuProxy;
}

- (void)forwardContextMenuAction:(id)sender
{
    RefPtr menuProxy = _menuProxy.get();
    if (!menuProxy)
        return;

    RetainPtr<id> representedObject = [sender representedObject];

    // NSMenuItems with a represented selection handler belong solely to the UI process
    // and don't need any further processing after the selection handler is called.
    if ([representedObject isKindOfClass:[WKSelectionHandlerWrapper class]]) {
        [representedObject executeSelectionHandler];
        return;
    }

    ASSERT(!sender || [sender isKindOfClass:NSMenuItem.class]);
    WebKit::WebContextMenuItemData item(WebCore::ContextMenuItemType::Action, static_cast<WebCore::ContextMenuAction>([sender tag]), [sender title], [sender isEnabled], [(NSMenuItem *)sender state] == NSControlStateValueOn);
    if (representedObject)
        item.setUserData(RefPtr { [checked_objc_cast<WKUserDataWrapper>(representedObject.get()) userData] }.get());

    menuProxy->contextMenuItemSelected(item);
}

#if ENABLE(WRITING_TOOLS)
- (void)showWritingTools:(id)sender
{
    RefPtr menuProxy = _menuProxy.get();
    if (!menuProxy)
        return;

    WTRequestedTool tool = (WTRequestedTool)[sender tag];

    menuProxy->handleContextMenuWritingTools(WebKit::convertToWebRequestedTool(tool));
}
#endif

- (void)performShare:(id)sender
{
    if (RefPtr menuProxy = _menuProxy.get())
        menuProxy->handleShareMenuItem();
}

@end

@interface WKMenuDelegate : NSObject <NSMenuDelegate, WKCaptionStyleMenuControllerDelegate> {
    WeakPtr<WebKit::WebContextMenuProxyMac> _menuProxy;
}
- (instancetype)initWithMenuProxy:(WebKit::WebContextMenuProxyMac&)menuProxy;
@end

@implementation WKMenuDelegate

-(instancetype)initWithMenuProxy:(WebKit::WebContextMenuProxyMac&)menuProxy
{
    if (!(self = [super init]))
        return nil;

    _menuProxy = menuProxy;

    return self;
}

#pragma mark - NSMenuDelegate

- (void)menuWillOpen:(NSMenu *)menu
{
    Ref { *_menuProxy }->protectedPage()->didShowContextMenu();
}

- (void)menuDidClose:(NSMenu *)menu
{
    Ref { *_menuProxy }->protectedPage()->didDismissContextMenu();
}

#pragma mark - WKCaptionStyleMenuControllerDelegate

- (void)captionStyleMenuWillOpen:(NSMenu *)menu
{
    Ref { *_menuProxy }->captionStyleMenuWillOpen();
}

- (void)captionStyleMenuDidClose:(NSMenu *)menu
{
    Ref { *_menuProxy }->captionStyleMenuDidClose();
}

@end

namespace WebKit {
using namespace WebCore;

WebContextMenuProxyMac::WebContextMenuProxyMac(NSView *webView, WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(page, WTFMove(context), userData)
    , m_webView(webView)
    , m_frameInfo(WTFMove(frameInfo))
{
}

WebContextMenuProxyMac::~WebContextMenuProxyMac()
{
    [m_menu cancelTracking];
}

void WebContextMenuProxyMac::contextMenuItemSelected(const WebContextMenuItemData& item)
{
#if ENABLE(SERVICE_CONTROLS)
    clearServicesMenu();
#endif

    protectedPage()->contextMenuItemSelected(item, m_frameInfo);
}

#if ENABLE(WRITING_TOOLS)
void WebContextMenuProxyMac::handleContextMenuWritingTools(WebCore::WritingTools::RequestedTool tool)
{
    protectedPage()->handleContextMenuWritingTools(tool);
}
#endif

void WebContextMenuProxyMac::handleShareMenuItem()
{
    RetainPtr shareMenuItem = createShareMenuItem(ShareMenuItemType::Popover);
    [shareMenuItem setMenu:m_menu.get()];
    [[NSApplication sharedApplication] sendAction:[shareMenuItem action] to:retainPtr([shareMenuItem target]).get() from:shareMenuItem.get()];
}

#if ENABLE(SERVICE_CONTROLS)
void WebContextMenuProxyMac::setupServicesMenu()
{
    bool includeEditorServices = m_context.controlledDataIsEditable();
    bool hasControlledImage = m_context.controlledImage();
    bool isPDFAttachment = false;
    auto attachment = protectedPage()->attachmentForIdentifier(m_context.controlledImageAttachmentID());
    if (attachment)
        isPDFAttachment = attachment->utiType() == String(UTTypePDF.identifier);
    NSArray *items = nil;
    RetainPtr<NSItemProvider> itemProvider;
    if (hasControlledImage) {
        if (attachment)
            itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:attachment->associatedElementNSData().get() typeIdentifier:attachment->utiType().createNSString().get()]);
        else {
            RefPtr<ShareableBitmap> image = m_context.controlledImage();
            if (!image)
                return;
            RetainPtr cgImage = image->createPlatformImage(DontCopyBackingStore);
            auto nsImage = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:image->size()]);

            itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:retainPtr([nsImage TIFFRepresentation]).get() typeIdentifier:UTTypeTIFF.identifier]);
        }
        items = @[ itemProvider.get() ];
        
    } else if (RetainPtr selection = m_context.controlledSelection().nsAttributedString())
        items = @[ selection.get() ];
    else if (isPDFAttachment) {
        itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:attachment->associatedElementNSData().get() typeIdentifier:attachment->utiType().createNSString().get()]);
        items = @[ itemProvider.get() ];
    } else {
        LOG_ERROR("No service controlled item represented in the context");
        return;
    }

    RetainPtr<NSSharingServicePicker> picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:items]);
    [picker setStyle:hasControlledImage || isPDFAttachment ? NSSharingServicePickerStyleRollover : NSSharingServicePickerStyleTextSelection];
    [picker setDelegate:[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate]];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:picker.get()];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setFiltersEditingServices:!includeEditorServices];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setHandlesEditingReplacement:includeEditorServices];
    
    NSRect imageRect = m_context.controlledImageBounds();
    auto webView = m_webView.get();
    imageRect = [webView convertRect:imageRect toView:nil];
    imageRect = [retainPtr([webView window]) convertRectToScreen:imageRect];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setSourceFrame:imageRect];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setAttachmentID:m_context.controlledImageAttachmentID()];

    m_menu = adoptNS([[picker menu] copy]);

    if (!hasControlledImage)
        [m_menu setShowsStateColumn:YES];

    appendRemoveBackgroundItemToControlledImageMenuIfNeeded();

    // Explicitly add a menu item for each telephone number that is in the selection.
    Vector<RetainPtr<NSMenuItem>> telephoneNumberMenuItems;

    for (auto& telephoneNumber : m_context.selectedTelephoneNumbers()) {
        if (RetainPtr item = menuItemForTelephoneNumber(telephoneNumber)) {
            [item setIndentationLevel:1];
            telephoneNumberMenuItems.append(WTFMove(item));
        }
    }

    if (!telephoneNumberMenuItems.isEmpty()) {
        if (m_menu)
            [m_menu insertItem:[NSMenuItem separatorItem] atIndex:0];
        else
            m_menu = adoptNS([[NSMenu alloc] init]);
        int itemPosition = 0;
        auto groupEntry = adoptNS([[NSMenuItem alloc] initWithTitle:menuItemTitleForTelephoneNumberGroup().get() action:nil keyEquivalent:@""]);
        [groupEntry setEnabled:NO];
        [m_menu insertItem:groupEntry.get() atIndex:itemPosition++];
        for (auto& menuItem : telephoneNumberMenuItems)
            [m_menu insertItem:menuItem.get() atIndex:itemPosition++];
    }

    // If there is no services menu, then the existing services on the system have changed, so refresh that list of services.
    // If <rdar://problem/17954709> is resolved then we can more accurately keep the list up to date without this call.
    if (!m_menu)
        ServicesController::singleton().refreshExistingServices();
}

void WebContextMenuProxyMac::appendRemoveBackgroundItemToControlledImageMenuIfNeeded()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RefPtr page = this->page();
    if (!page || !page->protectedPreferences()->removeBackgroundEnabled())
        return;

    auto context = m_context.controlledImageElementContext();
    if (!context)
        return;

    page->shouldAllowRemoveBackground(*context, [protectedThis = Ref { *this }, weakMenu = WeakObjCPtr<NSMenu> { m_menu.get() }](bool shouldAllow) mutable {
        if (!shouldAllow)
            return;

        RefPtr imageBitmap = protectedThis->m_context.controlledImage();
        if (!imageBitmap)
            return;

        RetainPtr image = imageBitmap->createPlatformImage(DontCopyBackingStore);
        if (!image)
            return;

        requestBackgroundRemoval(image.get(), [protectedThis = WTFMove(protectedThis), weakMenu = WTFMove(weakMenu)](CGImageRef result) {
            if (!result)
                return;

            auto strongMenu = weakMenu.get();
            if (!strongMenu)
                return;

            auto removeBackgroundItem = adoptNS([[NSMenuItem alloc] initWithTitle:contextMenuItemTitleRemoveBackground().createNSString().get() action:@selector(removeBackground) keyEquivalent:@""]);
            [removeBackgroundItem setImage:[NSImage imageWithSystemSymbolName:@"person.fill.viewfinder" accessibilityDescription:contextMenuItemTitleRemoveBackground().createNSString().get()]];
            [removeBackgroundItem setTarget:WKSharingServicePickerDelegate.sharedSharingServicePickerDelegate];
            [removeBackgroundItem setAction:@selector(removeBackground)];
            [removeBackgroundItem setIndentationLevel:[strongMenu itemArray].lastObject.indentationLevel];
            [strongMenu addItem:removeBackgroundItem.get()];

            protectedThis->m_copySubjectResult = result;
        });
    });
#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
}

void WebContextMenuProxyMac::showServicesMenu()
{
    setupServicesMenu();

    auto webView = m_webView.get();
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];
    [m_menu popUpMenuPositioningItem:nil atLocation:m_context.menuLocation() inView:webView.get()];
}

void WebContextMenuProxyMac::clearServicesMenu()
{
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nullptr];
    m_menu = nullptr;
}

void WebContextMenuProxyMac::removeBackgroundFromControlledImage()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RefPtr page = this->page();
    if (!page)
        return;

    auto elementContext = m_context.controlledImageElementContext();
    if (!elementContext)
        return;

    auto [data, type] = imageDataForRemoveBackground(m_copySubjectResult.get(), m_context.controlledImageMIMEType().createCFString().get());
    if (!data)
        return;

    page->replaceImageForRemoveBackground(*elementContext, { String(type.get()) }, span(data.get()));
#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
}

#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
static void updateMenuItemImage(NSMenuItem *menuItem, const WebCore::ContextMenuAction& action, const String& title)
{
    bool useAlternateImage = false;

    switch (action) {
    case ContextMenuItemTagMediaPlayPause:
        useAlternateImage = title == contextMenuItemTagMediaPause();
        break;
    case ContextMenuItemTagShowSpellingPanel:
        useAlternateImage = title == contextMenuItemTagShowSpellingPanel(false);
        break;
    case ContextMenuItemTagShowSubstitutions:
        useAlternateImage = title == contextMenuItemTagShowSubstitutions(false);
        break;
    case ContextMenuItemTagToggleMediaControls:
        useAlternateImage = title == contextMenuItemTagShowMediaControls();
        break;
    case ContextMenuItemTagToggleVideoEnhancedFullscreen:
        useAlternateImage = title == contextMenuItemTagExitVideoEnhancedFullscreen();
        break;
    case ContextMenuItemTagToggleVideoFullscreen:
        useAlternateImage = title == contextMenuItemTagExitVideoFullscreen();
        break;
    case ContextMenuItemTagToggleVideoViewer:
        useAlternateImage = title == contextMenuItemTagExitVideoViewer();
        break;
    default:
        useAlternateImage = false;
        break;
    }

    addImageToMenuItem(menuItem, action, useAlternateImage);
}
#endif

RetainPtr<NSMenuItem> WebContextMenuProxyMac::createShareMenuItem(ShareMenuItemType type)
{
    ASSERT(m_context.webHitTestResultData());
    auto hitTestData = m_context.webHitTestResultData().value();

    auto items = adoptNS([[NSMutableArray alloc] init]);

    if (!hitTestData.absoluteLinkURL.isEmpty()) {
        auto absoluteLinkURL = URL({ }, hitTestData.absoluteLinkURL);
        if (!absoluteLinkURL.isEmpty()) {
            if (RetainPtr url = absoluteLinkURL.createNSURL())
                [items addObject:url.get()];
        }
    }

    if (hitTestData.isDownloadableMedia && !hitTestData.absoluteMediaURL.isEmpty()) {
        auto downloadableMediaURL = URL({ }, hitTestData.absoluteMediaURL);
        if (!downloadableMediaURL.isEmpty()) {
            if (RetainPtr url = downloadableMediaURL.createNSURL())
                [items addObject:url.get()];
        }
    }

    bool usePlaceholder = type == ShareMenuItemType::Placeholder;
    if (hitTestData.imageSharedMemory) {
        if (usePlaceholder)
            [items addObject:adoptNS([[NSImage alloc] init]).get()];
        else if (auto image = adoptNS([[NSImage alloc] initWithData:Ref { *hitTestData.imageSharedMemory }->toNSData().get()])) {
            RetainPtr title = hitTestData.imageText.createNSString();
            if (![title length])
                title = WEB_UI_NSSTRING(@"Image", "Fallback title for images in the share sheet");

            auto activityItem = adoptNS([[NSPreviewRepresentingActivityItem alloc] initWithItem:image.get() title:title.get() image:image.get() icon:nil]);
            [items addObject:activityItem.get()];
        }
    }

    if (!m_context.selectedText().isEmpty())
        [items addObject:m_context.selectedText().createNSString().get()];

    if (![items count])
        return nil;

    RetainPtr sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:items.get()]);
    RetainPtr shareMenuItem = [sharingServicePicker standardShareMenuItemRelativeToRect:hitTestData.elementBoundingBox ofView:m_webView.get().get() preferredEdge:NSMinYEdge];

    if (!shareMenuItem)
        return nil;

    if (usePlaceholder) {
        RetainPtr placeholder = adoptNS([[NSMenuItem alloc] initWithTitle:retainPtr([shareMenuItem title]).get() action:@selector(performShare:) keyEquivalent:@""]);
        [placeholder setTarget:[WKMenuTarget sharedMenuTarget]];
#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
        [placeholder _setActionImage:[shareMenuItem _actionImage]];
#endif
        shareMenuItem = WTFMove(placeholder);
    } else
        [shareMenuItem setRepresentedObject:sharingServicePicker.get()];

    [shareMenuItem setIdentifier:_WKMenuItemIdentifierShareMenu];
    return shareMenuItem;
}
#endif

void WebContextMenuProxyMac::show()
{
#if ENABLE(SERVICE_CONTROLS)
    if (m_context.isServicesMenu()) {
        WebContextMenuProxy::useContextMenuItems({ });
        return;
    }
#endif

    if (!showAfterPostProcessingContextData())
        WebContextMenuProxy::show();
}

bool WebContextMenuProxyMac::showAfterPostProcessingContextData()
{
#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    if (!protectedPage()->protectedPreferences()->contextMenuQRCodeDetectionEnabled())
        return false;

    ASSERT(m_context.webHitTestResultData());
    auto hitTestData = m_context.webHitTestResultData().value();

    if (!hitTestData.absoluteLinkURL.isEmpty())
        return false;

    if (auto bitmap = hitTestData.imageBitmap) {
        RetainPtr image = bitmap->createPlatformImage(DontCopyBackingStore);
        requestPayloadForQRCode(image.get(), [this, protectedThis = Ref { *this }](NSString *result) mutable {
            m_context.setQRCodePayloadString(result);
            WebContextMenuProxy::show();
        });

        return true;
    }

    if (RefPtr potentialQRCodeNodeSnapshotImage = m_context.potentialQRCodeNodeSnapshotImage()) {
        RetainPtr image = potentialQRCodeNodeSnapshotImage->createPlatformImage(DontCopyBackingStore);
        requestPayloadForQRCode(image.get(), [this, protectedThis = Ref { *this }](NSString *result) mutable {
            RefPtr potentialQRCodeViewportSnapshotImage = m_context.potentialQRCodeViewportSnapshotImage();
            if (!potentialQRCodeViewportSnapshotImage || result.length) {
                m_context.setQRCodePayloadString(result);
                WebContextMenuProxy::show();
                return;
            }

            RetainPtr fallbackImage = potentialQRCodeViewportSnapshotImage->createPlatformImage(DontCopyBackingStore);
            requestPayloadForQRCode(fallbackImage.get(), [this, protectedThis = Ref { *this }](NSString *result) mutable {
                m_context.setQRCodePayloadString(result);
                WebContextMenuProxy::show();
            });
        });

        return true;
    }
#endif // ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

    return false;
}

static RetainPtr<NSString> menuItemIdentifier(const WebCore::ContextMenuAction action)
{
    switch (action) {
    case ContextMenuItemTagCopy:
        return _WKMenuItemIdentifierCopy;

    case ContextMenuItemTagCopyImageToClipboard:
        return _WKMenuItemIdentifierCopyImage;

    case ContextMenuItemTagCopyLinkToClipboard:
        return _WKMenuItemIdentifierCopyLink;

    case ContextMenuItemTagCopyMediaLinkToClipboard:
        return _WKMenuItemIdentifierCopyMediaLink;

    case ContextMenuItemTagDownloadImageToDisk:
        return _WKMenuItemIdentifierDownloadImage;

    case ContextMenuItemTagDownloadLinkToDisk:
        return _WKMenuItemIdentifierDownloadLinkedFile;

    case ContextMenuItemTagDownloadMediaToDisk:
        return _WKMenuItemIdentifierDownloadMedia;

    case ContextMenuItemTagGoBack:
        return _WKMenuItemIdentifierGoBack;

    case ContextMenuItemTagGoForward:
        return _WKMenuItemIdentifierGoForward;

    case ContextMenuItemTagInspectElement:
        return _WKMenuItemIdentifierInspectElement;

    case ContextMenuItemTagLookUpInDictionary:
        return _WKMenuItemIdentifierLookUp;

    case ContextMenuItemTagAddHighlightToCurrentQuickNote:
        return _WKMenuItemIdentifierAddHighlightToCurrentQuickNote;
        
    case ContextMenuItemTagAddHighlightToNewQuickNote:
        return _WKMenuItemIdentifierAddHighlightToNewQuickNote;

    case ContextMenuItemTagCopyLinkWithHighlight:
        return _WKMenuItemIdentifierCopyLinkWithHighlight;

    case ContextMenuItemTagOpenFrameInNewWindow:
        return _WKMenuItemIdentifierOpenFrameInNewWindow;

    case ContextMenuItemTagOpenImageInNewWindow:
        return _WKMenuItemIdentifierOpenImageInNewWindow;

    case ContextMenuItemTagOpenLink:
        return _WKMenuItemIdentifierOpenLink;

    case ContextMenuItemTagOpenLinkInNewWindow:
        return _WKMenuItemIdentifierOpenLinkInNewWindow;

    case ContextMenuItemTagOpenMediaInNewWindow:
        return _WKMenuItemIdentifierOpenMediaInNewWindow;

    case ContextMenuItemTagPaste:
        return _WKMenuItemIdentifierPaste;

    case ContextMenuItemTagReload:
        return _WKMenuItemIdentifierReload;

    case ContextMenuItemTagLookUpImage:
        return _WKMenuItemIdentifierRevealImage;

    case ContextMenuItemTagSearchWeb:
        return _WKMenuItemIdentifierSearchWeb;

    case ContextMenuItemTagToggleMediaControls:
        return _WKMenuItemIdentifierShowHideMediaControls;

    case ContextMenuItemTagShowMediaStats:
        return _WKMenuItemIdentifierShowHideMediaStats;

    case ContextMenuItemTagToggleVideoEnhancedFullscreen:
        return _WKMenuItemIdentifierToggleEnhancedFullScreen;

    case ContextMenuItemTagToggleVideoFullscreen:
        return _WKMenuItemIdentifierToggleFullScreen;

    case ContextMenuItemTagToggleVideoViewer:
        return _WKMenuItemIdentifierToggleVideoViewer;

    case ContextMenuItemTagTranslate:
        return _WKMenuItemIdentifierTranslate;

    case ContextMenuItemTagWritingTools:
        return _WKMenuItemIdentifierWritingTools;

    case ContextMenuItemTagProofread:
        return _WKMenuItemIdentifierProofread;

    case ContextMenuItemTagRewrite:
        return _WKMenuItemIdentifierRewrite;

    case ContextMenuItemTagSummarize:
        return _WKMenuItemIdentifierSummarize;

    case ContextMenuItemTagCopySubject:
        return _WKMenuItemIdentifierCopySubject;

    case ContextMenuItemTagShareMenu:
        return _WKMenuItemIdentifierShareMenu;

    case ContextMenuItemTagSpeechMenu:
        return _WKMenuItemIdentifierSpeechMenu;

    case ContextMenuItemTagSpellingMenu:
        return _WKMenuItemIdentifierSpellingMenu;

    case ContextMenuItemTagShowSpellingPanel:
        return _WKMenuItemIdentifierShowSpellingPanel;

    case ContextMenuItemTagCheckSpelling:
        return _WKMenuItemIdentifierCheckSpelling;

    case ContextMenuItemTagCheckSpellingWhileTyping:
        return _WKMenuItemIdentifierCheckSpellingWhileTyping;

    case ContextMenuItemTagCheckGrammarWithSpelling:
        return _WKMenuItemIdentifierCheckGrammarWithSpelling;

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    case ContextMenuItemTagPlayAllAnimations:
        return _WKMenuItemIdentifierPlayAllAnimations;

    case ContextMenuItemTagPauseAllAnimations:
        return _WKMenuItemIdentifierPauseAllAnimations;

    case ContextMenuItemTagPlayAnimation:
        return _WKMenuItemIdentifierPlayAnimation;

    case ContextMenuItemTagPauseAnimation:
        return _WKMenuItemIdentifierPauseAnimation;
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

    default:
        return nil;
    }
}

static RetainPtr<NSMenuItem> createMenuActionItem(const WebContextMenuItemData& item)
{
    auto type = item.type();
    ASSERT_UNUSED(type, type == WebCore::ContextMenuItemType::Action || type == WebCore::ContextMenuItemType::CheckableAction);

    RetainPtr menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title().createNSString().get() action:@selector(forwardContextMenuAction:) keyEquivalent:@""]);

    [menuItem setTag:item.action()];
    [menuItem setEnabled:item.enabled()];
    [menuItem setState:item.checked() ? NSControlStateValueOn : NSControlStateValueOff];
    [menuItem setIndentationLevel:item.indentationLevel()];
    [menuItem setTarget:[WKMenuTarget sharedMenuTarget]];
    [menuItem setIdentifier:menuItemIdentifier(item.action()).get()];

    if (item.userData())
        [menuItem setRepresentedObject:adoptNS([[WKUserDataWrapper alloc] initWithUserData:item.protectedUserData().get()]).get()];

    return menuItem;
}

void WebContextMenuProxyMac::getContextMenuFromItems(const Vector<WebContextMenuItemData>& items, CompletionHandler<void(NSMenu *)>&& completionHandler)
{
    auto menu = adoptNS([[NSMenu alloc] initWithTitle:@""]);
    [menu setAutoenablesItems:NO];

    if (items.isEmpty()) {
        completionHandler(menu.get());
        return;
    }

    auto filteredItems = items;
    auto webView = m_webView.get();

    bool isPopover = retainPtr(webView.get().window).get()._childWindowOrderingPriority == NSWindowChildOrderingPriorityPopover;
    bool isLookupDisabled = [NSUserDefaults.standardUserDefaults boolForKey:@"LULookupDisabled"];

    if (isLookupDisabled || isPopover) {
        filteredItems.removeAllMatching([] (auto& item) {
            return item.action() == WebCore::ContextMenuItemTagLookUpInDictionary;
        });
    }

    std::optional<WebContextMenuItemData> copySubjectItem;
    std::optional<WebContextMenuItemData> lookUpImageItem;

#if ENABLE(IMAGE_ANALYSIS)
    for (auto& item : filteredItems) {
        switch (item.action()) {
        case ContextMenuItemTagLookUpImage: {
            ASSERT(!lookUpImageItem);
            item.setEnabled(false);
            lookUpImageItem = { item };
            break;
        }

        case ContextMenuItemTagCopySubject: {
            ASSERT(!copySubjectItem);
            item.setEnabled(false);
            copySubjectItem = { item };
            break;
        }

        default:
            break;
        }
    }
#endif // ENABLE(IMAGE_ANALYSIS)

#if HAVE(TRANSLATION_UI_SERVICES)
    if (!protectedPage()->canHandleContextMenuTranslation() || isPopover) {
        filteredItems.removeAllMatching([] (auto& item) {
            return item.action() == ContextMenuItemTagTranslate;
        });
    }
#endif

#if ENABLE(WRITING_TOOLS)
    if (!protectedPage()->canHandleContextMenuWritingTools() || isPopover) {
        filteredItems.removeAllMatching([] (auto& item) {
            auto action = item.action();
            return action == ContextMenuItemTagWritingTools || action == ContextMenuItemTagProofread || action == ContextMenuItemTagRewrite || action == ContextMenuItemTagSummarize;
        });
    }
#endif

    ASSERT(m_context.webHitTestResultData());
    auto hitTestData = m_context.webHitTestResultData().value();
    
    auto imageURL = URL { hitTestData.absoluteImageURL };
    auto imageBitmap = hitTestData.imageBitmap;

    RetainPtr sparseMenuItems = [NSPointerArray strongObjectsPointerArray];
    auto insertMenuItem = makeBlockPtr([protectedThis = Ref { *this }, weakPage = WeakPtr { page() }, imageURL = WTFMove(imageURL), imageBitmap = WTFMove(imageBitmap), lookUpImageItem = WTFMove(lookUpImageItem), copySubjectItem = WTFMove(copySubjectItem), completionHandler = WTFMove(completionHandler), itemsRemaining = filteredItems.size(), menu = WTFMove(menu), sparseMenuItems](NSMenuItem *item, NSUInteger index) mutable {
        ASSERT(index < [sparseMenuItems count]);
        ASSERT(![sparseMenuItems pointerAtIndex:index]);
        [sparseMenuItems replacePointerAtIndex:index withPointer:item];
        if (--itemsRemaining)
            return;

        [menu setItemArray:[sparseMenuItems allObjects]];

        RefPtr page = weakPage.get();
        if (page && imageBitmap) {
#if ENABLE(IMAGE_ANALYSIS)
            if (lookUpImageItem) {
                page->computeHasVisualSearchResults(imageURL, *imageBitmap, [protectedThis, lookUpImageItem = WTFMove(*lookUpImageItem)] (bool hasVisualSearchResults) mutable {
                    if (hasVisualSearchResults) {
                        NSInteger index = [protectedThis->m_menu indexOfItemWithTag:lookUpImageItem.action()];
                        if (index >= 0)
                            [protectedThis->m_menu itemAtIndex:index].enabled = YES;
                    }
                });
            }
#else
            UNUSED_PARAM(imageURL);
#endif
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
            if (copySubjectItem) {
                if (RetainPtr image = imageBitmap->createPlatformImage()) {
                    protectedThis->m_copySubjectResult = nullptr;
                    requestBackgroundRemoval(image.get(), [weakPage, protectedThis, copySubjectItem = WTFMove(*copySubjectItem)](auto result) {
                        if (!result)
                            return;

                        RefPtr page = weakPage.get();
                        if (!page)
                            return;

                        protectedThis->m_copySubjectResult = result;

                        NSInteger index = [protectedThis->m_menu indexOfItemWithTag:copySubjectItem.action()];
                        if (index >= 0)
                            [protectedThis->m_menu itemAtIndex:index].enabled = YES;
                    });
                }
            }
#else
            UNUSED_PARAM(copySubjectItem);
#endif
        }

        completionHandler(menu.get());
    });

    for (size_t i = 0; i < filteredItems.size(); ++i) {
        [sparseMenuItems addPointer:nullptr];
        getContextMenuItem(filteredItems[i], [insertMenuItem, i](NSMenuItem *menuItem) {
            insertMenuItem(menuItem, i);
        });
    }
}

void WebContextMenuProxyMac::getContextMenuItem(const WebContextMenuItemData& item, CompletionHandler<void(NSMenuItem *)>&& completionHandler)
{
#if ENABLE(SERVICE_CONTROLS)
    if (item.action() == ContextMenuItemTagShareMenu) {
        completionHandler(createShareMenuItem(ShareMenuItemType::Placeholder).get());
        return;
    }
#endif

#if ENABLE(WRITING_TOOLS) && !ENABLE(TOP_LEVEL_WRITING_TOOLS_CONTEXT_MENU_ITEMS)
    if (item.action() == ContextMenuItemTagWritingTools) {
        RetainPtr menuItem = [NSMenuItem standardWritingToolsMenuItem];
        [[menuItem submenu] setAutoenablesItems:NO];

        for (NSMenuItem *subItem in [menuItem submenu].itemArray) {
            if (subItem.isSeparatorItem)
                continue;

            bool shouldEnableItem = [&] {
                RefPtr page = this->page();
                if (!page)
                    return false;

                return page->shouldEnableWritingToolsRequestedTool(convertToWebRequestedTool((WTRequestedTool)[subItem tag]));
            }();

            subItem.enabled = static_cast<BOOL>(shouldEnableItem);
            subItem.target = WKMenuTarget.sharedMenuTarget;
        }

        completionHandler(menuItem.get());
        return;
    }
#endif

    switch (item.type()) {
    case WebCore::ContextMenuItemType::Action:
    case WebCore::ContextMenuItemType::CheckableAction: {
        RetainPtr menuItem = createMenuActionItem(item);
#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
        updateMenuItemImage(menuItem.get(), item.action(), item.title());
#endif
        completionHandler(menuItem.get());
        return;
    }

    case WebCore::ContextMenuItemType::Separator:
        completionHandler(NSMenuItem.separatorItem);
        return;

    case WebCore::ContextMenuItemType::Submenu: {
        RetainPtr menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title().createNSString().get() action:nullptr keyEquivalent:@""]);
        [menuItem setEnabled:item.enabled()];
        [menuItem setIndentationLevel:item.indentationLevel()];
        [menuItem setIdentifier:menuItemIdentifier(item.action()).get()];
#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
        updateMenuItemImage(menuItem.get(), item.action(), item.title());
#endif

        if (item.action() == ContextMenuItemCaptionDisplayStyleSubmenu) {
            RetainPtr controller = captionStyleMenuController();
            [menuItem setSubmenu:[controller captionStyleMenu]];
            completionHandler(menuItem.get());
            return;
        }

        getContextMenuFromItems(item.submenu(), [menuItem = WTFMove(menuItem), completionHandler = WTFMove(completionHandler)](NSMenu *menu) mutable {
            [menuItem setSubmenu:menu];
            completionHandler(menuItem.get());
        });
        return;
    }
    }
}

void WebContextMenuProxyMac::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
#if ENABLE(SERVICE_CONTROLS)
    if (m_context.isServicesMenu()) {
        ASSERT(items.isEmpty());
        showServicesMenu();
        return;
    }
#endif

    RefPtr page = this->page();
    if (page->contextMenuClient().canShowContextMenu()) {
        page->contextMenuClient().showContextMenu(*page, m_context.menuLocation(), items);
        return;
    }

    ASSERT(items.isEmpty());
    if (!m_menu)
        return;

    auto webView = m_webView.get();
    NSPoint menuLocation = [webView convertPoint:m_context.menuLocation() toView:nil];
    auto event = page->createSyntheticEventForContextMenu(menuLocation);
    [NSMenu popUpContextMenu:m_menu.get() withEvent:event.get() forView:webView.get()];
}

void WebContextMenuProxyMac::useContextMenuItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    if (items.isEmpty() || !page() || page()->contextMenuClient().canShowContextMenu()) {
        WebContextMenuProxy::useContextMenuItems(WTFMove(items));
        return;
    }

    auto data = WTF::map(items, [](auto& item) {
        return item->data();
    });

    getContextMenuFromItems(data, [this, protectedThis = Ref { *this }](NSMenu *menu) mutable {
        if (!page()) {
            WebContextMenuProxy::useContextMenuItems({ });
            return;
        }

        [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];

        auto menuFromProposedMenu = [this, protectedThis = Ref { *this }] (RetainPtr<NSMenu>&& menu) {
            m_menu = WTFMove(menu);
            [m_menu setDelegate:RetainPtr { menuDelegate() }.get()];

            WebContextMenuProxy::useContextMenuItems({ });
        };

        if (m_context.type() != ContextMenuContext::Type::ContextMenu) {
            menuFromProposedMenu(menu);
            return;
        }

        ASSERT(m_context.webHitTestResultData());
        Ref page = *this->page();

#if ENABLE(WK_WEB_EXTENSIONS)
        if (RefPtr webExtensionController = page->webExtensionController())
            webExtensionController->addItemsToContextMenu(page, m_context, menu);
#endif

        page->contextMenuClient().menuFromProposedMenu(page, menu, m_context, m_userData.protectedObject().get(), WTFMove(menuFromProposedMenu));
    });
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView.get() window];
}

NSMenu *WebContextMenuProxyMac::platformMenu() const
{
    return m_menu.get();
}

static RetainPtr<NSDictionary> contentsOfContextMenuItem(NSMenuItem *item)
{
    RetainPtr result = adoptNS([[NSMutableDictionary alloc] init]);

    if (item.title.length)
        result.get()[@"title"] = item.title;

    if (item.isSeparatorItem)
        result.get()[@"separator"] = @YES;
    else if (!item.enabled)
        result.get()[@"enabled"] = @NO;

    if (NSInteger indentationLevel = item.indentationLevel)
        result.get()[@"indentationLevel"] = [NSNumber numberWithInteger:indentationLevel];

    if (item.state == NSControlStateValueOn)
        result.get()[@"checked"] = @YES;

    if (RetainPtr<NSArray<NSMenuItem *>> submenuItems = item.submenu.itemArray) {
        RetainPtr children = adoptNS([[NSMutableArray alloc] initWithCapacity:[submenuItems count]]);
        for (NSMenuItem *submenuItem : submenuItems.get())
            [children addObject:contentsOfContextMenuItem(submenuItem).get()];
        result.get()[@"children"] = children.get();
    }

    return result;
}

RetainPtr<NSArray> WebContextMenuProxyMac::platformData() const
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] init]);

    if (RetainPtr<NSArray<NSMenuItem *>> submenuItems = [m_menu itemArray]) {
        RetainPtr children = adoptNS([[NSMutableArray alloc] initWithCapacity:[submenuItems count]]);
        for (NSMenuItem *submenuItem : submenuItems.get())
            [children addObject:contentsOfContextMenuItem(submenuItem).get()];
        [result addObject:@{ @"children": children.get() }];
    }

    return result;
}

WKCaptionStyleMenuController *WebContextMenuProxyMac::captionStyleMenuController()
{
    if (!m_captionStyleMenuController) {
        m_captionStyleMenuController = [WKCaptionStyleMenuController menuController];
        [m_captionStyleMenuController setDelegate:RetainPtr { menuDelegate() }.get()];
    }
    return m_captionStyleMenuController.get();
}

WKMenuDelegate *WebContextMenuProxyMac::menuDelegate()
{
    if (!m_menuDelegate)
        m_menuDelegate = adoptNS([[WKMenuDelegate alloc] initWithMenuProxy:*this]);
    return m_menuDelegate.get();
}

void WebContextMenuProxyMac::captionStyleMenuWillOpen()
{
    if (auto identifier = m_context.mediaElementIdentifier())
        protectedPage()->showCaptionDisplaySettingsPreview(m_frameInfo, *identifier);
}

void WebContextMenuProxyMac::captionStyleMenuDidClose()
{
    if (auto identifier = m_context.mediaElementIdentifier())
        protectedPage()->hideCaptionDisplaySettingsPreview(m_frameInfo, *identifier);
}

} // namespace WebKit

#endif // PLATFORM(MAC)

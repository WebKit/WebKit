/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#import "APIContextMenuClient.h"
#import "MenuUtilities.h"
#import "ServicesController.h"
#import "ShareableBitmap.h"
#import "WKMenuItemIdentifiersPrivate.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuItem.h"
#import "WebContextMenuItemData.h"
#import "WebPageProxy.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <wtf/RetainPtr.h>

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
    WebKit::WebContextMenuProxyMac* _menuProxy;
}
+ (WKMenuTarget *)sharedMenuTarget;
- (WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)forwardContextMenuAction:(id)sender;
@end

@implementation WKMenuTarget

+ (WKMenuTarget*)sharedMenuTarget
{
    static WKMenuTarget* target = [[WKMenuTarget alloc] init];
    return target;
}

- (WebKit::WebContextMenuProxyMac*)menuProxy
{
    return _menuProxy;
}

- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy
{
    _menuProxy = menuProxy;
}

- (void)forwardContextMenuAction:(id)sender
{
    id representedObject = [sender representedObject];

    // NSMenuItems with a represented selection handler belong solely to the UI process
    // and don't need any further processing after the selection handler is called.
    if ([representedObject isKindOfClass:[WKSelectionHandlerWrapper class]]) {
        [representedObject executeSelectionHandler];
        return;
    }

    ASSERT(!sender || [sender isKindOfClass:NSMenuItem.class]);
    WebKit::WebContextMenuItemData item(WebCore::ActionType, static_cast<WebCore::ContextMenuAction>([sender tag]), [sender title], [sender isEnabled], [(NSMenuItem *)sender state] == NSControlStateValueOn);
    if (representedObject) {
        ASSERT([representedObject isKindOfClass:[WKUserDataWrapper class]]);
        item.setUserData([static_cast<WKUserDataWrapper *>(representedObject) userData]);
    }

    _menuProxy->contextMenuItemSelected(item);
}

@end

namespace WebKit {
using namespace WebCore;

WebContextMenuProxyMac::WebContextMenuProxyMac(NSView *webView, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(page, WTFMove(context), userData)
    , m_webView(webView)
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

    page()->contextMenuItemSelected(item);
}

#if ENABLE(SERVICE_CONTROLS)
void WebContextMenuProxyMac::setupServicesMenu()
{
    bool includeEditorServices = m_context.controlledDataIsEditable();
    bool hasControlledImage = m_context.controlledImage();
    NSArray *items = nil;
    if (hasControlledImage) {
        RefPtr<ShareableBitmap> image = m_context.controlledImage();
        if (!image)
            return;

        auto cgImage = image->makeCGImage();
        auto nsImage = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:image->size()]);

        auto itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:[nsImage TIFFRepresentation] typeIdentifier:(__bridge NSString *)kUTTypeTIFF]);
        items = @[ itemProvider.get() ];
    } else if (!m_context.controlledSelectionData().isEmpty()) {
        auto selectionData = adoptNS([[NSData alloc] initWithBytes:static_cast<const void*>(m_context.controlledSelectionData().data()) length:m_context.controlledSelectionData().size()]);
        auto selection = adoptNS([[NSAttributedString alloc] initWithRTFD:selectionData.get() documentAttributes:nil]);

        items = @[ selection.get() ];
    } else {
        LOG_ERROR("No service controlled item represented in the context");
        return;
    }

    RetainPtr<NSSharingServicePicker> picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:items]);
    [picker setStyle:hasControlledImage ? NSSharingServicePickerStyleRollover : NSSharingServicePickerStyleTextSelection];
    [picker setDelegate:[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate]];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:picker.get()];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setFiltersEditingServices:!includeEditorServices];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setHandlesEditingReplacement:includeEditorServices];

    m_menu = adoptNS([[picker menu] copy]);

    if (!hasControlledImage)
        [m_menu setShowsStateColumn:YES];

    // Explicitly add a menu item for each telephone number that is in the selection.
    Vector<RetainPtr<NSMenuItem>> telephoneNumberMenuItems;

    for (auto& telephoneNumber : m_context.selectedTelephoneNumbers()) {
        if (NSMenuItem *item = menuItemForTelephoneNumber(telephoneNumber)) {
            [item setIndentationLevel:1];
            telephoneNumberMenuItems.append(item);
        }
    }

    if (!telephoneNumberMenuItems.isEmpty()) {
        if (m_menu)
            [m_menu insertItem:[NSMenuItem separatorItem] atIndex:0];
        else
            m_menu = adoptNS([[NSMenu alloc] init]);
        int itemPosition = 0;
        auto groupEntry = adoptNS([[NSMenuItem alloc] initWithTitle:menuItemTitleForTelephoneNumberGroup() action:nil keyEquivalent:@""]);
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

static void getStandardShareMenuItem(NSArray *items, void (^completionHandler)(NSMenuItem *))
{
#if HAVE(NSSHARINGSERVICEPICKER_ASYNC_MENUS)
    // FIXME (<rdar://problem/54551500>): Replace this with the async variant of +[NSMenuItem standardShareMenuItemForItems:] when it's available.
    auto sharingServicePicker = adoptNS([[NSSharingServicePicker alloc] initWithItems:items]);
    [sharingServicePicker setStyle:NSSharingServicePickerStyleMenu];
    [sharingServicePicker getMenuWithCompletion:^(NSMenu *shareMenu) {
        ASSERT(isMainThread());
        shareMenu.delegate = (id <NSMenuDelegate>)sharingServicePicker.get();
        auto shareMenuItem = adoptNS([[NSMenuItem alloc] initWithTitle:WEB_UI_STRING("Share", "Title for Share context menu item.") action:nil keyEquivalent:@""]);
        [shareMenuItem setRepresentedObject:sharingServicePicker.get()];
        [shareMenuItem setSubmenu:shareMenu];
        completionHandler(shareMenuItem.get());
    }];
#else
    completionHandler([NSMenuItem standardShareMenuItemForItems:items]);
#endif
}

void WebContextMenuProxyMac::getShareMenuItem(CompletionHandler<void(NSMenuItem *)>&& completionHandler)
{
    const WebHitTestResultData& hitTestData = m_context.webHitTestResultData();

    auto items = adoptNS([[NSMutableArray alloc] init]);

    if (!hitTestData.absoluteLinkURL.isEmpty()) {
        auto absoluteLinkURL = URL({ }, hitTestData.absoluteLinkURL);
        if (!absoluteLinkURL.isEmpty())
            [items addObject:(NSURL *)absoluteLinkURL];
    }

    if (hitTestData.isDownloadableMedia && !hitTestData.absoluteMediaURL.isEmpty()) {
        auto downloadableMediaURL = URL({ }, hitTestData.absoluteMediaURL);
        if (!downloadableMediaURL.isEmpty())
            [items addObject:(NSURL *)downloadableMediaURL];
    }

    if (hitTestData.imageSharedMemory && hitTestData.imageSize) {
        if (auto image = adoptNS([[NSImage alloc] initWithData:[NSData dataWithBytes:(unsigned char*)hitTestData.imageSharedMemory->data() length:hitTestData.imageSize]]))
            [items addObject:image.get()];
    }

    if (!m_context.selectedText().isEmpty())
        [items addObject:(NSString *)m_context.selectedText()];

    if (![items count]) {
        completionHandler(nil);
        return;
    }

    getStandardShareMenuItem(items.get(), makeBlockPtr([completionHandler = WTFMove(completionHandler), protectedThis = makeRef(*this), this](NSMenuItem *item) mutable {
        if (!item) {
            completionHandler(nil);
            return;
        }

        NSSharingServicePicker *sharingServicePicker = item.representedObject;
        WKSharingServicePickerDelegate *sharingServicePickerDelegate = WKSharingServicePickerDelegate.sharedSharingServicePickerDelegate;
        sharingServicePicker.delegate = sharingServicePickerDelegate;

        sharingServicePickerDelegate.filtersEditingServices = NO;
        sharingServicePickerDelegate.handlesEditingReplacement = NO;
        sharingServicePickerDelegate.menuProxy = this;

        // Setting the picker lets the delegate retain it to keep it alive, but this picker is kept alive by the menu item.
        sharingServicePickerDelegate.picker = nil;

        item.identifier = _WKMenuItemIdentifierShareMenu;

        completionHandler(item);
    }).get());
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

    WebContextMenuProxy::show();
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
    
    bool isPopover = webView.get().window._childWindowOrderingPriority == NSWindowChildOrderingPriorityPopover;
    bool isLookupDisabled = [NSUserDefaults.standardUserDefaults boolForKey:@"LULookupDisabled"];
    
    if (isLookupDisabled || isPopover) {
        filteredItems.removeAllMatching([] (auto& item) {
            return item.action() == WebCore::ContextMenuItemTagLookUpInDictionary;
        });
    }

    auto sparseMenuItems = retainPtr([NSPointerArray strongObjectsPointerArray]);
    auto insertMenuItem = makeBlockPtr([completionHandler = WTFMove(completionHandler), itemsRemaining = filteredItems.size(), menu = WTFMove(menu), sparseMenuItems](NSMenuItem *item, NSUInteger index) mutable {
        ASSERT(index < [sparseMenuItems count]);
        ASSERT(![sparseMenuItems pointerAtIndex:index]);
        [sparseMenuItems replacePointerAtIndex:index withPointer:item];
        if (--itemsRemaining)
            return;

        [menu setItemArray:[sparseMenuItems allObjects]];
        completionHandler(menu.get());
    });

    for (size_t i = 0; i < filteredItems.size(); ++i) {
        [sparseMenuItems addPointer:nullptr];
        getContextMenuItem(filteredItems[i], [insertMenuItem, i](NSMenuItem *menuItem) {
            insertMenuItem(menuItem, i);
        });
    }
}

static NSString *menuItemIdentifier(const WebCore::ContextMenuAction action)
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

    case ContextMenuItemTagSearchWeb:
        return _WKMenuItemIdentifierSearchWeb;

    case ContextMenuItemTagToggleMediaControls:
        return _WKMenuItemIdentifierShowHideMediaControls;

    case ContextMenuItemTagToggleVideoEnhancedFullscreen:
        return _WKMenuItemIdentifierToggleEnhancedFullScreen;

    case ContextMenuItemTagToggleVideoFullscreen:
        return _WKMenuItemIdentifierToggleFullScreen;

    case ContextMenuItemTagShareMenu:
        return _WKMenuItemIdentifierShareMenu;

    case ContextMenuItemTagSpeechMenu:
        return _WKMenuItemIdentifierSpeechMenu;

    default:
        return nil;
    }
}

void WebContextMenuProxyMac::getContextMenuItem(const WebContextMenuItemData& item, CompletionHandler<void(NSMenuItem *)>&& completionHandler)
{
#if ENABLE(SERVICE_CONTROLS)
    if (item.action() == ContextMenuItemTagShareMenu) {
        getShareMenuItem(WTFMove(completionHandler));
        return;
    }
#endif

    switch (item.type()) {
    case WebCore::ActionType:
    case WebCore::CheckableActionType: {
        auto menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title() action:@selector(forwardContextMenuAction:) keyEquivalent:@""]);

        [menuItem setTag:item.action()];
        [menuItem setEnabled:item.enabled()];
        [menuItem setState:item.checked() ? NSControlStateValueOn : NSControlStateValueOff];
        [menuItem setTarget:[WKMenuTarget sharedMenuTarget]];
        [menuItem setIdentifier:menuItemIdentifier(item.action())];

        if (item.userData()) {
            auto wrapper = adoptNS([[WKUserDataWrapper alloc] initWithUserData:item.userData()]);
            [menuItem setRepresentedObject:wrapper.get()];
        }

        completionHandler(menuItem.get());
        return;
    }

    case WebCore::SeparatorType:
        completionHandler(NSMenuItem.separatorItem);
        return;

    case WebCore::SubmenuType: {
        getContextMenuFromItems(item.submenu(), [action = item.action(), completionHandler = WTFMove(completionHandler), enabled = item.enabled(), title = item.title()](NSMenu *menu) mutable {
            auto menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:title action:nullptr keyEquivalent:@""]);
            [menuItem setEnabled:enabled];
            [menuItem setSubmenu:menu];
            [menuItem setIdentifier:menuItemIdentifier(action)];
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

    if (page()->contextMenuClient().canShowContextMenu()) {
        page()->contextMenuClient().showContextMenu(*page(), m_context.menuLocation(), items);
        return;
    }

    ASSERT(items.isEmpty());
    if (!m_menu)
        return;

    auto webView = m_webView.get();
    NSPoint menuLocation = [webView convertPoint:m_context.menuLocation() toView:nil];
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:menuLocation modifierFlags:0 timestamp:0 windowNumber:[webView window].windowNumber context:nil eventNumber:0 clickCount:0 pressure:0];
    [NSMenu popUpContextMenu:m_menu.get() withEvent:event forView:webView.get()];
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

    getContextMenuFromItems(data, [this, protectedThis = makeRef(*this)](NSMenu *menu) mutable {
        if (!page()) {
            WebContextMenuProxy::useContextMenuItems({ });
            return;
        }

        [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];
        page()->contextMenuClient().menuFromProposedMenu(*page(), menu, m_context.webHitTestResultData(), m_userData.object(), [this, protectedThis = WTFMove(protectedThis)](RetainPtr<NSMenu>&& menu) {
            m_menu = WTFMove(menu);
            WebContextMenuProxy::useContextMenuItems({ });
        });
    });
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView window];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "DataReference.h"
#import "MenuUtilities.h"
#import "PageClientImplMac.h"
#import "ServicesController.h"
#import "ShareableBitmap.h"
#import "StringUtilities.h"
#import "WKMenuItemIdentifiersPrivate.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuItem.h"
#import "WebContextMenuItemData.h"
#import "WebContextMenuListenerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>
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

WebContextMenuProxyMac::WebContextMenuProxyMac(NSView* webView, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(WTFMove(context), userData)
    , m_webView(webView)
    , m_page(page)
{
}

WebContextMenuProxyMac::~WebContextMenuProxyMac()
{
    [m_menu cancelTracking];

    if (m_contextMenuListener) {
        m_contextMenuListener->invalidate();
        m_contextMenuListener = nullptr;
    }
}

void WebContextMenuProxyMac::contextMenuItemSelected(const WebContextMenuItemData& item)
{
#if ENABLE(SERVICE_CONTROLS)
    clearServicesMenu();
#endif

    m_page.contextMenuItemSelected(item);
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

    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];
    [m_menu popUpMenuPositioningItem:nil atLocation:m_context.menuLocation() inView:m_webView];
}

void WebContextMenuProxyMac::clearServicesMenu()
{
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nullptr];
    m_menu = nullptr;
}

RetainPtr<NSMenuItem> WebContextMenuProxyMac::createShareMenuItem()
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

    if (![items count])
        return nil;

    RetainPtr<NSMenuItem> item = [NSMenuItem standardShareMenuItemForItems:items.get()];
    if (!item)
        return nil;

    NSSharingServicePicker *sharingServicePicker = [item representedObject];
    sharingServicePicker.delegate = [WKSharingServicePickerDelegate sharedSharingServicePickerDelegate];

    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setFiltersEditingServices:NO];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setHandlesEditingReplacement:NO];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];

    // Setting the picker lets the delegate retain it to keep it alive, but this picker is kept alive by the menu item.
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nil];

    [item setIdentifier:_WKMenuItemIdentifierShareMenu];

    return item;
}
#endif

void WebContextMenuProxyMac::show()
{
    Ref<WebPageProxy> protect(m_page);

#if ENABLE(SERVICE_CONTROLS)
    if (m_context.isServicesMenu()) {
        showServicesMenu();
        return;
    }
#endif

    showContextMenu();
}

RetainPtr<NSMenu> WebContextMenuProxyMac::createContextMenuFromItems(const Vector<WebContextMenuItemData>& items)
{
    auto menu = adoptNS([[NSMenu alloc] initWithTitle:@""]);
    [menu setAutoenablesItems:NO];

    for (auto& item : items) {
        if (auto menuItem = createContextMenuItem(item))
            [menu addItem:menuItem.get()];
    }

    return menu;
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

RetainPtr<NSMenuItem> WebContextMenuProxyMac::createContextMenuItem(const WebContextMenuItemData& item)
{
#if ENABLE(SERVICE_CONTROLS)
    if (item.action() == ContextMenuItemTagShareMenu)
        return createShareMenuItem();
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

        return menuItem;
    }

    case WebCore::SeparatorType:
        return [NSMenuItem separatorItem];

    case WebCore::SubmenuType: {
        auto menuItem = adoptNS([[NSMenuItem alloc] initWithTitle:item.title() action:nullptr keyEquivalent:@""]);
        [menuItem setEnabled:item.enabled()];
        [menuItem setSubmenu:createContextMenuFromItems(item.submenu()).get()];
        [menuItem setIdentifier:menuItemIdentifier(item.action())];

        return menuItem;
    }
    }
}

void WebContextMenuProxyMac::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    if (m_page.contextMenuClient().showContextMenu(m_page, m_context.menuLocation(), items))
        return;

    if (items.isEmpty())
        return;

    Vector<WebContextMenuItemData> data;
    data.reserveInitialCapacity(items.size());
    for (auto& item : items)
        data.uncheckedAppend(item->data());
    
    auto menu = createContextMenuFromItems(data);
    [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];
    m_page.contextMenuClient().menuFromProposedMenu(m_page, menu.get(), m_context.webHitTestResultData(), m_userData.object(), [this, protectedThis = makeRef(*this)] (RetainPtr<NSMenu>&& menu) {
        m_menu = WTFMove(menu);
        NSPoint menuLocation = [m_webView convertPoint:m_context.menuLocation() toView:nil];
        NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:menuLocation modifierFlags:0 timestamp:0 windowNumber:m_webView.window.windowNumber context:nil eventNumber:0 clickCount:0 pressure:0];
        [NSMenu popUpContextMenu:m_menu.get() withEvent:event forView:m_webView];
        
        if (m_contextMenuListener) {
            m_contextMenuListener->invalidate();
            m_contextMenuListener = nullptr;
        }
    });
}

void WebContextMenuProxyMac::showContextMenu()
{
    Vector<Ref<WebContextMenuItem>> proposedAPIItems;
    for (auto& item : m_context.menuItems())
        proposedAPIItems.append(WebContextMenuItem::create(item));

    if (m_contextMenuListener) {
        m_contextMenuListener->invalidate();
        m_contextMenuListener = nullptr;
    }

    m_contextMenuListener = WebContextMenuListenerProxy::create(this);

    m_page.contextMenuClient().getContextMenuFromProposedMenu(m_page, WTFMove(proposedAPIItems), *m_contextMenuListener, m_context.webHitTestResultData(), m_page.process().transformHandlesToObjects(m_userData.object()).get());
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView window];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

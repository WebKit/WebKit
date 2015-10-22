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
#import "PageClientImpl.h"
#import "ServicesController.h"
#import "ShareableBitmap.h"
#import "StringUtilities.h"
#import "WKSharingServicePickerDelegate.h"
#import "WKView.h"
#import "WebContextMenuItem.h"
#import "WebContextMenuItemData.h"
#import "WebProcessProxy.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSSharingServiceSPI.h>
#import <WebKitSystemInterface.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

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
    std::function<void ()> _selectionHandler;
}
- (id)initWithSelectionHandler:(std::function<void ()>)selectionHandler;
- (void)executeSelectionHandler;
@end

@implementation WKSelectionHandlerWrapper
- (id)initWithSelectionHandler:(std::function<void ()>)selectionHandler
{
    self = [super init];
    if (!self)
        return nil;
    
    _selectionHandler = selectionHandler;
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

    WebKit::WebContextMenuItemData item(ActionType, static_cast<ContextMenuAction>([sender tag]), [sender title], [sender isEnabled], [sender state] == NSOnState);
    if (representedObject) {
        ASSERT([representedObject isKindOfClass:[WKUserDataWrapper class]]);
        item.setUserData([static_cast<WKUserDataWrapper *>(representedObject) userData]);
    }

    _menuProxy->contextMenuItemSelected(item);
}

@end

namespace WebKit {

WebContextMenuProxyMac::WebContextMenuProxyMac(WKView* webView, WebPageProxy& page, const ContextMenuContextData& context, const UserData& userData)
    : WebContextMenuProxy(context, userData)
    , m_webView(webView)
    , m_page(page)
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

    m_page.contextMenuItemSelected(item);
}

static void populateNSMenu(NSMenu* menu, const Vector<RetainPtr<NSMenuItem>>& menuItemVector)
{
    for (unsigned i = 0; i < menuItemVector.size(); ++i) {
        NSInteger oldState = [menuItemVector[i].get() state];
        [menu addItem:menuItemVector[i].get()];
        [menuItemVector[i].get() setState:oldState];
    }
}

template<typename ItemType> static Vector<RetainPtr<NSMenuItem>> nsMenuItemVector(WebContextMenuProxyMac&, const Vector<ItemType>&);

static RetainPtr<NSMenuItem> nsMenuItem(WebContextMenuProxyMac& contextMenuProxy, const WebContextMenuItemData& item)
{
#if ENABLE(SERVICE_CONTROLS)
    if (item.action() == ContextMenuItemTagShareMenu)
        return contextMenuProxy.shareMenuItem().platformDescription();
#endif

    switch (item.type()) {
    case ActionType:
    case CheckableActionType: {
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:nsStringFromWebCoreString(item.title()) action:@selector(forwardContextMenuAction:) keyEquivalent:@""];
        [menuItem setTag:item.action()];
        [menuItem setEnabled:item.enabled()];
        [menuItem setState:item.checked() ? NSOnState : NSOffState];

        if (item.userData()) {
            WKUserDataWrapper *wrapper = [[WKUserDataWrapper alloc] initWithUserData:item.userData()];
            [menuItem setRepresentedObject:wrapper];
            [wrapper release];
        }

        return adoptNS(menuItem);
        break;
    }
    case SeparatorType:
        return [NSMenuItem separatorItem];
        break;
    case SubmenuType: {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:nsStringFromWebCoreString(item.title())];
        [menu setAutoenablesItems:NO];
        populateNSMenu(menu, nsMenuItemVector(contextMenuProxy, item.submenu()));
            
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:nsStringFromWebCoreString(item.title()) action:@selector(forwardContextMenuAction:) keyEquivalent:@""];
        [menuItem setEnabled:item.enabled()];
        [menuItem setSubmenu:menu];
        [menu release];

        return adoptNS(menuItem);
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

static RetainPtr<NSMenuItem> nsMenuItem(WebContextMenuProxyMac& contextMenuProxy, const RefPtr<WebContextMenuItem>& item)
{
    if (NativeContextMenuItem* nativeItem = item->nativeContextMenuItem())
        return nativeItem->nsMenuItem();

    return nsMenuItem(contextMenuProxy, item->data());
}

template<typename ItemType> static Vector<RetainPtr<NSMenuItem>> nsMenuItemVector(WebContextMenuProxyMac& contextMenuProxy, const Vector<ItemType>& items)
{
    Vector<RetainPtr<NSMenuItem>> result;

    unsigned size = items.size();
    result.reserveCapacity(size);
    for (auto& item : items) {
        if (auto menuItem = nsMenuItem(contextMenuProxy, item))
            result.uncheckedAppend(menuItem);
    }

    WKMenuTarget* target = [WKMenuTarget sharedMenuTarget];
    for (auto& item : result)
        [item.get() setTarget:target];
    
    return result;
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

#ifdef __LP64__
        auto itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:[nsImage TIFFRepresentation] typeIdentifier:(__bridge NSString *)kUTTypeTIFF]);
        items = @[ itemProvider.get() ];
#else
        items = @[ ];
#endif
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
        NSMenuItem *groupEntry = [[NSMenuItem alloc] initWithTitle:menuItemTitleForTelephoneNumberGroup() action:nil keyEquivalent:@""];
        [groupEntry setEnabled:NO];
        [m_menu insertItem:groupEntry atIndex:itemPosition++];
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

ContextMenuItem WebContextMenuProxyMac::shareMenuItem()
{
    const WebHitTestResultData& hitTestData = m_context.webHitTestResultData();

    URL absoluteLinkURL;
    if (!hitTestData.absoluteLinkURL.isEmpty())
        absoluteLinkURL = URL(ParsedURLString, hitTestData.absoluteLinkURL);

    URL downloadableMediaURL;
    if (!hitTestData.absoluteMediaURL.isEmpty() && hitTestData.isDownloadableMedia)
        downloadableMediaURL = URL(ParsedURLString, hitTestData.absoluteMediaURL);

    RetainPtr<NSImage> image;
    if (hitTestData.imageSharedMemory && hitTestData.imageSize)
        image = adoptNS([[NSImage alloc] initWithData:[NSData dataWithBytes:(unsigned char*)hitTestData.imageSharedMemory->data() length:hitTestData.imageSize]]);

    ContextMenuItem item = ContextMenuItem::shareMenuItem(absoluteLinkURL, downloadableMediaURL, image.get(), m_context.selectedText());
    if (item.isNull())
        return item;

    NSMenuItem *nsItem = item.platformDescription();

    NSSharingServicePicker *sharingServicePicker = [nsItem representedObject];
    sharingServicePicker.delegate = [WKSharingServicePickerDelegate sharedSharingServicePickerDelegate];

    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setFiltersEditingServices:NO];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setHandlesEditingReplacement:NO];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];

    // Setting the picker lets the delegate retain it to keep it alive, but this picker is kept alive by the menu item.
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nil];

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

void WebContextMenuProxyMac::showContextMenu()
{
    Vector<RefPtr<WebContextMenuItem>> proposedAPIItems;
    for (auto& item : m_context.menuItems())
        proposedAPIItems.append(WebContextMenuItem::create(item));

    Vector<RefPtr<WebContextMenuItem>> clientItems;
    bool useProposedItems = true;

    if (m_page.contextMenuClient().getContextMenuFromProposedMenu(m_page, proposedAPIItems, clientItems, m_context.webHitTestResultData(), m_page.process().transformHandlesToObjects(m_userData.object()).get()))
        useProposedItems = false;

    const Vector<RefPtr<WebContextMenuItem>>& items = useProposedItems ? proposedAPIItems : clientItems;

    if (items.isEmpty())
        return;

    m_menu = [[NSMenu alloc] initWithTitle:@""];
    [m_menu setAutoenablesItems:NO];

    populateNSMenu(m_menu.get(), nsMenuItemVector(*this, items));

    [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];
    [m_menu popUpMenuPositioningItem:nil atLocation:m_context.menuLocation() inView:m_webView];
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView window];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

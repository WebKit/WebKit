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

WebContextMenuProxyMac::WebContextMenuProxyMac(WKView* webView, WebPageProxy* page)
    : m_webView(webView)
    , m_page(page)
{
    ASSERT(m_page);
}

WebContextMenuProxyMac::~WebContextMenuProxyMac()
{
    if (m_popup)
        [m_popup setControlView:nil];
}

void WebContextMenuProxyMac::contextMenuItemSelected(const WebContextMenuItemData& item)
{
#if ENABLE(SERVICE_CONTROLS)
    clearServicesMenu();
#endif

    m_page->contextMenuItemSelected(item);
}

static void populateNSMenu(NSMenu* menu, const Vector<RetainPtr<NSMenuItem>>& menuItemVector)
{
    for (unsigned i = 0; i < menuItemVector.size(); ++i) {
        NSInteger oldState = [menuItemVector[i].get() state];
        [menu addItem:menuItemVector[i].get()];
        [menuItemVector[i].get() setState:oldState];
    }
}

template<typename ItemType> static Vector<RetainPtr<NSMenuItem>> nsMenuItemVector(const Vector<ItemType>&);

static RetainPtr<NSMenuItem> nsMenuItem(const WebContextMenuItemData& item)
{
    switch (item.type()) {
    case ActionType:
    case CheckableActionType: {
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:nsStringFromWebCoreString(item.title()) action:@selector(forwardContextMenuAction:) keyEquivalent:@""];
        [menuItem setTag:item.action()];
        [menuItem setEnabled:item.enabled()];
        [menuItem setState:item.checked() ? NSOnState : NSOffState];

        if (std::function<void ()> selectionHandler = item.selectionHandler()) {
            WKSelectionHandlerWrapper *wrapper = [[WKSelectionHandlerWrapper alloc] initWithSelectionHandler:selectionHandler];
            [menuItem setRepresentedObject:wrapper];
            [wrapper release];
        } else if (item.userData()) {
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
        populateNSMenu(menu, nsMenuItemVector(item.submenu()));
            
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

static RetainPtr<NSMenuItem> nsMenuItem(const RefPtr<WebContextMenuItem>& item)
{
    if (NativeContextMenuItem* nativeItem = item->nativeContextMenuItem())
        return nativeItem->nsMenuItem();

    ASSERT(item->data());
    return nsMenuItem(*item->data());
}

template<typename ItemType> static Vector<RetainPtr<NSMenuItem>> nsMenuItemVector(const Vector<ItemType>& items)
{
    Vector<RetainPtr<NSMenuItem>> result;

    unsigned size = items.size();
    result.reserveCapacity(size);
    for (auto& item : items)
        result.uncheckedAppend(nsMenuItem(item));

    WKMenuTarget* target = [WKMenuTarget sharedMenuTarget];
    for (auto& item : result)
        [item.get() setTarget:target];
    
    return result;
}

#if ENABLE(SERVICE_CONTROLS)

void WebContextMenuProxyMac::setupServicesMenu(const ContextMenuContextData& context)
{
    bool includeEditorServices = context.controlledDataIsEditable();
    bool hasControlledImage = context.controlledImage();
    NSArray *items = nil;
    if (hasControlledImage) {
        RefPtr<ShareableBitmap> image = context.controlledImage();
        if (!image)
            return;

        RetainPtr<CGImageRef> cgImage = image->makeCGImage();
        RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:image->size()]);
        items = @[ nsImage.get() ];
    } else if (!context.controlledSelectionData().isEmpty()) {
        RetainPtr<NSData> selectionData = adoptNS([[NSData alloc] initWithBytes:(void*)context.controlledSelectionData().data() length:context.controlledSelectionData().size()]);
        RetainPtr<NSAttributedString> selection = adoptNS([[NSAttributedString alloc] initWithRTFD:selectionData.get() documentAttributes:nil]);

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

    m_servicesMenu = adoptNS([[picker menu] copy]);

    if (!hasControlledImage)
        [m_servicesMenu setShowsStateColumn:YES];

    // Explicitly add a menu item for each telephone number that is in the selection.
    const Vector<String>& selectedTelephoneNumbers = context.selectedTelephoneNumbers();
    Vector<RetainPtr<NSMenuItem>> telephoneNumberMenuItems;
    for (auto& telephoneNumber : selectedTelephoneNumbers) {
        if (NSMenuItem *item = menuItemForTelephoneNumber(telephoneNumber)) {
            [item setIndentationLevel:1];
            telephoneNumberMenuItems.append(item);
        }
    }

    if (!telephoneNumberMenuItems.isEmpty()) {
        if (m_servicesMenu)
            [m_servicesMenu insertItem:[NSMenuItem separatorItem] atIndex:0];
        else
            m_servicesMenu = adoptNS([[NSMenu alloc] init]);
        int itemPosition = 0;
        NSMenuItem *groupEntry = [[NSMenuItem alloc] initWithTitle:menuItemTitleForTelephoneNumberGroup() action:nil keyEquivalent:@""];
        [groupEntry setEnabled:NO];
        [m_servicesMenu insertItem:groupEntry atIndex:itemPosition++];
        for (auto& menuItem : telephoneNumberMenuItems)
            [m_servicesMenu insertItem:menuItem.get() atIndex:itemPosition++];
    }

    // If there is no services menu, then the existing services on the system have changed, so refresh that list of services.
    // If <rdar://problem/17954709> is resolved then we can more accurately keep the list up to date without this call.
    if (!m_servicesMenu)
        ServicesController::singleton().refreshExistingServices();
}

void WebContextMenuProxyMac::clearServicesMenu()
{
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nullptr];
    m_servicesMenu = nullptr;
}
#endif

void WebContextMenuProxyMac::populate(const Vector<RefPtr<WebContextMenuItem>>& items, const ContextMenuContextData& context)
{
#if ENABLE(SERVICE_CONTROLS)
    if (context.needsServicesMenu()) {
        setupServicesMenu(context);
        return;
    }
#endif

    if (m_popup)
        [m_popup removeAllItems];
    else {
        m_popup = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popup setUsesItemFromMenu:NO];
        [m_popup setAutoenablesItems:NO];
        [m_popup setAltersStateOfSelectedItem:NO];
    }

    NSMenu* menu = [m_popup menu];
    populateNSMenu(menu, nsMenuItemVector(items));
}

void WebContextMenuProxyMac::showContextMenu(const IntPoint& menuLocation, const Vector<RefPtr<WebContextMenuItem>>& items, const ContextMenuContextData& context)
{
#if ENABLE(SERVICE_CONTROLS)
    if (items.isEmpty() && !context.needsServicesMenu())
        return;
#else
    if (items.isEmpty())
        return;
#endif

    populate(items, context);

    [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];

    NSRect menuRect = NSMakeRect(menuLocation.x(), menuLocation.y(), 0, 0);

#if ENABLE(SERVICE_CONTROLS)
    if (context.needsServicesMenu())
        [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];

    if (!m_servicesMenu)
        [m_popup attachPopUpWithFrame:menuRect inView:m_webView];

    NSMenu *menu = m_servicesMenu ? m_servicesMenu.get() : [m_popup menu];

    // Telephone number and service menus must use the [NSMenu popUpMenuPositioningItem:atLocation:inView:] API.
    // FIXME: That API is better than WKPopupContextMenu. In the future all menus should use either it
    // or the [NSMenu popUpContextMenu:withEvent:forView:] API, depending on the menu type.
    // Then we could get rid of NSPopUpButtonCell, custom metrics, and WKPopupContextMenu.
    if (context.needsServicesMenu()) {
        [menu popUpMenuPositioningItem:nil atLocation:menuLocation inView:m_webView];
        hideContextMenu();
        return;
    }

#else
    [m_popup attachPopUpWithFrame:menuRect inView:m_webView];

    NSMenu *menu = [m_popup menu];
#endif

    // These values were borrowed from AppKit to match their placement of the menu.
    NSRect titleFrame = [m_popup titleRectForBounds:menuRect];
    if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
        titleFrame = menuRect;
    float vertOffset = roundf((NSMaxY(menuRect) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
    NSPoint location = NSMakePoint(NSMinX(menuRect), NSMaxY(menuRect) - vertOffset);

    location = [m_webView convertPoint:location toView:nil];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    location = [m_webView.window convertBaseToScreen:location];
#pragma clang diagnostic pop

    Ref<WebContextMenuProxyMac> protect(*this);

    WKPopupContextMenu(menu, location);

    hideContextMenu();
}

void WebContextMenuProxyMac::hideContextMenu()
{
    [m_popup dismissPopUp];
}

void WebContextMenuProxyMac::cancelTracking()
{
    [[m_popup menu] cancelTracking];
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView window];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

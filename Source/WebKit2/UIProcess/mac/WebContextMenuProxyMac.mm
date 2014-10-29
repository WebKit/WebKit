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
#import "WebContext.h"
#import "WebContextMenuItemData.h"
#import "WebProcessProxy.h"
#import "WKView.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <WebKitSystemInterface.h>
#import <wtf/RetainPtr.h>

#if ENABLE(SERVICE_CONTROLS)
#import <AppKit/NSSharingService.h>

#if __has_include(<AppKit/NSSharingService_Private.h>)
#import <AppKit/NSSharingService_Private.h>
#else
typedef enum {
    NSSharingServicePickerStyleMenu = 0,
    NSSharingServicePickerStyleRollover = 1,
    NSSharingServicePickerStyleTextSelection = 2
} NSSharingServicePickerStyle;
#endif

@interface NSSharingServicePicker (Details)
@property NSSharingServicePickerStyle style;
- (NSMenu *)menu;
@end

#endif // ENABLE(SERVICE_CONTROLS)

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
    std::function<void()> _selectionHandler;
}
- (id)initWithSelectionHandler:(std::function<void()>)selectionHandler;
- (void)executeSelectionHandler;
@end

@implementation WKSelectionHandlerWrapper
- (id)initWithSelectionHandler:(std::function<void()>)selectionHandler
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

#if ENABLE(SERVICE_CONTROLS)
@interface WKSharingServicePickerDelegate : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate> {
    WebKit::WebContextMenuProxyMac* _menuProxy;
    RetainPtr<NSSharingServicePicker> _picker;
    BOOL _includeEditorServices;
}

+ (WKSharingServicePickerDelegate *)sharedSharingServicePickerDelegate;
- (WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setPicker:(NSSharingServicePicker *)picker;
- (void)setIncludeEditorServices:(BOOL)includeEditorServices;
@end

// FIXME: We probably need to hang on the picker itself until the context menu operation is done, and this object will probably do that.
@implementation WKSharingServicePickerDelegate
+ (WKSharingServicePickerDelegate*)sharedSharingServicePickerDelegate
{
    static WKSharingServicePickerDelegate* delegate = [[WKSharingServicePickerDelegate alloc] init];
    return delegate;
}

- (WebKit::WebContextMenuProxyMac*)menuProxy
{
    return _menuProxy;
}

- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy
{
    _menuProxy = menuProxy;
}

- (void)setPicker:(NSSharingServicePicker *)picker
{
    _picker = picker;
}

- (void)setIncludeEditorServices:(BOOL)includeEditorServices
{
    _includeEditorServices = includeEditorServices;
}

- (NSArray *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)mask proposedSharingServices:(NSArray *)proposedServices
{
    if (_includeEditorServices)
        return proposedServices;

    NSMutableArray *services = [[NSMutableArray alloc] initWithCapacity:[proposedServices count]];
    
    for (NSSharingService *service in proposedServices) {
        if (service.type != NSSharingServiceTypeEditor)
            [services addObject:service];
    }
    
    return services;
}

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items
{
    _menuProxy->clearServicesMenu();
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    // We only care about what item was shared if we were interested in editor services
    // (i.e., if we plan on replacing the selection with the returned item)
    if (!_includeEditorServices)
        return;

    Vector<String> types;
    IPC::DataReference dataReference;

    id item = [items objectAtIndex:0];

    if ([item isKindOfClass:[NSAttributedString class]]) {
        NSData *data = [item RTFDFromRange:NSMakeRange(0, [item length]) documentAttributes:nil];
        dataReference = IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);

        types.append(NSPasteboardTypeRTFD);
        types.append(NSRTFDPboardType);
    } else if ([item isKindOfClass:[NSData class]]) {
        NSData *data = (NSData *)item;
        RetainPtr<CGImageSourceRef> source = adoptCF(CGImageSourceCreateWithData((CFDataRef)data, NULL));
        RetainPtr<CGImageRef> image = adoptCF(CGImageSourceCreateImageAtIndex(source.get(), 0, NULL));

        if (!image)
            return;

        dataReference = IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
        types.append(NSPasteboardTypeTIFF);
    } else {
        LOG_ERROR("sharingService:didShareItems: - Unknown item type returned\n");
        return;
    }

    // FIXME: We should adopt replaceSelectionWithAttributedString instead of bouncing through the (fake) pasteboard.
    _menuProxy->page().replaceSelectionWithPasteboardData(types, dataReference);
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return _menuProxy->window();
}

@end

#endif

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

static Vector<RetainPtr<NSMenuItem>> nsMenuItemVector(const Vector<WebContextMenuItemData>& items)
{
    Vector<RetainPtr<NSMenuItem>> result;

    unsigned size = items.size();
    result.reserveCapacity(size);
    for (unsigned i = 0; i < size; i++) {
        switch (items[i].type()) {
        case ActionType:
        case CheckableActionType: {
            NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:nsStringFromWebCoreString(items[i].title()) action:@selector(forwardContextMenuAction:) keyEquivalent:@""];
            [menuItem setTag:items[i].action()];
            [menuItem setEnabled:items[i].enabled()];
            [menuItem setState:items[i].checked() ? NSOnState : NSOffState];

            if (std::function<void()> selectionHandler = items[i].selectionHandler()) {
                WKSelectionHandlerWrapper *wrapper = [[WKSelectionHandlerWrapper alloc] initWithSelectionHandler:selectionHandler];
                [menuItem setRepresentedObject:wrapper];
                [wrapper release];
            } else if (items[i].userData()) {
                WKUserDataWrapper *wrapper = [[WKUserDataWrapper alloc] initWithUserData:items[i].userData()];
                [menuItem setRepresentedObject:wrapper];
                [wrapper release];
            }

            result.append(adoptNS(menuItem));
            break;
        }
        case SeparatorType:
            result.append([NSMenuItem separatorItem]);
            break;
        case SubmenuType: {
            NSMenu* menu = [[NSMenu alloc] initWithTitle:nsStringFromWebCoreString(items[i].title())];
            [menu setAutoenablesItems:NO];
            populateNSMenu(menu, nsMenuItemVector(items[i].submenu()));
                
            NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:nsStringFromWebCoreString(items[i].title()) action:@selector(forwardContextMenuAction:) keyEquivalent:@""];
            [menuItem setEnabled:items[i].enabled()];
            [menuItem setSubmenu:menu];
            [menu release];

            result.append(adoptNS(menuItem));
            
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }

    WKMenuTarget* target = [WKMenuTarget sharedMenuTarget];
    for (unsigned i = 0; i < size; ++i)
        [result[i].get() setTarget:target];
    
    return result;
}

#if ENABLE(SERVICE_CONTROLS)

void WebContextMenuProxyMac::setupServicesMenu(const ContextMenuContextData& context)
{
    bool includeEditorServices = context.controlledDataIsEditable();
    bool hasControlledImage = !context.controlledImageHandle().isNull();
    NSArray *items = nil;
    if (hasControlledImage) {
        RefPtr<ShareableBitmap> image = ShareableBitmap::create(context.controlledImageHandle());
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
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setIncludeEditorServices:includeEditorServices];

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
        ServicesController::shared().refreshExistingServices();
}

void WebContextMenuProxyMac::clearServicesMenu()
{
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nullptr];
    m_servicesMenu = nullptr;
}
#endif

void WebContextMenuProxyMac::populate(const Vector<WebContextMenuItemData>& items, const ContextMenuContextData& context)
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
    }

    NSMenu* menu = [m_popup menu];
    populateNSMenu(menu, nsMenuItemVector(items));
}

void WebContextMenuProxyMac::showContextMenu(const IntPoint& menuLocation, const Vector<WebContextMenuItemData>& items, const ContextMenuContextData& context)
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
    if (context.isTelephoneNumberContext() || context.needsServicesMenu()) {
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

    WKPopupContextMenu(menu, location);

    hideContextMenu();
}

void WebContextMenuProxyMac::hideContextMenu()
{
    [m_popup dismissPopUp];
}

NSWindow *WebContextMenuProxyMac::window() const
{
    return [m_webView window];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

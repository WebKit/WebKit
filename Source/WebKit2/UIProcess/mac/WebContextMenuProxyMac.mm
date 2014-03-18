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

#import "PageClientImpl.h"
#import "ShareableBitmap.h"
#import "StringUtilities.h"
#import "WebContextMenuItemData.h"
#import "WKView.h"
#import <WebCore/GraphicsContext.h>
#import <WebCore/IntRect.h>
#import <WebCore/NotImplemented.h>
#import <WebKitSystemInterface.h>
#import <wtf/RetainPtr.h>

#if ENABLE(IMAGE_CONTROLS)
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
#endif

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
    WebKit::WebContextMenuItemData item(ActionType, static_cast<ContextMenuAction>([sender tag]), [sender title], [sender isEnabled], [sender state] == NSOnState);
    
    if (id representedObject = [sender representedObject]) {
        ASSERT([representedObject isKindOfClass:[WKUserDataWrapper class]]);
        item.setUserData([static_cast<WKUserDataWrapper *>(representedObject) userData]);
    }
            
    _menuProxy->contextMenuItemSelected(item);
}

@end

#if ENABLE(IMAGE_CONTROLS)
@interface WKSharingServicePickerDelegate : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate> {
    WebKit::WebContextMenuProxyMac* _menuProxy;
    RetainPtr<NSSharingServicePicker> _picker;
}

+ (WKSharingServicePickerDelegate *)sharedSharingServicePickerDelegate;
- (WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setPicker:(NSSharingServicePicker *)picker;
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

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items
{
    _menuProxy->clearImageServicesMenu();
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    RetainPtr<CGImageSourceRef> source = adoptCF(CGImageSourceCreateWithData((CFDataRef)[items objectAtIndex:0], NULL));
    RetainPtr<CGImageRef> image = adoptCF(CGImageSourceCreateImageAtIndex(source.get(), 0, NULL));
    _menuProxy->replaceControlledImage(image.get());
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
}

WebContextMenuProxyMac::~WebContextMenuProxyMac()
{
    if (m_popup)
        [m_popup setControlView:nil];
}

void WebContextMenuProxyMac::contextMenuItemSelected(const WebContextMenuItemData& item)
{
#if ENABLE(IMAGE_CONTROLS)
    clearImageServicesMenu();
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
                        
            if (items[i].userData()) {
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

#if ENABLE(IMAGE_CONTROLS)
void WebContextMenuProxyMac::setupImageServicesMenu(ShareableBitmap& image)
{
    RetainPtr<CGImageRef> cgImage = image.makeCGImage();
    RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithCGImage:cgImage.get() size:image.size()]);

    RetainPtr<NSSharingServicePicker> picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ nsImage.get() ]]);
    [picker setStyle:NSSharingServicePickerStyleRollover];
    [picker setDelegate:[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate]];
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:picker.get()];

    m_servicesMenu = [picker menu];
}

void WebContextMenuProxyMac::clearImageServicesMenu()
{
    [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setPicker:nullptr];
    m_servicesMenu = nullptr;
}
#endif

void WebContextMenuProxyMac::populate(const Vector<WebContextMenuItemData>& items, const ContextMenuContextData& context)
{
#if ENABLE(IMAGE_CONTROLS)
    if (RefPtr<ShareableBitmap> image = ShareableBitmap::create(context.controlledImageHandle())) {
        setupImageServicesMenu(*image);
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
#if ENABLE(IMAGE_CONTROLS)
    if (items.isEmpty() && context.controlledImageHandle().isNull())
        return;
#else
    if (items.isEmpty())
        return;
#endif

    populate(items, context);

    [[WKMenuTarget sharedMenuTarget] setMenuProxy:this];

    NSRect menuRect = NSMakeRect(menuLocation.x(), menuLocation.y(), 0, 0);

#if ENABLE(IMAGE_CONTROLS)
    if (!context.controlledImageHandle().isNull())
        [[WKSharingServicePickerDelegate sharedSharingServicePickerDelegate] setMenuProxy:this];

    if (!m_servicesMenu)
        [m_popup attachPopUpWithFrame:menuRect inView:m_webView];

    NSMenu *menu = m_servicesMenu ? m_servicesMenu.get() : [m_popup menu];
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

#if ENABLE(IMAGE_CONTROLS)
void WebContextMenuProxyMac::replaceControlledImage(CGImageRef newImage)
{
    FloatSize newImageSize(CGImageGetWidth(newImage), CGImageGetHeight(newImage));
    RefPtr<ShareableBitmap> newBitmap = ShareableBitmap::createShareable(expandedIntSize(newImageSize), ShareableBitmap::SupportsAlpha);
    newBitmap->createGraphicsContext()->drawNativeImage(newImage, newImageSize, ColorSpaceDeviceRGB, FloatRect(FloatPoint(), newImageSize), FloatRect(FloatPoint(), newImageSize));

    m_page->replaceControlledImage(newBitmap.release());
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC)

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
#import "WebPopupMenuProxyMac.h"

#if USE(APPKIT)

#import "CoreTextHelpers.h"
#import "NativeWebMouseEvent.h"
#import "PageClientImplMac.h"
#import "PlatformPopupMenuData.h"
#import "WebPopupItem.h"
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/mac/NSCellSPI.h>
#import <pal/system/mac/PopupMenu.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SetForScope.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {
using namespace WebCore;

WebPopupMenuProxyMac::WebPopupMenuProxyMac(NSView *webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
{
}

WebPopupMenuProxyMac::~WebPopupMenuProxyMac()
{
    if (m_popup)
        [m_popup setControlView:nil];
}

void WebPopupMenuProxyMac::populate(const Vector<WebPopupItem>& items, NSFont *font, TextDirection menuTextDirection)
{
    if (m_popup)
        [m_popup removeAllItems];
    else {
        m_popup = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popup setUsesItemFromMenu:NO];
        [m_popup setAutoenablesItems:NO];
    }

    int size = items.size();

    for (int i = 0; i < size; i++) {
        if (items[i].m_type == WebPopupItem::Type::Separator)
            [protectedMenu() addItem:[NSMenuItem separatorItem]];
        else {
            [m_popup addItemWithTitle:@""];
            RetainPtr menuItem = [m_popup lastItem];

            RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
            NSWritingDirection writingDirection = items[i].m_textDirection == TextDirection::LTR ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft;
            [paragraphStyle setBaseWritingDirection:writingDirection];
            [paragraphStyle setAlignment:menuTextDirection == TextDirection::LTR ? NSTextAlignmentLeft : NSTextAlignmentRight];
            RetainPtr<NSMutableDictionary> attributes = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
                paragraphStyle.get(), NSParagraphStyleAttributeName,
                font, NSFontAttributeName,
            nil]);
            if (items[i].m_hasTextDirectionOverride) {
                auto writingDirectionValue = static_cast<NSInteger>(writingDirection) + static_cast<NSInteger>(NSWritingDirectionOverride);
                RetainPtr<NSNumber> writingDirectionNumber = adoptNS([[NSNumber alloc] initWithInteger:writingDirectionValue]);
                RetainPtr<NSArray> writingDirectionArray = adoptNS([[NSArray alloc] initWithObjects:writingDirectionNumber.get(), nil]);
                [attributes setObject:writingDirectionArray.get() forKey:NSWritingDirectionAttributeName];
            }
            if (!items[i].m_language.isEmpty())
                [attributes setObject:items[i].m_language.createNSString().get() forKey:NSLanguageIdentifierAttributeName];
            RetainPtr<NSAttributedString> string = adoptNS([[NSAttributedString alloc] initWithString:items[i].m_text.createNSString().get() attributes:attributes.get()]);

            [menuItem setAttributedTitle:string.get()];
            // We set the title as well as the attributed title here. The attributed title will be displayed in the menu,
            // but typeahead will use the non-attributed string that doesn't contain any leading or trailing whitespace.
            [menuItem setTitle:[retainPtr([string string]) stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
            [menuItem setEnabled:items[i].m_isEnabled];
            [menuItem setToolTip:items[i].m_toolTip.createNSString().get()];
        }
    }
}

void WebPopupMenuProxyMac::showPopupMenu(const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData& data, int32_t selectedIndex)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    RetainPtr<CTFontRef> font;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    PlatformPopupMenuData mutableData = data;
    auto scaledFontSize = mutableData.font.metadata.pointSize * pageScaleFactor;
    mutableData.font.metadata.pointSize = scaledFontSize;
    font = mutableData.font.toCTFont();

    // font will be nil when using a custom font. However, we should still
    // honor the font size, matching other browsers.
    if (!font)
        font = bridge_cast([NSFont menuFontOfSize:scaledFontSize]);

    END_BLOCK_OBJC_EXCEPTIONS

    populate(items, bridge_cast(font.get()), textDirection);

    [m_popup attachPopUpWithFrame:rect inView:m_webView.get().get()];
    [m_popup selectItemAtIndex:selectedIndex];
    [m_popup setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    RetainPtr menu = [m_popup menu];
    [menu setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    // These values were borrowed from AppKit to match their placement of the menu.
    const int popOverHorizontalAdjust = -13;
    const int popUnderHorizontalAdjust = 6;
    const int popUnderVerticalAdjust = 6;
    
    // Menus that pop-over directly obscure the node that generated the popup menu.
    // Menus that pop-under are offset underneath it.
    NSPoint location;
    if (data.shouldPopOver) {
        NSRect titleFrame = [m_popup  titleRectForBounds:rect];
        if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
            titleFrame = rect;
        float verticalOffset = roundf((NSMaxY(rect) - NSMaxY(titleFrame)) + NSHeight(titleFrame)) + 1;
        if (textDirection == TextDirection::LTR)
            location = NSMakePoint(NSMinX(rect) + popOverHorizontalAdjust, NSMaxY(rect) - verticalOffset);
        else
            location = NSMakePoint(NSMaxX(rect) - popOverHorizontalAdjust, NSMaxY(rect) - verticalOffset);
    } else {
        if (textDirection == TextDirection::LTR)
            location = NSMakePoint(NSMinX(rect) + popUnderHorizontalAdjust, NSMaxY(rect) + popUnderVerticalAdjust);
        else
            location = NSMakePoint(NSMaxX(rect) - popUnderHorizontalAdjust, NSMaxY(rect) + popUnderVerticalAdjust);
    }
    RetainPtr<NSView> dummyView = adoptNS([[NSView alloc] initWithFrame:rect]);
    [dummyView.get() setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];
    [m_webView.get() addSubview:dummyView.get()];
    location = [dummyView convertPoint:location fromView:m_webView.get().get()];

    NSControlSize controlSize;
    switch (data.menuSize) {
    case WebCore::PopupMenuStyle::Size::Normal:
        controlSize = NSControlSizeRegular;
        break;
    case WebCore::PopupMenuStyle::Size::Small:
        controlSize = NSControlSizeSmall;
        break;
    case WebCore::PopupMenuStyle::Size::Mini:
        controlSize = NSControlSizeMini;
        break;
    case WebCore::PopupMenuStyle::Size::Large:
        controlSize = NSControlSizeLarge;
        break;
    }

    SetForScope visibleScope { m_isVisible, true };

    Ref<WebPopupMenuProxyMac> protect(*this);
    PAL::popUpMenu(menu.get(), location, roundf(NSWidth(rect)), dummyView.get(), selectedIndex, bridge_cast(font.get()), controlSize, data.hideArrows);

    [m_popup dismissPopUp];
    [dummyView removeFromSuperview];
    
    CheckedPtr client = this->client();
    if (!client || m_wasCanceled)
        return;
    
    client->valueChangedForPopupMenu(this, [m_popup indexOfSelectedItem]);
    
    // <https://bugs.webkit.org/show_bug.cgi?id=57904> This code is adopted from EventHandler::sendFakeEventsAfterWidgetTracking().
    if (!client->currentlyProcessedMouseDownEvent())
        return;
    
    RetainPtr initiatingNSEvent = client->currentlyProcessedMouseDownEvent()->nativeEvent();
    if ([initiatingNSEvent type] != NSEventTypeLeftMouseDown)
        return;

    RetainPtr fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                            location:[initiatingNSEvent locationInWindow]
                                       modifierFlags:[initiatingNSEvent modifierFlags]
                                           timestamp:[initiatingNSEvent timestamp]
                                        windowNumber:[initiatingNSEvent windowNumber]
                                             context:nullptr
                                         eventNumber:[initiatingNSEvent eventNumber]
                                          clickCount:[initiatingNSEvent clickCount]
                                            pressure:[initiatingNSEvent pressure]];

    [NSApp postEvent:fakeEvent.get() atStart:YES];
    fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                   location:[retainPtr([m_webView.get() window]) convertPointFromScreen:[NSEvent mouseLocation]]
                              modifierFlags:[initiatingNSEvent modifierFlags]
                                  timestamp:[initiatingNSEvent timestamp]
                               windowNumber:[initiatingNSEvent windowNumber]
                                    context:nullptr
                                eventNumber:0
                                 clickCount:0
                                   pressure:0];
    [NSApp postEvent:fakeEvent.get() atStart:YES];
}

void WebPopupMenuProxyMac::hidePopupMenu()
{
    [protectedMenu() cancelTracking];
}

void WebPopupMenuProxyMac::cancelTracking()
{
    [protectedMenu() cancelTracking];
    m_wasCanceled = true;
}

RetainPtr<NSPopUpButtonCell> WebPopupMenuProxyMac::protectedPopup() const
{
    return m_popup;
}

RetainPtr<NSMenu> WebPopupMenuProxyMac::protectedMenu() const
{
    return [m_popup menu];
}

} // namespace WebKit

#endif // USE(APPKIT)

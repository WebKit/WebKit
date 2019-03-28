/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#import "NativeWebMouseEvent.h"
#import "PageClientImplMac.h"
#import "PlatformPopupMenuData.h"
#import "StringUtilities.h"
#import "WebPopupItem.h"
#import <pal/system/mac/PopupMenu.h>
#import <wtf/ProcessPrivilege.h>

namespace WebKit {
using namespace WebCore;

WebPopupMenuProxyMac::WebPopupMenuProxyMac(NSView *webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
    , m_wasCanceled(false)
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
        if (items[i].m_type == WebPopupItem::Separator)
            [[m_popup menu] addItem:[NSMenuItem separatorItem]];
        else {
            [m_popup addItemWithTitle:@""];
            NSMenuItem *menuItem = [m_popup lastItem];

            RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
            NSWritingDirection writingDirection = items[i].m_textDirection == TextDirection::LTR ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft;
            [paragraphStyle setBaseWritingDirection:writingDirection];
            [paragraphStyle setAlignment:menuTextDirection == TextDirection::LTR ? NSTextAlignmentLeft : NSTextAlignmentRight];
            RetainPtr<NSMutableDictionary> attributes = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
                paragraphStyle.get(), NSParagraphStyleAttributeName,
                font, NSFontAttributeName,
            nil]);
            if (items[i].m_hasTextDirectionOverride) {
                RetainPtr<NSNumber> writingDirectionValue = adoptNS([[NSNumber alloc] initWithInteger:writingDirection + NSWritingDirectionOverride]);
                RetainPtr<NSArray> writingDirectionArray = adoptNS([[NSArray alloc] initWithObjects:writingDirectionValue.get(), nil]);
                [attributes setObject:writingDirectionArray.get() forKey:NSWritingDirectionAttributeName];
            }
            RetainPtr<NSAttributedString> string = adoptNS([[NSAttributedString alloc] initWithString:nsStringFromWebCoreString(items[i].m_text) attributes:attributes.get()]);

            [menuItem setAttributedTitle:string.get()];
            // We set the title as well as the attributed title here. The attributed title will be displayed in the menu,
            // but typeahead will use the non-attributed string that doesn't contain any leading or trailing whitespace.
            [menuItem setTitle:[[string string] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
            [menuItem setEnabled:items[i].m_isEnabled];
            [menuItem setToolTip:nsStringFromWebCoreString(items[i].m_toolTip)];
        }
    }
}

void WebPopupMenuProxyMac::showPopupMenu(const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData& data, int32_t selectedIndex)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    NSFont *font;
    if (data.fontInfo.fontAttributeDictionary) {
        NSFontDescriptor *fontDescriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:(__bridge NSDictionary *)data.fontInfo.fontAttributeDictionary.get()];
        font = [NSFont fontWithDescriptor:fontDescriptor size:((pageScaleFactor != 1) ? [fontDescriptor pointSize] * pageScaleFactor : 0)];
    } else
        font = [NSFont menuFontOfSize:0];

    populate(items, font, textDirection);

    [m_popup attachPopUpWithFrame:rect inView:m_webView];
    [m_popup selectItemAtIndex:selectedIndex];
    [m_popup setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    NSMenu *menu = [m_popup menu];
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
    [m_webView addSubview:dummyView.get()];
    location = [dummyView convertPoint:location fromView:m_webView];

    NSControlSize controlSize;
    switch (data.menuSize) {
    case WebCore::PopupMenuStyle::PopupMenuSizeNormal:
        controlSize = NSControlSizeRegular;
        break;
    case WebCore::PopupMenuStyle::PopupMenuSizeSmall:
        controlSize = NSControlSizeSmall;
        break;
    case WebCore::PopupMenuStyle::PopupMenuSizeMini:
        controlSize = NSControlSizeMini;
        break;
    }

    Ref<WebPopupMenuProxyMac> protect(*this);
    PAL::popUpMenu(menu, location, roundf(NSWidth(rect)), dummyView.get(), selectedIndex, font, controlSize, data.hideArrows);

    [m_popup dismissPopUp];
    [dummyView removeFromSuperview];
    
    if (!m_client || m_wasCanceled)
        return;
    
    m_client->valueChangedForPopupMenu(this, [m_popup indexOfSelectedItem]);
    
    // <https://bugs.webkit.org/show_bug.cgi?id=57904> This code is adopted from EventHandler::sendFakeEventsAfterWidgetTracking().
    if (!m_client->currentlyProcessedMouseDownEvent())
        return;
    
    NSEvent* initiatingNSEvent = m_client->currentlyProcessedMouseDownEvent()->nativeEvent();
    if ([initiatingNSEvent type] != NSEventTypeLeftMouseDown)
        return;

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                            location:[initiatingNSEvent locationInWindow]
                                       modifierFlags:[initiatingNSEvent modifierFlags]
                                           timestamp:[initiatingNSEvent timestamp]
                                        windowNumber:[initiatingNSEvent windowNumber]
                                             context:nullptr
                                         eventNumber:[initiatingNSEvent eventNumber]
                                          clickCount:[initiatingNSEvent clickCount]
                                            pressure:[initiatingNSEvent pressure]];

    [NSApp postEvent:fakeEvent atStart:YES];
    fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                   location:[[m_webView window]
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                        convertScreenToBase:[NSEvent mouseLocation]]
ALLOW_DEPRECATED_DECLARATIONS_END
                              modifierFlags:[initiatingNSEvent modifierFlags]
                                  timestamp:[initiatingNSEvent timestamp]
                               windowNumber:[initiatingNSEvent windowNumber]
                                    context:nullptr
                                eventNumber:0
                                 clickCount:0
                                   pressure:0];
    [NSApp postEvent:fakeEvent atStart:YES];
}

void WebPopupMenuProxyMac::hidePopupMenu()
{
    [[m_popup menu] cancelTracking];
}

void WebPopupMenuProxyMac::cancelTracking()
{
    [[m_popup menu] cancelTracking];
    m_wasCanceled = true;
}

} // namespace WebKit

#endif // USE(APPKIT)

/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "config.h"
#import "PopupMenu.h"

#import "AXObjectCache.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "EventHandler.h"
#import "Frame.h"
#import "FrameView.h"
#import "HTMLNames.h"
#import "HTMLOptGroupElement.h"
#import "HTMLOptionElement.h"
#import "HTMLSelectElement.h"
#import "Page.h"
#import "SimpleFontData.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

using namespace HTMLNames;

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
{
}

PopupMenu::~PopupMenu()
{
    if (m_popup)
        [m_popup.get() setControlView:nil];
}

void PopupMenu::clear()
{
    if (m_popup)
        [m_popup.get() removeAllItems];
}

void PopupMenu::populate()
{
    if (m_popup)
        clear();
    else {
        m_popup = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:!client()->shouldPopOver()];
        [m_popup.get() release]; // release here since the RetainPtr has retained the object already
        [m_popup.get() setUsesItemFromMenu:NO];
        [m_popup.get() setAutoenablesItems:NO];
    }

    BOOL messagesEnabled = [[m_popup.get() menu] menuChangedMessagesEnabled];
    [[m_popup.get() menu] setMenuChangedMessagesEnabled:NO];
    
    // For pullDown menus the first item is hidden.
    if (!client()->shouldPopOver())
        [m_popup.get() addItemWithTitle:@""];

    ASSERT(client());
    int size = client()->listSize();

    for (int i = 0; i < size; i++) {
        if (client()->itemIsSeparator(i))
            [[m_popup.get() menu] addItem:[NSMenuItem separatorItem]];
        else {
            PopupMenuStyle style = client()->itemStyle(i);
            NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
            if (style.font() != Font()) {
                NSFont *font = style.font().primaryFont()->getNSFont();
                if (!font) {
                    CGFloat size = style.font().primaryFont()->platformData().size();
                    font = style.font().weight() < FontWeightBold ? [NSFont systemFontOfSize:size] : [NSFont boldSystemFontOfSize:size];
                }
                [attributes setObject:font forKey:NSFontAttributeName];
            }
            // FIXME: Add support for styling the foreground and background colors.
            // FIXME: Find a way to customize text color when an item is highlighted.
            NSAttributedString* string = [[NSAttributedString alloc] initWithString:client()->itemText(i) attributes:attributes];
            [attributes release];

            [m_popup.get() addItemWithTitle:@""];
            NSMenuItem* menuItem = [m_popup.get() lastItem];
            [menuItem setAttributedTitle:string];
            [menuItem setEnabled:client()->itemIsEnabled(i)];
            [menuItem setToolTip:client()->itemToolTip(i)];
            [string release];
            
            // Allow the accessible text of the item to be overriden if necessary.
            if (AXObjectCache::accessibilityEnabled()) {
                NSString *accessibilityOverride = client()->itemAccessibilityText(i);
                if ([accessibilityOverride length])
                    [menuItem accessibilitySetOverrideValue:accessibilityOverride forAttribute:NSAccessibilityDescriptionAttribute];
            }
        }
    }

    [[m_popup.get() menu] setMenuChangedMessagesEnabled:messagesEnabled];
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    populate();
    int numItems = [m_popup.get() numberOfItems];
    if (numItems <= 0) {
        if (client())
            client()->popupDidHide();
        return;
    }
    ASSERT(numItems > index);

    // Workaround for crazy bug where a selected index of -1 for a menu with only 1 item will cause a blank menu.
    if (index == -1 && numItems == 2 && !client()->shouldPopOver() && ![[m_popup.get() itemAtIndex:1] isEnabled])
        index = 0;

    NSView* view = v->documentView();

    [m_popup.get() attachPopUpWithFrame:r inView:view];
    [m_popup.get() selectItemAtIndex:index];

    NSMenu* menu = [m_popup.get() menu];
    
    NSPoint location;
    NSFont* font = client()->menuStyle().font().primaryFont()->getNSFont();

    // These values were borrowed from AppKit to match their placement of the menu.
    const int popOverHorizontalAdjust = -10;
    const int popUnderHorizontalAdjust = 6;
    const int popUnderVerticalAdjust = 6;
    if (client()->shouldPopOver()) {
        NSRect titleFrame = [m_popup.get() titleRectForBounds:r];
        if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
            titleFrame = r;
        float vertOffset = roundf((NSMaxY(r) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
        // Adjust for fonts other than the system font.
        NSFont* defaultFont = [NSFont systemFontOfSize:[font pointSize]];
        vertOffset += [font descender] - [defaultFont descender];
        vertOffset = fminf(NSHeight(r), vertOffset);
    
        location = NSMakePoint(NSMinX(r) + popOverHorizontalAdjust, NSMaxY(r) - vertOffset);
    } else
        location = NSMakePoint(NSMinX(r) + popUnderHorizontalAdjust, NSMaxY(r) + popUnderVerticalAdjust);    

    // Save the current event that triggered the popup, so we can clean up our event
    // state after the NSMenu goes away.
    RefPtr<Frame> frame = v->frame();
    NSEvent* event = [frame->eventHandler()->currentNSEvent() retain];
    
    RefPtr<PopupMenu> protector(this);

    RetainPtr<NSView> dummyView(AdoptNS, [[NSView alloc] initWithFrame:r]);
    [view addSubview:dummyView.get()];
    location = [dummyView.get() convertPoint:location fromView:view];
    
    if (Page* page = frame->page())
        page->chrome()->client()->willPopUpMenu(menu);
    wkPopupMenu(menu, location, roundf(NSWidth(r)), dummyView.get(), index, font);

    [m_popup.get() dismissPopUp];
    [dummyView.get() removeFromSuperview];

    if (client()) {
        int newIndex = [m_popup.get() indexOfSelectedItem];
        client()->popupDidHide();

        // Adjust newIndex for hidden first item.
        if (!client()->shouldPopOver())
            newIndex--;

        if (index != newIndex && newIndex >= 0)
            client()->valueChanged(newIndex);

        // Give the frame a chance to fix up its event state, since the popup eats all the
        // events during tracking.
        frame->eventHandler()->sendFakeEventsAfterWidgetTracking(event);
    }

    [event release];
}

void PopupMenu::hide()
{
    [m_popup.get() dismissPopUp];
}
    
void PopupMenu::updateFromElement()
{
}

bool PopupMenu::itemWritingDirectionIsNatural()
{
    return true;
}

}

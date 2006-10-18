/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#import "config.h"
#import "PopupMenu.h"

#import "FontData.h"
#import "FrameMac.h"
#import "FrameView.h"
#import "HTMLNames.h"
#import "HTMLOptGroupElement.h"
#import "HTMLOptionElement.h"
#import "RenderMenuList.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

using namespace HTMLNames;

PopupMenu::PopupMenu(RenderMenuList* menuList)
    : m_menuList(menuList)
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
        [m_popup.get() removeAllItems];
    else {
        m_popup = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO];
        [m_popup.get() release]; // release here since the RetainPtr has retained the object already
        [m_popup.get() setUsesItemFromMenu:NO];
        [m_popup.get() setAutoenablesItems:NO];
    }
    BOOL messagesEnabled = [[m_popup.get() menu] menuChangedMessagesEnabled];
    [[m_popup.get() menu] setMenuChangedMessagesEnabled:NO];
    PopupMenu::addItems();
    [[m_popup.get() menu] setMenuChangedMessagesEnabled:messagesEnabled];
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    populate();
    if ([m_popup.get() numberOfItems] <= 0)
        return;
    ASSERT([m_popup.get() numberOfItems] > index);

    NSView* view = v->getDocumentView();

    [m_popup.get() attachPopUpWithFrame:r inView:view];
    [m_popup.get() selectItemAtIndex:index];
    
    NSFont* font = menuList()->style()->font().primaryFont()->getNSFont();

    NSRect titleFrame = [m_popup.get() titleRectForBounds:r];
    if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
        titleFrame = r;
    float vertOffset = roundf((NSMaxY(r) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
    // Adjust for fonts other than the system font.
    NSFont* defaultFont = [NSFont systemFontOfSize:[font pointSize]];
    vertOffset += [font descender] - [defaultFont descender];
    vertOffset = fmin(NSHeight(r), vertOffset);

    NSMenu* menu = [m_popup.get() menu];
    // FIXME: Need to document where this magic number 10 comes from.
    NSPoint location = NSMakePoint(NSMinX(r) - 10, NSMaxY(r) - vertOffset);

    // Save the current event that triggered the popup, so we can clean up our event
    // state after the NSMenu goes away.
    RefPtr<FrameMac> frame = Mac(v->frame());
    NSEvent* event = [frame->currentEvent() retain];
    
    RefPtr<PopupMenu> protector(this);
    
    frame->willPopupMenu(menu);
    wkPopupMenu(menu, location, roundf(NSWidth(r)), view, index, font);

    if (menuList()) {
        int newIndex = [m_popup.get() indexOfSelectedItem];
        menuList()->hidePopup();

        if (index != newIndex && newIndex >= 0)
            menuList()->valueChanged(newIndex);

        // Give the frame a chance to fix up its event state, since the popup eats all the
        // events during tracking.
        frame->sendFakeEventsAfterWidgetTracking(event);
    }

    [event release];
}

void PopupMenu::hide()
{
    [m_popup.get() dismissPopUp];
}

void PopupMenu::addSeparator()
{
    [[m_popup.get() menu] addItem:[NSMenuItem separatorItem]];
}

void PopupMenu::addGroupLabel(HTMLOptGroupElement* element)
{
    String text = element->groupLabelText();

    RenderStyle* s = element->renderStyle();
    if (!s)
        s = menuList()->style();

    NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
    if (s->font() != Font())
        [attributes setObject:s->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];
    // FIXME: Add support for styling the foreground and background colors.
    NSAttributedString* string = [[NSAttributedString alloc] initWithString:text attributes:attributes];
    [attributes release];

    [m_popup.get() addItemWithTitle:@""];
    NSMenuItem* menuItem = [m_popup.get() lastItem];
    [menuItem setAttributedTitle:string];
    [menuItem setEnabled:NO];

    [string release];
}

void PopupMenu::addOption(HTMLOptionElement* element)
{
    String text = element->optionText();
    
    bool groupEnabled = true;
    if (element->parentNode() && element->parentNode()->hasTagName(optgroupTag))
        groupEnabled = element->parentNode()->isEnabled();

    RenderStyle* s = element->renderStyle();
    if (!s)
        s = menuList()->style();
        
    NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
    if (s->font() != Font())
        [attributes setObject:s->font().primaryFont()->getNSFont() forKey:NSFontAttributeName];
    // FIXME: Add support for styling the foreground and background colors.
    // FIXME: Find a way to customize text color when an item is highlighted.
    NSAttributedString* string = [[NSAttributedString alloc] initWithString:text attributes:attributes];
    [attributes release];

    [m_popup.get() addItemWithTitle:@""];
    NSMenuItem* menuItem = [m_popup.get() lastItem];
    [menuItem setAttributedTitle:string];
    [menuItem setEnabled:groupEnabled && element->isEnabled()];

    [string release];
}

}

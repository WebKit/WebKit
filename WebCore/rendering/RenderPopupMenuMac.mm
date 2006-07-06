/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
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
#import "RenderPopupMenuMac.h"

#import "FrameView.h"
#import "HTMLNames.h"
#import "HTMLOptionElement.h"
#import "HTMLOptGroupElement.h"
#import "RenderMenuList.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

using namespace HTMLNames;

RenderPopupMenuMac::RenderPopupMenuMac(Node* element)
    : RenderPopupMenu(element)
    , popup(nil)
{
}

RenderPopupMenuMac::~RenderPopupMenuMac()
{
    if (popup) {
        [popup setControlView: nil];
        [popup release];
    }
}

void RenderPopupMenuMac::clear()
{
    if (popup)
        [popup removeAllItems];
}

void RenderPopupMenuMac::populate()
{
    BOOL messagesEnabled = [[popup menu] menuChangedMessagesEnabled];
    [[popup menu] setMenuChangedMessagesEnabled:NO];
    RenderPopupMenu::populate();
    [[popup menu] setMenuChangedMessagesEnabled:messagesEnabled];

}

void RenderPopupMenuMac::showPopup(const IntRect& r, FrameView* v, int index)
{
    if (!popup) {
        popup = [[[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO] retain];
        [popup setUsesItemFromMenu:NO];
        [popup setAutoenablesItems:NO];
    }
    // If we decide to cache the NSMenuItems, we shouldn't clear and populate every time we show the popup menu
    clear();
    populate();
    
    if ([popup numberOfItems] <= 0)
        return;
    
    ASSERT([popup numberOfItems] > index);
    
    NSView* view = v->getDocumentView();
    [popup attachPopUpWithFrame:r inView:view];
    
    [popup selectItemAtIndex:index];
    
    NSPoint location = NSMakePoint(NSMinX(r), NSMaxY(r));
    float vertOffset;
    
    //center it in our title frame
    NSRect titleFrame = [popup titleRectForBounds:r];
    if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
        titleFrame = r;
    NSFont* font = style()->font().getNSFont();
    NSSize sizeOfSelectedItem = titleFrame.size;

    vertOffset = round((NSMaxY(r) - NSMaxY(titleFrame)) + (NSHeight(titleFrame) + sizeOfSelectedItem.height)/2.f);

    //Align us for different fonts
    NSFont* defaultFont = [NSFont systemFontOfSize:[font pointSize]];
    vertOffset += [font descender] - [defaultFont descender];

    vertOffset = fmin(NSHeight(r), vertOffset);

    location.x -= 10;
    location.y -= vertOffset;

//  NSColor* backgroundColor = nsColor(style()->backgroundColor());
    NSMenu* menu = [popup menu];
    
    wkPopupMenu(menu, location, floor(NSWidth(r) + 0.5), view, index, font);
    
    // update text on button
    int newIndex = [popup indexOfSelectedItem];
    if (index != newIndex && newIndex >= 0)
        getRenderMenuList()->valueChanged(newIndex);

    [popup dismissPopUp];
}

void RenderPopupMenuMac::addSeparator()
{
    NSMenuItem *separator = [NSMenuItem separatorItem];
    [[popup menu] addItem:separator];
}

void RenderPopupMenuMac::addGroupLabel(HTMLOptGroupElement* element)
{
    if (!element)
        return;
    String text = element->groupLabelText();
    
    [popup addItemWithTitle:@""];
    NSMenuItem *menuItem = [popup lastItem];

    RenderStyle* s = element->renderStyle();
    if (!s)
        s = style();

    NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
    if (s->font() != Font())
        [attributes setObject:s->font().getNSFont() forKey:NSFontAttributeName];
    [attributes setObject:nsColor(s->color()) forKey:NSForegroundColorAttributeName];
    NSAttributedString *string = [[NSAttributedString alloc] initWithString:text attributes:attributes];

    [menuItem setAttributedTitle:string];
    [string release];
    [attributes release];
    [menuItem setEnabled:NO];
}

void RenderPopupMenuMac::addOption(HTMLOptionElement* element)
{
    if (!element)
        return;
    String text = element->optionText();
    
    bool groupEnabled = true;
    if (element->parentNode() && element->parentNode()->hasTagName(optgroupTag))
        groupEnabled = element->parentNode()->isEnabled();

    [popup addItemWithTitle:@""];
    NSMenuItem *menuItem = [popup lastItem];
    
    RenderStyle* s = element->renderStyle();
    if (!s)
        s = style();
        
    NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
    if (s->font() != Font())
        [attributes setObject:s->font().getNSFont() forKey:NSFontAttributeName];
    [attributes setObject:nsColor(s->color()) forKey:NSForegroundColorAttributeName];
    NSAttributedString *string = [[NSAttributedString alloc] initWithString:text attributes:attributes];

    [menuItem setAttributedTitle:string];
    [string release];
    [attributes release];
    [menuItem setEnabled:groupEnabled && element->isEnabled()];

}

}

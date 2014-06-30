/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther zecke@selfish.org
 *            (C) 2009 Kenneth Rohde Christiansen
 *            (C) 2009 INdT, Instituto Nokia de Technologia
 *            (C) 2009-2010 ProFUSION embedded systems
 *            (C) 2009-2010 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ScrollbarEfl.h"

#include "IntRect.h"
#include "Settings.h"

namespace WebCore {

PassRefPtr<Scrollbar> Scrollbar::createNativeScrollbar(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    if (Settings::mockScrollbarsEnabled())
        return adoptRef(new Scrollbar(scrollableArea, orientation, size));

    return adoptRef(new ScrollbarEfl(scrollableArea, orientation, size));
}

ScrollbarEfl::ScrollbarEfl(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
    : Scrollbar(scrollableArea, orientation, controlSize)
{
}

ScrollbarEfl::~ScrollbarEfl()
{
}

void ScrollbarEfl::setParent(ScrollView* view)
{
    Widget::setParent(view);
}

void ScrollbarEfl::setFrameRect(const IntRect& rect)
{
    Widget::setFrameRect(rect);
    frameRectsChanged();
}

void ScrollbarEfl::frameRectsChanged()
{
}

void ScrollbarEfl::invalidate()
{
    Widget::invalidate();
}

}

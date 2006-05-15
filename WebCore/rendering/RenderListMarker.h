/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 *
 */

#ifndef RenderListMarker_H
#define RenderListMarker_H

#include "DeprecatedString.h"
#include "RenderBox.h"

namespace WebCore {

class RenderListItem;

/* used to render the lists marker.
     This class always has to be a direct child of a RenderListItem!
*/
class RenderListMarker : public RenderBox
{
public:
    RenderListMarker(Document*);
    ~RenderListMarker();

    virtual void setStyle(RenderStyle*);

    virtual const char* renderName() const { return "RenderListMarker"; }
    // so the marker gets to layout itself. Only needed for
    // list-style-position: inside

    virtual void paint(PaintInfo&, int xoff, int yoff);
    virtual void layout();
    virtual void calcMinMaxWidth();

    virtual void imageChanged(CachedImage*);

    virtual void calcWidth();

    virtual InlineBox* createInlineBox(bool, bool, bool);

    virtual short lineHeight(bool b, bool isRootLineBox=false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox=false) const;

    virtual bool isListMarker() const { return true; }
    
    CachedImage* listImage() const { return m_listImage; }
    
    RenderListItem* listItem() { return m_listItem; }
    void setListItem(RenderListItem* listItem) { m_listItem = listItem; }
    
    const DeprecatedString& text() const { return m_item; }

    bool isInside() const;
    
    IntRect getRelativeMarkerRect();
    
    virtual SelectionState selectionState() const { return m_selectionState; }
    virtual void setSelectionState(SelectionState);
    virtual IntRect selectionRect();
    virtual Color selectionColor(GraphicsContext*) const;
    virtual bool canBeSelectionLeaf() const { return true; }

private:
    DeprecatedString m_item;
    CachedImage* m_listImage;
    RenderListItem* m_listItem;
    SelectionState m_selectionState;
};

} //namespace

#endif

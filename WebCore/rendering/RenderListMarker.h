/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef RenderListMarker_h
#define RenderListMarker_h

#include "RenderBox.h"

namespace WebCore {

class RenderListItem;

String listMarkerText(EListStyleType, int value);

// Used to render the list item's marker.
// The RenderListMarker always has to be a child of a RenderListItem.
class RenderListMarker : public RenderBox {
public:
    RenderListMarker(RenderListItem*);
    ~RenderListMarker();

    virtual const char* renderName() const { return "RenderListMarker"; }

    virtual bool isListMarker() const { return true; }

    virtual void paint(PaintInfo&, int tx, int ty);

    virtual void layout();
    virtual void calcPrefWidths();

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);

    virtual InlineBox* createInlineBox();

    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;
    virtual int baselinePosition(bool firstLine, bool isRootLineBox = false) const;

    bool isImage() const;
    bool isText() const { return !isImage(); }
    const String& text() const { return m_text; }

    bool isInside() const;

    virtual void setSelectionState(SelectionState);
    virtual IntRect selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool clipToVisibleContent = true);
    virtual bool canBeSelectionLeaf() const { return true; }

    void updateMargins();

protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    IntRect getRelativeMarkerRect();

    String m_text;
    RefPtr<StyleImage> m_image;
    RenderListItem* m_listItem;
};

} // namespace WebCore

#endif // RenderListMarker_h

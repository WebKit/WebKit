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

#ifndef RenderListItem_H
#define RenderListItem_H

#include "RenderBlock.h"

namespace WebCore {

class RenderListMarker;

class RenderListItem : public RenderBlock
{
public:
    RenderListItem(Node*);
    
    virtual void destroy();

    virtual const char* renderName() const { return "RenderListItem"; }

    virtual void setStyle(RenderStyle*);

    virtual bool isListItem() const { return true; }
    
    int value() const { return m_value; }
    void setValue(int v) { m_predefVal = v; }
    void calcValue();
    void resetValue();

    virtual bool isEmpty() const;
    virtual void paint(PaintInfo&, int xoff, int yoff);

    virtual void layout();
    virtual void calcMinMaxWidth();

    virtual void positionListMarker();
    void updateMarkerLocation();
    
    void setNotInList(bool notInList) { m_notInList = notInList; }
    bool notInList() const { return m_notInList; }

    DeprecatedString markerStringValue();

private:
    int m_predefVal;
    RenderListMarker* m_marker;
    bool m_notInList;
    int m_value;
};


} //namespace

#endif

/**
* This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "InlineBox.h"

namespace WebCore {

class EllipsisBox : public InlineBox
{
public:
    EllipsisBox(RenderObject* obj, const AtomicString& ellipsisStr, InlineFlowBox* p,
                int w, int y, int h, int b, bool firstLine, InlineBox* markupBox)
        : InlineBox(obj)
        , m_str(ellipsisStr)
    {
        m_parent = p;
        m_width = w;
        m_y = y;
        m_height = h;
        m_baseline = b;
        m_firstLine = firstLine;
        m_constructed = true;
        m_markupBox = markupBox;
    }
    
    virtual void paint(RenderObject::PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& info, int _x, int _y, int _tx, int _ty);

private:
    AtomicString m_str;
    InlineBox* m_markupBox;
};

}
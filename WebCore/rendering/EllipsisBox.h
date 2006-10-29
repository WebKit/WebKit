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

#ifndef EllipsisBox_H
#define EllipsisBox_H

#include "InlineBox.h"

namespace WebCore {

class HitTestResult;

class EllipsisBox : public InlineBox {
public:
    EllipsisBox(RenderObject* obj, const AtomicString& ellipsisStr, InlineFlowBox* parent,
                int width, int y, int height, int baseline, bool firstLine, InlineBox* markupBox)
        : InlineBox(obj, 0, y, width, height, baseline, firstLine, true, false, false, 0, 0, parent)
        , m_str(ellipsisStr)
        , m_markupBox(markupBox)
    {
    }
    
    virtual void paint(RenderObject::PaintInfo&, int _tx, int _ty);
    virtual bool nodeAtPoint(HitTestResult&, int _x, int _y, int _tx, int _ty);

private:
    AtomicString m_str;
    InlineBox* m_markupBox;
};

} // namespace WebCore

#endif // EllipsisBox_H

/*
 * This file is part of the line box implementation for KDE.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef InlineRunBox_h
#define InlineRunBox_h

#include "InlineBox.h"

namespace WebCore {

class InlineRunBox : public InlineBox {
public:
    InlineRunBox(RenderObject* obj)
        : InlineBox(obj)
        , m_prevLine(0)
        , m_nextLine(0)
    {
    }

    InlineRunBox* prevLineBox() const { return m_prevLine; }
    InlineRunBox* nextLineBox() const { return m_nextLine; }
    void setNextLineBox(InlineRunBox* n) { m_nextLine = n; }
    void setPreviousLineBox(InlineRunBox* p) { m_prevLine = p; }

    virtual void paintBoxDecorations(RenderObject::PaintInfo&, int /*tx*/, int /*ty*/) { }
    virtual void paintTextDecorations(RenderObject::PaintInfo&, int /*tx*/, int /*ty*/, bool /*paintedChildren*/ = false) { }
    
protected:
    InlineRunBox* m_prevLine;  // The previous box that also uses our RenderObject
    InlineRunBox* m_nextLine;  // The next box that also uses our RenderObject
};

} // namespace WebCore

#endif // InlineRunBox_h

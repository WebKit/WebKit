/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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

#ifndef RenderBR_h
#define RenderBR_h

#include "RenderText.h"

/*
 * The whole class here is a hack to get <br> working, as long as we don't have support for
 * CSS2 :before and :after pseudo elements
 */
namespace WebCore {

class Position;

class RenderBR : public RenderText {
public:
    RenderBR(Node*);
    virtual ~RenderBR();

    virtual const char* renderName() const { return "RenderBR"; }
 
    virtual IntRect selectionRectForRepaint(RenderBoxModelObject* /*repaintContainer*/, bool /*clipToVisibleContent*/) { return IntRect(); }

    virtual unsigned width(unsigned /*from*/, unsigned /*len*/, const Font&, int /*xpos*/) const { return 0; }
    virtual unsigned width(unsigned /*from*/, unsigned /*len*/, int /*xpos*/, bool /*firstLine = false*/) const { return 0; }

    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;
    virtual int baselinePosition(bool firstLine, bool isRootLineBox = false) const;

    // overrides
    virtual bool isBR() const { return true; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual VisiblePosition positionForPoint(const IntPoint&);

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    mutable int m_lineHeight;
};

} // namespace WebCore

#endif // RenderBR_h

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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef RENDER_BR_H
#define RENDER_BR_H

#include "render_text.h"

namespace DOM {
    class Position;
};

/*
 * The whole class here is a hack to get <br> working, as long as we don't have support for
 * CSS2 :before and :after pseudo elements
 */
namespace khtml {

class RenderBR : public RenderText
{
public:
    RenderBR(DOM::NodeImpl* node);
    virtual ~RenderBR();

    virtual const char *renderName() const { return "RenderBR"; }

    virtual void paint(PaintInfo&, int, int) {}
    
    virtual void position(InlineBox* box, int from, int len, bool reverse);
    virtual unsigned int width(unsigned int, unsigned int, const Font *) const { return 0; }
    virtual unsigned int width( unsigned int, unsigned int, bool) const { return 0; }

    virtual short lineHeight(bool firstLine, bool isRootLineBox=false) const;
    virtual void setStyle(RenderStyle* _style);

    // overrides
    virtual InlineBox* createInlineBox(bool, bool, bool isOnlyRun = false);
    virtual void calcMinMaxWidth() {}
    virtual int minWidth() const { return 0; }
    virtual int maxWidth() const { return 0; }

    virtual int xPos() const { return m_x; }
    virtual int yPos() const { return m_y; }
    virtual int height() const { return m_height; }
    virtual void setPos(int xPos, int yPos);
    
    virtual bool isBR() const { return true; }

    virtual long caretMinOffset() const;
    virtual long caretMaxOffset() const;
    virtual unsigned long caretMaxRenderedOffset() const;
    
    virtual DOM::Position positionForCoordinates(int _x, int _y);
    virtual void caretPos(int offset, bool override, int &_x, int &_y, int &_w, int &_h);

    virtual InlineBox *inlineBox(long offset);
    
private:
    int m_x;
    int m_y;
    int m_height;
    mutable short m_lineHeight;

};

}

#endif

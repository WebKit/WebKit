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

#include "RenderText.h"

namespace WebCore {
    class Position;
};

/*
 * The whole class here is a hack to get <br> working, as long as we don't have support for
 * CSS2 :before and :after pseudo elements
 */
namespace WebCore {

class RenderBR : public RenderText
{
public:
    RenderBR(WebCore::Node* node);
    virtual ~RenderBR();

    virtual const char *renderName() const { return "RenderBR"; }
 
    virtual IntRect selectionRect() { return IntRect(); }

    virtual unsigned int width(unsigned int from, unsigned int len, const Font *f, int xpos) const { return 0; }
    virtual unsigned int width(unsigned int from, unsigned int len, int xpos, bool firstLine = false) const { return 0; }

    virtual short lineHeight(bool firstLine, bool isRootLineBox=false) const;
    virtual short baselinePosition( bool firstLine, bool isRootLineBox=false) const;
    virtual void setStyle(RenderStyle* _style);

    // overrides
    virtual InlineBox* createInlineBox(bool, bool, bool isOnlyRun = false);

    virtual bool isBR() const { return true; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    
    virtual VisiblePosition positionForCoordinates(int x, int y);

    virtual InlineBox *inlineBox(int offset, EAffinity affinity = UPSTREAM);
    
private:
    mutable short m_lineHeight;

};

}

#endif

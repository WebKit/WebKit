/*
 * This file is part of the html renderer for KDE.
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
#ifndef RENDER_BODY
#define RENDER_BODY

#include "render_block.h"

namespace DOM
{
    class HTMLBodyElementImpl;
}

namespace khtml {

class RenderBody : public RenderBlock
{
public:
    RenderBody(DOM::HTMLBodyElementImpl* node);
    virtual ~RenderBody();

    virtual bool isBody() const { return true; }

    virtual const char *renderName() const { return "RenderBody"; }
    virtual void repaint(bool immediate=false);

    virtual void layout();
    virtual void setStyle(RenderStyle* style);

    virtual int availableHeight() const;
    
protected:
    virtual void paintBoxDecorations(QPainter *p,int _x, int _y,
                                     int _w, int _h, int _tx, int _ty);
    bool scrollbarsStyled;
};

}; // end namespace
#endif

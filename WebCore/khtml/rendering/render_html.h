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
#ifndef RENDER_HTML
#define RENDER_HTML

#include "render_flow.h"

namespace DOM {

    class HTMLElementImpl;
}

class QScrollView;

namespace khtml {

    class RenderHtml : public RenderFlow
    {
    public:
	RenderHtml(DOM::HTMLElementImpl* node);
	virtual ~RenderHtml();

	virtual const char *renderName() const { return "RenderHtml"; }

	virtual bool isHtml() const { return true; }
	virtual void setStyle(RenderStyle *style);
	virtual void print( QPainter *, int x, int y, int w, int h, int tx, int ty);
	virtual void repaint(bool immediate=false);
	virtual void layout();
    virtual short containingBlockWidth() const;
    protected:
	virtual void printBoxDecorations(QPainter *p,int _x, int _y,
					 int _w, int _h, int _tx, int _ty);
    private:
        QScrollView* m_view;
    };
};
#endif

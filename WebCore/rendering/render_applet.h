/*
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef render_applet_h
#define render_applet_h

#include "html_elementimpl.h"
#include "render_replaced.h"

namespace WebCore {

class RenderApplet : public RenderWidget
{
public:
    RenderApplet(HTMLElementImpl*, const HashMap<DOMString, DOMString>& args);
    virtual ~RenderApplet();

    virtual const char* renderName() const { return "RenderApplet"; }

    void createWidgetIfNecessary();

    virtual void layout();
    virtual int intrinsicWidth() const;
    virtual int intrinsicHeight() const;
    virtual bool isApplet() const { return true; }

    HTMLElementImpl *element() const
        { return static_cast<HTMLElementImpl*>(RenderObject::element()); }

private:
    HashMap<DOMString, DOMString> m_args;
};

class RenderEmptyApplet : public RenderWidget
{
public:
    RenderEmptyApplet(NodeImpl*);

    virtual const char* renderName() const { return "RenderEmptyApplet"; }

    virtual int intrinsicWidth() const;
    virtual int intrinsicHeight() const;
    virtual void layout();
};

}

#endif

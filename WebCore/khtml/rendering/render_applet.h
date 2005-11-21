/*
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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

#include "rendering/render_replaced.h"
#include "html/html_objectimpl.h"

#include <qwidget.h>
#include <qmap.h>

class KHTMLView;

namespace DOM {
    class HTMLElementImpl;
};

namespace khtml {

class RenderApplet : public RenderWidget
{
public:
    RenderApplet(DOM::HTMLElementImpl* node, const QMap<QString, QString> &args);
    virtual ~RenderApplet();

    virtual const char *renderName() const { return "RenderApplet"; }

    void createWidgetIfNecessary();

    virtual void layout();
    virtual int intrinsicWidth() const;
    virtual int intrinsicHeight() const;
    virtual bool isApplet() const { return true; }

    DOM::HTMLElementImpl *element() const
    { return static_cast<DOM::HTMLElementImpl*>(RenderObject::element()); }

private:
    KJavaAppletContext *m_context;
    QMap<QString, QString> m_args;
};

class RenderEmptyApplet : public RenderWidget
{
public:
    RenderEmptyApplet(DOM::NodeImpl* node);

    virtual const char *renderName() const { return "RenderEmptyApplet"; }

    virtual int intrinsicWidth() const;
    virtual int intrinsicHeight() const;
    virtual void layout();
};

};
#endif

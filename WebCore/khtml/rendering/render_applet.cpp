/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include <config.h>
#include <klocale.h>

#include <kdebug.h>

#include "rendering/render_applet.h"
#include "rendering/render_canvas.h"
#include "xml/dom_docimpl.h"
#include "khtmlview.h"
#include "khtml_part.h"

#include <qlabel.h>

#ifndef Q_WS_QWS // We don't have Java in Qt Embedded

#include "java/kjavaappletwidget.h"
#include "html/html_objectimpl.h"

using namespace khtml;
using namespace DOM;

RenderApplet::RenderApplet(HTMLElementImpl *applet, const QMap<QString, QString> &args )
    : RenderWidget(applet)
{
    // init RenderObject attributes
    setInline(true);

    KJavaAppletContext *context = 0;
    KHTMLPart *part = applet->getDocument()->part();
    if ( part ) {
        context = part->createJavaContext();
    }

#if APPLE_CHANGES
    m_context = context;
    m_args = args;
#else
    if ( context ) {
        //kdDebug(6100) << "RenderApplet::RenderApplet, setting QWidget" << endl;
        setQWidget( new KJavaAppletWidget(context, _view->viewport()) );
        processArguments(args);
    }
#endif
}

RenderApplet::~RenderApplet()
{
}

int RenderApplet::intrinsicWidth() const
{
    int rval = 150;

    if( m_widget )
        rval = ((KJavaAppletWidget*)(m_widget))->sizeHint().width();

    return rval > 10 ? rval : 50;
}

int RenderApplet::intrinsicHeight() const
{
    int rval = 150;

    if( m_widget )
        rval = m_widget->sizeHint().height();

    return rval > 10 ? rval : 50;
}

void RenderApplet::createWidgetIfNecessary()
{
    if (!m_widget) {
        if (static_cast<HTMLAppletElementImpl*>(element())->allParamsAvailable()) {
            // FIXME: Java applets can't be resized (this is a bug in Apple's Java implementation).  In order to work around
            // this problem, we will simply use fixed widths/heights from the style system when we can, since the widget might
            // not have an accurate m_width/m_height.
            int width = style()->width().isFixed() ? style()->width().value : 
                        m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
            int height = style()->height().isFixed() ? style()->height().value :
                         m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
            NodeImpl *child = element()->firstChild();
            while (child) {
                if (child->hasTagName(HTMLTags::param())) {
                    HTMLParamElementImpl *p = static_cast<HTMLParamElementImpl *>(child);
                    m_args.insert(p->name().string(), p->value().string());
                }
                child = child->nextSibling();
            }
        
            setQWidget(new KJavaAppletWidget(QSize(width, height), m_context, m_args));
        }
    }
}

void RenderApplet::layout()
{
    //kdDebug(6100) << "RenderApplet::layout" << endl;

    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    calcWidth();
    calcHeight();

    KJavaAppletWidget *tmp = static_cast<KJavaAppletWidget*>(m_widget);
#if APPLE_CHANGES
    // The applet's QWidget gets created lazily upon first layout.
    if (!tmp)
        createWidgetIfNecessary();
#else 
    if ( tmp ) {
        NodeImpl *child = element()->firstChild();

        while(child) {

            if(child->id() == ID_PARAM) {
                HTMLParamElementImpl *p = static_cast<HTMLParamElementImpl *>(child);
                if(tmp->applet())
                    tmp->applet()->setParameter( p->name(), p->value());
            }
            child = child->nextSibling();
        }
        //kdDebug(6100) << "setting applet widget to size: " << m_width << ", " << m_height << endl;
        m_widget->resize(m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight(),
                         m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom());
        tmp->showApplet();
    }
#endif
    setNeedsLayout(false);
}

#if !APPLE_CHANGES
void RenderApplet::processArguments(const QMap<QString, QString> &args)
{
    KJavaAppletWidget *w = static_cast<KJavaAppletWidget*>(m_widget);
    KJavaApplet* applet = w ? w->applet() : 0;

    if ( applet ) {
        applet->setBaseURL( args[QString::fromLatin1("baseURL") ] );
        applet->setAppletClass( args[QString::fromLatin1("code") ] );

	QString str = args[QString::fromLatin1("codeBase") ];
        if( !str.isEmpty() )
            applet->setCodeBase( str );

	str = args[QString::fromLatin1("name") ];
        if( !str.isNull() )
            applet->setAppletName( str );
        else
            applet->setAppletName( args[QString::fromLatin1("code") ] );

	str = args[QString::fromLatin1("archive") ];
        if( !str.isEmpty() ) 
            applet->setArchives( args[QString::fromLatin1("archive") ] );
    }
}
#endif

RenderEmptyApplet::RenderEmptyApplet(DOM::NodeImpl* node)
  : RenderWidget(node)
{
    // init RenderObject attributes
    setInline(true);

#if APPLE_CHANGES
    // FIXME: Figure out how to handle this.
#else
    QLabel* label = new QLabel(i18n("Java Applet is not loaded. (Java interpreter disabled)"), node->getDocument()->view()->viewport());
    label->setAlignment( Qt::AlignCenter | Qt::WordBreak );
    setQWidget(label);
#endif
}

int RenderEmptyApplet::intrinsicWidth() const
{
    return (m_widget ? m_widget->sizeHint().width() : 150);
}

int RenderEmptyApplet::intrinsicHeight() const
{
    return (m_widget ? m_widget->sizeHint().height() : 150);
}

void RenderEmptyApplet::layout()
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    calcWidth();
    calcHeight();

    // updateWidgetPositions will size the widget, so we don't need to do that here.
#if !APPLE_CHANGES
    if(m_widget)
    {
        //kdDebug(6100) << "RenderEmptyApplet::layout, m_width = " << m_width << ", m_height = " << m_height << endl;
        m_widget->resize(m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                         m_height-borderTop()-borderBottom()-paddingTop()-paddingBottom());
    }
#endif
    
    setNeedsLayout(false);
}
#endif

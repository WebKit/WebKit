/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "config.h"
#include "RenderPart.h"

#include "Document.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "Page.h"

namespace WebCore {

using namespace HTMLNames;

RenderPart::RenderPart(Node* node)
    : RenderWidget(node)
{
    // init RenderObject attributes
    setInline(false);
}

RenderPart::~RenderPart()
{
    // Since deref ends up calling setWidget back on us, need to make sure
    // that widget is already 0 so it won't do any work.
    Widget* widget = m_widget;
    m_widget = 0;
    if (widget && widget->isFrameView())
        static_cast<FrameView*>(widget)->deref();
    else
        delete widget;
}

void RenderPart::setWidget(Widget* widget)
{
    if (widget != m_widget) {
        if (widget && widget->isFrameView())
            static_cast<FrameView*>(widget)->ref();
        RenderWidget::setWidget(widget);

        // make sure the scrollbars are set correctly for restore
        // ### find better fix
        viewCleared();
    }
}

void RenderPart::viewCleared()
{
}

void RenderPart::deleteWidget()
{
    if (m_widget && m_widget->isFrameView())
        static_cast<FrameView*>(m_widget)->deref();
    else
        delete m_widget;
}

// FIXME: This should not be necessary.  Remove this once WebKit knows to properly schedule
// layouts using WebCore when objects resize.
void RenderPart::updateWidgetPosition()
{
    if (!m_widget)
        return;
    
    FloatPoint absPos = localToAbsolute();
    absPos.move(borderLeft() + paddingLeft(), borderTop() + paddingTop());
    int w = width() - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    int h = height() - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    IntRect newBounds(absPos.x(), absPos.y(), w, h);
    bool boundsChanged = newBounds != m_widget->frameRect();
    if (boundsChanged) {
        // The widget changed positions.  Update the frame geometry.
        RenderArena *arena = ref();
        element()->ref();
        m_widget->setFrameRect(newBounds);
        element()->deref();
        deref(arena);
    }

    // if the frame bounds got changed, or if view needs layout (possibly indicating
    // content size is wrong) we have to do a layout to set the right widget size
    if (m_widget && m_widget->isFrameView()) {
        FrameView* frameView = static_cast<FrameView*>(m_widget);
        if (boundsChanged || frameView->needsLayout())
            frameView->layout();
    }
}

}

/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "HTMLFrameSetElement.h"

#include "Document.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "Length.h"
#include "dom2_eventsimpl.h"
#include "RenderFrameSet.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLFrameSetElement::HTMLFrameSetElement(Document *doc)
    : HTMLElement(framesetTag, doc)
    , m_rows(0)
    , m_cols(0)
    , m_totalRows(1)
    , m_totalCols(1)
    , m_border(4)
    , frameborder(true)
    , frameBorderSet(false)
    , noresize(false)
{
}

HTMLFrameSetElement::~HTMLFrameSetElement()
{
    if (m_rows)
        delete [] m_rows;
    if (m_cols)
        delete [] m_cols;
}

bool HTMLFrameSetElement::checkDTD(const Node* newChild)
{
    // FIXME: Old code had adjacent double returns and seemed to want to do something with NOFRAMES (but didn't).
    // What is the correct behavior?
    return newChild->hasTagName(framesetTag) || newChild->hasTagName(frameTag);
}

void HTMLFrameSetElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == rowsAttr) {
        if (!attr->isNull()) {
            if (m_rows) delete [] m_rows;
            m_rows = attr->value().toLengthArray(m_totalRows);
            setChanged();
        }
    } else if (attr->name() == colsAttr) {
        if (!attr->isNull()) {
            delete [] m_cols;
            m_cols = attr->value().toLengthArray(m_totalCols);
            setChanged();
        }
    } else if (attr->name() == frameborderAttr) {
        // false or "no" or "0"..
        if (attr->value().toInt() == 0) {
            frameborder = false;
            m_border = 0;
        }
        frameBorderSet = true;
    } else if (attr->name() == noresizeAttr) {
        noresize = true;
    } else if (attr->name() == borderAttr) {
        m_border = attr->value().toInt();
        if(!m_border)
            frameborder = false;
    } else if (attr->name() == onloadAttr) {
        document()->setHTMLWindowEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        document()->setHTMLWindowEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        document()->setHTMLWindowEventListener(unloadEvent, attr);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLFrameSetElement::rendererIsNeeded(RenderStyle *style)
{
    // Ignore display: none but do pay attention if a stylesheet has caused us to delay our loading.
    return style->isStyleAvailable();
}

RenderObject *HTMLFrameSetElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderFrameSet(this);
}

void HTMLFrameSetElement::attach()
{
    // inherit default settings from parent frameset
    HTMLElement* node = static_cast<HTMLElement*>(parentNode());
    while (node) {
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElement* frameset = static_cast<HTMLFrameSetElement*>(node);
            if(!frameBorderSet)  frameborder = frameset->frameBorder();
            if(!noresize)  noresize = frameset->noResize();
            break;
        }
        node = static_cast<HTMLElement*>(node->parentNode());
    }

    HTMLElement::attach();
}

void HTMLFrameSetElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && !noresize && renderer()) {
        static_cast<WebCore::RenderFrameSet*>(renderer())->userResize(static_cast<MouseEvent*>(evt));
        evt->setDefaultHandled();
    }

    HTMLElement::defaultEventHandler(evt);
}

void HTMLFrameSetElement::recalcStyle(StyleChange ch)
{
    if (changed() && renderer()) {
        renderer()->setNeedsLayout(true);
        setChanged(false);
    }
    HTMLElement::recalcStyle(ch);
}

String HTMLFrameSetElement::cols() const
{
    return getAttribute(colsAttr);
}

void HTMLFrameSetElement::setCols(const String &value)
{
    setAttribute(colsAttr, value);
}

String HTMLFrameSetElement::rows() const
{
    return getAttribute(rowsAttr);
}

void HTMLFrameSetElement::setRows(const String &value)
{
    setAttribute(rowsAttr, value);
}

}

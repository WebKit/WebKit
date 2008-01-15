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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLFrameSetElement.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "Length.h"
#include "MouseEvent.h"
#include "RenderFrameSet.h"
#include "Text.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLFrameSetElement::HTMLFrameSetElement(Document *doc)
    : HTMLElement(framesetTag, doc)
    , m_rows(0)
    , m_cols(0)
    , m_totalRows(1)
    , m_totalCols(1)
    , m_border(6)
    , m_borderSet(false)
    , m_borderColorSet(false)
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
    if (newChild->isTextNode())
        return static_cast<const Text*>(newChild)->containsOnlyWhitespace();
    return newChild->hasTagName(framesetTag) || newChild->hasTagName(frameTag);
}

bool HTMLFrameSetElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == bordercolorAttr) {
        result = eUniversal;
        return true;
    }

    return HTMLElement::mapToEntry(attrName, result);
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
        if (!attr->isNull()) {
            // false or "no" or "0"..
            if (attr->value().toInt() == 0) {
                frameborder = false;
                m_border = 0;
            }
            frameBorderSet = true;
        } else {
            frameborder = false;
            frameBorderSet = false;
        }
    } else if (attr->name() == noresizeAttr) {
        noresize = true;
    } else if (attr->name() == borderAttr) {
        if (!attr->isNull()) {
            m_border = attr->value().toInt();
            if (!m_border)
                frameborder = false;
            m_borderSet = true;
        } else
            m_borderSet = false;
    } else if (attr->name() == bordercolorAttr) {
        m_borderColorSet = attr->decl();
        if (!attr->decl() && !attr->isEmpty()) {
            addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
            m_borderColorSet = true;
        }
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
    // For compatibility, frames render even when display: none is set.
    // However, we delay creating a renderer until stylesheets have loaded. 
    return style->isStyleAvailable();
}

RenderObject *HTMLFrameSetElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (style->contentData())
        return RenderObject::createObject(this, style);
    
    return new (arena) RenderFrameSet(this);
}

void HTMLFrameSetElement::attach()
{
    // Inherit default settings from parent frameset
    // FIXME: This is not dynamic.
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElement* frameset = static_cast<HTMLFrameSetElement*>(node);
            if (!frameBorderSet)
                frameborder = frameset->hasFrameBorder();
            if (frameborder) {
                if (!m_borderSet)
                    m_border = frameset->border();
                if (!m_borderColorSet)
                    m_borderColorSet = frameset->hasBorderColor();
            }
            if (!noresize)
                noresize = frameset->noResize();
            break;
        }
    }

    HTMLElement::attach();
}

void HTMLFrameSetElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && !noresize && renderer()) {
        if (static_cast<RenderFrameSet*>(renderer())->userResize(static_cast<MouseEvent*>(evt))) {
            evt->setDefaultHandled();
            return;
        }
    }
    HTMLElement::defaultEventHandler(evt);
}

void HTMLFrameSetElement::recalcStyle(StyleChange ch)
{
    if (changed() && renderer()) {
        renderer()->setNeedsLayout(true);
        setChanged(NoStyleChange);
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

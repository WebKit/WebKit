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
#include "HTMLIFrameElement.h"

#include "CSSPropertyNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderPartObject.h"

namespace WebCore {

using namespace HTMLNames;

HTMLIFrameElement::HTMLIFrameElement(Document* doc)
    : HTMLFrameElement(iframeTag, doc)
    , needWidgetUpdate(false)
{
    m_frameBorder = false;
    m_marginWidth = -1;
    m_marginHeight = -1;
}

HTMLIFrameElement::~HTMLIFrameElement()
{
}

bool HTMLIFrameElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr || attrName == heightAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == alignAttr) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLIFrameElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == alignAttr)
        addHTMLAlignment(attr);
    else if (attr->name() == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument *doc = static_cast<HTMLDocument *>(document());
            doc->removeDocExtraNamedItem(oldNameAttr);
            doc->addDocExtraNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLFrameElement::parseMappedAttribute(attr);
}

void HTMLIFrameElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->addDocExtraNamedItem(oldNameAttr);
    }

    HTMLElement::insertedIntoDocument();
}

void HTMLIFrameElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->removeDocExtraNamedItem(oldNameAttr);
    }

    HTMLElement::removedFromDocument();
}

bool HTMLIFrameElement::rendererIsNeeded(RenderStyle *style)
{
    // Don't ignore display: none the way frame does.
    return isURLAllowed(m_URL) && style->display() != NONE;
}

RenderObject *HTMLIFrameElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElement::attach()
{
    m_name = getAttribute(nameAttr);
    if (m_name.isNull())
        m_name = getAttribute(idAttr);

    HTMLElement::attach();

    Frame* parentFrame = document()->frame();
    if (renderer() && parentFrame) {
        parentFrame->page()->incrementFrameCount();
        m_name = parentFrame->tree()->uniqueChildName(m_name);
        static_cast<RenderPartObject*>(renderer())->updateWidget();
        needWidgetUpdate = false;
    }
}

void HTMLIFrameElement::recalcStyle( StyleChange ch )
{
    if (needWidgetUpdate) {
        if (renderer())
            static_cast<RenderPartObject*>(renderer())->updateWidget();
        needWidgetUpdate = false;
    }
    HTMLElement::recalcStyle( ch );
}

void HTMLIFrameElement::openURL()
{
    needWidgetUpdate = true;
    setChanged();
}

bool HTMLIFrameElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
}

String HTMLIFrameElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLIFrameElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLIFrameElement::height() const
{
    return getAttribute(heightAttr);
}

void HTMLIFrameElement::setHeight(const String &value)
{
    setAttribute(heightAttr, value);
}

String HTMLIFrameElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

String HTMLIFrameElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLIFrameElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}

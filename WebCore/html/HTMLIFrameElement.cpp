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
#include "HTMLIFrameElement.h"

#include "CSSPropertyNames.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "RenderPartObject.h"

namespace WebCore {

using namespace HTMLNames;

HTMLIFrameElement::HTMLIFrameElement(Document* doc)
    : HTMLFrameElementBase(iframeTag, doc)
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
    
    if (attrName == frameborderAttr) {
        result = eReplaced;
        return false;
    }

    return HTMLFrameElementBase::mapToEntry(attrName, result);
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
            HTMLDocument* doc = static_cast<HTMLDocument* >(document());
            doc->removeDocExtraNamedItem(oldNameAttr);
            doc->addDocExtraNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else if (attr->name() == frameborderAttr) {
        // Frame border doesn't really match the HTML4 spec definition for iframes.  It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!attr->isNull() && !attr->value().toInt())
            // Add a rule that nulls out our border width.
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, "0");
    } else
        HTMLFrameElementBase::parseMappedAttribute(attr);
}

bool HTMLIFrameElement::rendererIsNeeded(RenderStyle* style)
{
    return isURLAllowed(m_URL) && style->display() != NONE;
}

RenderObject* HTMLIFrameElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderPartObject(this);
}

void HTMLIFrameElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* doc = static_cast<HTMLDocument*>(document());
        doc->addDocExtraNamedItem(oldNameAttr);
    }

    HTMLFrameElementBase::insertedIntoDocument();
}

void HTMLIFrameElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* doc = static_cast<HTMLDocument*>(document());
        doc->removeDocExtraNamedItem(oldNameAttr);
    }

    HTMLFrameElementBase::removedFromDocument();
}

void HTMLIFrameElement::attach()
{
    HTMLFrameElementBase::attach();

    if (RenderPartObject* renderPartObject = static_cast<RenderPartObject*>(renderer()))
        renderPartObject->updateWidget(false);
}

bool HTMLIFrameElement::isURLAttribute(Attribute* attr) const
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

String HTMLIFrameElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLIFrameElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

}

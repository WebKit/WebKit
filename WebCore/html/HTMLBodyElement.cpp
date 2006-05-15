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
#include "HTMLBodyElement.h"

#include "css_stylesheetimpl.h"
#include "CSSPropertyNames.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "csshelper.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(Document* doc)
    : HTMLElement(bodyTag, doc)
{
}

HTMLBodyElement::~HTMLBodyElement()
{
    if (m_linkDecl) {
        m_linkDecl->setNode(0);
        m_linkDecl->setParent(0);
    }
}

void HTMLBodyElement::createLinkDecl()
{
    m_linkDecl = new CSSMutableStyleDeclaration;
    m_linkDecl->setParent(document()->elementSheet());
    m_linkDecl->setNode(this);
    m_linkDecl->setStrictParsing(!document()->inCompatMode());
}

bool HTMLBodyElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == backgroundAttr) {
        result = (MappedAttributeEntry)(eLastEntry + document()->docID());
        return false;
    } 
    
    if (attrName == bgcolorAttr ||
        attrName == textAttr ||
        attrName == marginwidthAttr ||
        attrName == leftmarginAttr ||
        attrName == marginheightAttr ||
        attrName == topmarginAttr ||
        attrName == bgpropertiesAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLBodyElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == backgroundAttr) {
        String url = WebCore::parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSS_PROP_BACKGROUND_IMAGE, document()->completeURL(url));
    } else if (attr->name() == marginwidthAttr || attr->name() == leftmarginAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
    } else if (attr->name() == marginheightAttr || attr->name() == topmarginAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
    } else if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == textAttr) {
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
    } else if (attr->name() == bgpropertiesAttr) {
        if (equalIgnoringCase(attr->value(), "fixed"))
            addCSSProperty(attr, CSS_PROP_BACKGROUND_ATTACHMENT, CSS_VAL_FIXED);
    } else if (attr->name() == vlinkAttr ||
               attr->name() == alinkAttr ||
               attr->name() == linkAttr) {
        if (attr->isNull()) {
            if (attr->name() == linkAttr)
                document()->resetLinkColor();
            else if (attr->name() == vlinkAttr)
                document()->resetVisitedLinkColor();
            else
                document()->resetActiveLinkColor();
        } else {
            if (!m_linkDecl)
                createLinkDecl();
            m_linkDecl->setProperty(CSS_PROP_COLOR, attr->value(), false, false);
            RefPtr<CSSValue> val = m_linkDecl->getPropertyCSSValue(CSS_PROP_COLOR);
            if (val && val->isPrimitiveValue()) {
                Color col = document()->styleSelector()->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValue*>(val.get()));
                if (attr->name() == linkAttr)
                    document()->setLinkColor(col);
                else if (attr->name() == vlinkAttr)
                    document()->setVisitedLinkColor(col);
                else
                    document()->setActiveLinkColor(col);
            }
        }
        
        if (attached())
            document()->recalcStyle(Force);
    } else if (attr->name() == onloadAttr) {
        document()->setHTMLWindowEventListener(loadEvent, attr);
    } else if (attr->name() == onbeforeunloadAttr) {
        document()->setHTMLWindowEventListener(beforeunloadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        document()->setHTMLWindowEventListener(unloadEvent, attr);
    } else if (attr->name() == onblurAttr) {
        document()->setHTMLWindowEventListener(blurEvent, attr);
    } else if (attr->name() == onfocusAttr) {
        document()->setHTMLWindowEventListener(focusEvent, attr);
    } else if (attr->name() == onresizeAttr) {
        document()->setHTMLWindowEventListener(resizeEvent, attr);
    } else if (attr->name() == onscrollAttr) {
        document()->setHTMLWindowEventListener(scrollEvent, attr);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLBodyElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    // FIXME: perhaps this code should be in attach() instead of here

    FrameView *w = document()->view();
    if (w && w->marginWidth() != -1)
        setAttribute(marginwidthAttr, String::number(w->marginWidth()));
    if (w && w->marginHeight() != -1)
        setAttribute(marginheightAttr, String::number(w->marginHeight()));

    if (w)
        w->scheduleRelayout();
}

bool HTMLBodyElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

String HTMLBodyElement::aLink() const
{
    return getAttribute(alinkAttr);
}

void HTMLBodyElement::setALink(const String &value)
{
    setAttribute(alinkAttr, value);
}

String HTMLBodyElement::background() const
{
    return getAttribute(backgroundAttr);
}

void HTMLBodyElement::setBackground(const String &value)
{
    setAttribute(backgroundAttr, value);
}

String HTMLBodyElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLBodyElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLBodyElement::link() const
{
    return getAttribute(linkAttr);
}

void HTMLBodyElement::setLink(const String &value)
{
    setAttribute(linkAttr, value);
}

String HTMLBodyElement::text() const
{
    return getAttribute(textAttr);
}

void HTMLBodyElement::setText(const String &value)
{
    setAttribute(textAttr, value);
}

String HTMLBodyElement::vLink() const
{
    return getAttribute(vlinkAttr);
}

void HTMLBodyElement::setVLink(const String &value)
{
    setAttribute(vlinkAttr, value);
}

}

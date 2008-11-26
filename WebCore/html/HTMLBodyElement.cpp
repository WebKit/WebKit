/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "HTMLBodyElement.h"

#include "CSSHelper.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(const QualifiedName& qName, Document* doc)
    : HTMLElement(qName, doc)
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
    m_linkDecl = CSSMutableStyleDeclaration::create();
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
        String url = parseURL(attr->value());
        if (!url.isEmpty())
            addCSSImageProperty(attr, CSSPropertyBackgroundImage, document()->completeURL(url).string());
    } else if (attr->name() == marginwidthAttr || attr->name() == leftmarginAttr) {
        addCSSLength(attr, CSSPropertyMarginRight, attr->value());
        addCSSLength(attr, CSSPropertyMarginLeft, attr->value());
    } else if (attr->name() == marginheightAttr || attr->name() == topmarginAttr) {
        addCSSLength(attr, CSSPropertyMarginBottom, attr->value());
        addCSSLength(attr, CSSPropertyMarginTop, attr->value());
    } else if (attr->name() == bgcolorAttr) {
        addCSSColor(attr, CSSPropertyBackgroundColor, attr->value());
    } else if (attr->name() == textAttr) {
        addCSSColor(attr, CSSPropertyColor, attr->value());
    } else if (attr->name() == bgpropertiesAttr) {
        if (equalIgnoringCase(attr->value(), "fixed"))
            addCSSProperty(attr, CSSPropertyBackgroundAttachment, CSSValueFixed);
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
            m_linkDecl->setProperty(CSSPropertyColor, attr->value(), false, false);
            RefPtr<CSSValue> val = m_linkDecl->getPropertyCSSValue(CSSPropertyColor);
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
    } else if (attr->name() == onloadAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().loadEvent, attr);
    else if (attr->name() == onbeforeunloadAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().beforeunloadEvent, attr);
    else if (attr->name() == onunloadAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().unloadEvent, attr);
    else if (attr->name() == onblurAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().blurEvent, attr);
    else if (attr->name() == onfocusAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().focusEvent, attr);
    else if (attr->name() == onresizeAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().resizeEvent, attr);
    else if (attr->name() == onscrollAttr)
        document()->setWindowInlineEventListenerForTypeAndAttribute(eventNames().scrollEvent, attr);
    else if (attr->name() == onstorageAttr) {
        // The HTML5 spec currently specifies that storage events are fired only at the body element of
        // an HTMLDocument, which is why the onstorage attribute differs from the ones before it.
        // The spec might change on this, and then so should we!
        setInlineEventListenerForTypeAndAttribute(eventNames().storageEvent, attr);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLBodyElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    // FIXME: Perhaps this code should be in attach() instead of here.
    Element* ownerElement = document()->ownerElement();
    if (ownerElement && (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))) {
        HTMLFrameElementBase* ownerFrameElement = static_cast<HTMLFrameElementBase*>(ownerElement);
        int marginWidth = ownerFrameElement->getMarginWidth();
        if (marginWidth != -1)
            setAttribute(marginwidthAttr, String::number(marginWidth));
        int marginHeight = ownerFrameElement->getMarginHeight();
        if (marginHeight != -1)
            setAttribute(marginheightAttr, String::number(marginHeight));
    }

    // FIXME: This call to scheduleRelayout should not be needed here.
    // But without it we hang during WebKit tests; need to fix that and remove this.
    if (FrameView* view = document()->view())
        view->scheduleRelayout();
}

bool HTMLBodyElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == backgroundAttr;
}

String HTMLBodyElement::aLink() const
{
    return getAttribute(alinkAttr);
}

void HTMLBodyElement::setALink(const String& value)
{
    setAttribute(alinkAttr, value);
}

String HTMLBodyElement::background() const
{
    return getAttribute(backgroundAttr);
}

void HTMLBodyElement::setBackground(const String& value)
{
    setAttribute(backgroundAttr, value);
}

String HTMLBodyElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLBodyElement::setBgColor(const String& value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLBodyElement::link() const
{
    return getAttribute(linkAttr);
}

void HTMLBodyElement::setLink(const String& value)
{
    setAttribute(linkAttr, value);
}

String HTMLBodyElement::text() const
{
    return getAttribute(textAttr);
}

void HTMLBodyElement::setText(const String& value)
{
    setAttribute(textAttr, value);
}

String HTMLBodyElement::vLink() const
{
    return getAttribute(vlinkAttr);
}

void HTMLBodyElement::setVLink(const String& value)
{
    setAttribute(vlinkAttr, value);
}

int HTMLBodyElement::scrollLeft() const
{
    // Update the document's layout.
    Document* doc = document();
    doc->updateLayoutIgnorePendingStylesheets();
    FrameView* view = doc->view();
    return view ? view->scrollX() : 0;
}

void HTMLBodyElement::setScrollLeft(int scrollLeft)
{
    FrameView* sview = ownerDocument()->view();
    if (sview) {
        // Update the document's layout
        document()->updateLayoutIgnorePendingStylesheets();
        sview->setScrollPosition(IntPoint(scrollLeft, sview->scrollY()));
    }    
}

int HTMLBodyElement::scrollTop() const
{
    // Update the document's layout.
    Document* doc = document();
    doc->updateLayoutIgnorePendingStylesheets();
    FrameView* view = doc->view();
    return view ? view->scrollY() : 0;
}

void HTMLBodyElement::setScrollTop(int scrollTop)
{
    FrameView* sview = ownerDocument()->view();
    if (sview) {
        // Update the document's layout
        document()->updateLayoutIgnorePendingStylesheets();
        sview->setScrollPosition(IntPoint(sview->scrollX(), scrollTop));
    }        
}

int HTMLBodyElement::scrollHeight() const
{
    // Update the document's layout.
    Document* doc = document();
    doc->updateLayoutIgnorePendingStylesheets();
    FrameView* view = doc->view();
    return view ? view->contentsHeight() : 0;    
}

int HTMLBodyElement::scrollWidth() const
{
    // Update the document's layout.
    Document* doc = document();
    doc->updateLayoutIgnorePendingStylesheets();
    FrameView* view = doc->view();
    return view ? view->contentsWidth() : 0;    
}

void HTMLBodyElement::getSubresourceAttributeStrings(Vector<String>& urls) const
{
    urls.append(background());
}

}

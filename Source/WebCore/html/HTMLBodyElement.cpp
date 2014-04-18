/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "CSSImageValue.h"
#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Page.h"
#include "StyleProperties.h"

namespace WebCore {

using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(bodyTag));
}

PassRefPtr<HTMLBodyElement> HTMLBodyElement::create(Document& document)
{
    return adoptRef(new HTMLBodyElement(bodyTag, document));
}

PassRefPtr<HTMLBodyElement> HTMLBodyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLBodyElement(tagName, document));
}

HTMLBodyElement::~HTMLBodyElement()
{
}

bool HTMLBodyElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == marginwidthAttr || name == leftmarginAttr || name == marginheightAttr || name == topmarginAttr || name == bgcolorAttr || name == textAttr || name == bgpropertiesAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLBodyElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty()) {
            auto imageValue = CSSImageValue::create(document().completeURL(url).string());
            imageValue.get().setInitiator(localName());
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, std::move(imageValue)));
        }
    } else if (name == marginwidthAttr || name == leftmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    } else if (name == marginheightAttr || name == topmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    } else if (name == bgcolorAttr) {
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    } else if (name == textAttr) {
        addHTMLColorToStyle(style, CSSPropertyColor, value);
    } else if (name == bgpropertiesAttr) {
        if (equalIgnoringCase(value, "fixed"))
           addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundAttachment, CSSValueFixed);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLBodyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == vlinkAttr || name == alinkAttr || name == linkAttr) {
        if (value.isNull()) {
            if (name == linkAttr)
                document().resetLinkColor();
            else if (name == vlinkAttr)
                document().resetVisitedLinkColor();
            else
                document().resetActiveLinkColor();
        } else {
            RGBA32 color;
            if (CSSParser::parseColor(color, value, !document().inQuirksMode())) {
                if (name == linkAttr)
                    document().setLinkColor(color);
                else if (name == vlinkAttr)
                    document().setVisitedLinkColor(color);
                else
                    document().setActiveLinkColor(color);
            }
        }

        setNeedsStyleRecalc();
    } else if (name == onloadAttr)
        document().setWindowAttributeEventListener(eventNames().loadEvent, name, value);
    else if (name == onbeforeunloadAttr)
        document().setWindowAttributeEventListener(eventNames().beforeunloadEvent, name, value);
    else if (name == onunloadAttr)
        document().setWindowAttributeEventListener(eventNames().unloadEvent, name, value);
    else if (name == onpagehideAttr)
        document().setWindowAttributeEventListener(eventNames().pagehideEvent, name, value);
    else if (name == onpageshowAttr)
        document().setWindowAttributeEventListener(eventNames().pageshowEvent, name, value);
    else if (name == onpopstateAttr)
        document().setWindowAttributeEventListener(eventNames().popstateEvent, name, value);
    else if (name == onblurAttr)
        document().setWindowAttributeEventListener(eventNames().blurEvent, name, value);
    else if (name == onfocusAttr)
        document().setWindowAttributeEventListener(eventNames().focusEvent, name, value);
#if ENABLE(ORIENTATION_EVENTS)
    else if (name == onorientationchangeAttr)
        document().setWindowAttributeEventListener(eventNames().orientationchangeEvent, name, value);
#endif
    else if (name == onhashchangeAttr)
        document().setWindowAttributeEventListener(eventNames().hashchangeEvent, name, value);
    else if (name == onresizeAttr)
        document().setWindowAttributeEventListener(eventNames().resizeEvent, name, value);
    else if (name == onscrollAttr)
        document().setWindowAttributeEventListener(eventNames().scrollEvent, name, value);
    else if (name == onselectionchangeAttr)
        document().setAttributeEventListener(eventNames().selectionchangeEvent, name, value);
    else if (name == onstorageAttr)
        document().setWindowAttributeEventListener(eventNames().storageEvent, name, value);
    else if (name == ononlineAttr)
        document().setWindowAttributeEventListener(eventNames().onlineEvent, name, value);
    else if (name == onofflineAttr)
        document().setWindowAttributeEventListener(eventNames().offlineEvent, name, value);
#if ENABLE(WILL_REVEAL_EDGE_EVENTS)
    else if (name == onwebkitwillrevealbottomAttr)
        document().setWindowAttributeEventListener(eventNames().webkitwillrevealbottomEvent, name, value);
    else if (name == onwebkitwillrevealleftAttr)
        document().setWindowAttributeEventListener(eventNames().webkitwillrevealleftEvent, name, value);
    else if (name == onwebkitwillrevealrightAttr)
        document().setWindowAttributeEventListener(eventNames().webkitwillrevealrightEvent, name, value);
    else if (name == onwebkitwillrevealtopAttr)
        document().setWindowAttributeEventListener(eventNames().webkitwillrevealtopEvent, name, value);
#endif
    else
        HTMLElement::parseAttribute(name, value);
}

Node::InsertionNotificationRequest HTMLBodyElement::insertedInto(ContainerNode& insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (!insertionPoint.inDocument())
        return InsertionDone;

    // FIXME: It's surprising this is web compatible since it means a marginwidth and marginheight attribute can
    // magically appear on the <body> of all documents embedded through <iframe> or <frame>.
    // FIXME: Perhaps this code should be in attach() instead of here.
    Element* ownerElement = document().ownerElement();
    if (ownerElement && isHTMLFrameElementBase(*ownerElement)) {
        HTMLFrameElementBase& ownerFrameElement = toHTMLFrameElementBase(*ownerElement);
        int marginWidth = ownerFrameElement.marginWidth();
        if (marginWidth != -1)
            setIntegralAttribute(marginwidthAttr, marginWidth);
        int marginHeight = ownerFrameElement.marginHeight();
        if (marginHeight != -1)
            setIntegralAttribute(marginheightAttr, marginHeight);
    }

    return InsertionDone;
}

bool HTMLBodyElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLBodyElement::supportsFocus() const
{
    return hasEditableStyle() || HTMLElement::supportsFocus();
}

static int adjustForZoom(int value, Frame& frame)
{
    float zoomFactor = frame.pageZoomFactor() * frame.frameScaleFactor();
    if (zoomFactor == 1)
        return value;
    // Needed because of truncation (rather than rounding) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
}

int HTMLBodyElement::scrollLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return 0;
    FrameView* view = frame->view();
    if (!view)
        return 0;
    return adjustForZoom(view->contentsScrollPosition().x(), *frame);
}

void HTMLBodyElement::setScrollLeft(int scrollLeft)
{
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return;
    FrameView* view = frame->view();
    if (!view)
        return;
    view->setScrollPosition(IntPoint(static_cast<int>(scrollLeft * frame->pageZoomFactor() * frame->frameScaleFactor()), view->scrollY()));
}

int HTMLBodyElement::scrollTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return 0;
    FrameView* view = frame->view();
    if (!view)
        return 0;
    return adjustForZoom(view->contentsScrollPosition().y(), *frame);
}

void HTMLBodyElement::setScrollTop(int scrollTop)
{
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return;
    FrameView* view = frame->view();
    if (!view)
        return;
    view->setScrollPosition(IntPoint(view->scrollX(), static_cast<int>(scrollTop * frame->pageZoomFactor() * frame->frameScaleFactor())));
}

int HTMLBodyElement::scrollHeight()
{
    // Update the document's layout.
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return 0;
    FrameView* view = frame->view();
    if (!view)
        return 0;
    return adjustForZoom(view->contentsHeight(), *frame);
}

int HTMLBodyElement::scrollWidth()
{
    // Update the document's layout.
    document().updateLayoutIgnorePendingStylesheets();
    Frame* frame = document().frame();
    if (!frame)
        return 0;
    FrameView* view = frame->view();
    if (!view)
        return 0;
    return adjustForZoom(view->contentsWidth(), *frame);
}

void HTMLBodyElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(getAttribute(backgroundAttr)));
}

} // namespace WebCore

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006-2010, 2015 Apple Inc. All rights reserved.
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
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(bodyTag));
}

bool HTMLBodyElement::isFirstBodyElementOfDocument() const
{
    // By spec http://dev.w3.org/csswg/cssom-view/#the-html-body-element
    // "The HTML body element is the first body HTML element child of the root HTML element html."
    return document().body() == this;
}

Ref<HTMLBodyElement> HTMLBodyElement::create(Document& document)
{
    return adoptRef(*new HTMLBodyElement(bodyTag, document));
}

Ref<HTMLBodyElement> HTMLBodyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLBodyElement(tagName, document));
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
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, WTFMove(imageValue)));
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
        if (equalLettersIgnoringASCIICase(value, "fixed"))
           addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundAttachment, CSSValueFixed);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

HTMLElement::EventHandlerNameMap HTMLBodyElement::createWindowEventHandlerNameMap()
{
    static const QualifiedName* const table[] = {
        &onbeforeunloadAttr,
        &onblurAttr,
        &onerrorAttr,
        &onfocusAttr,
        &onfocusinAttr,
        &onfocusoutAttr,
        &onhashchangeAttr,
        &onloadAttr,
        &onmessageAttr,
        &onofflineAttr,
        &ononlineAttr,
        &onorientationchangeAttr,
        &onpagehideAttr,
        &onpageshowAttr,
        &onpopstateAttr,
        &onresizeAttr,
        &onscrollAttr,
        &onstorageAttr,
        &onunloadAttr,
        &onwebkitmouseforcechangedAttr,
        &onwebkitmouseforcedownAttr,
        &onwebkitmouseforceupAttr,
        &onwebkitmouseforcewillbeginAttr,
        &onwebkitwillrevealbottomAttr,
        &onwebkitwillrevealleftAttr,
        &onwebkitwillrevealrightAttr,
        &onwebkitwillrevealtopAttr,
    };

    EventHandlerNameMap map;
    populateEventHandlerNameMap(map, table);
    return map;
}

const AtomicString& HTMLBodyElement::eventNameForWindowEventHandlerAttribute(const QualifiedName& attributeName)
{
    static NeverDestroyed<EventHandlerNameMap> map = createWindowEventHandlerNameMap();
    return eventNameForEventHandlerAttribute(attributeName, map.get());
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
        return;
    }

    if (name == onselectionchangeAttr) {
        document().setAttributeEventListener(eventNames().selectionchangeEvent, name, value);
        return;
    }

    auto& eventName = eventNameForWindowEventHandlerAttribute(name);
    if (!eventName.isNull()) {
        document().setWindowAttributeEventListener(eventName, name, value);
        return;
    }

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
    HTMLFrameOwnerElement* ownerElement = document().ownerElement();
    if (is<HTMLFrameElementBase>(ownerElement)) {
        HTMLFrameElementBase& ownerFrameElement = downcast<HTMLFrameElementBase>(*ownerElement);
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

static int adjustForZoom(int value, const Frame& frame)
{
    double zoomFactor = frame.pageZoomFactor() * frame.frameScaleFactor();
    if (zoomFactor == 1)
        return value;
    // Needed because of truncation (rather than rounding) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
}

int HTMLBodyElement::scrollLeft()
{
    if (isFirstBodyElementOfDocument()) {
        document().updateLayoutIgnorePendingStylesheets();
        Frame* frame = document().frame();
        if (!frame)
            return 0;
        FrameView* view = frame->view();
        if (!view)
            return 0;
        return adjustForZoom(view->contentsScrollPosition().x(), *frame);
    }
    return HTMLElement::scrollLeft();
}

void HTMLBodyElement::setScrollLeft(int scrollLeft)
{
    if (isFirstBodyElementOfDocument()) {
        document().updateLayoutIgnorePendingStylesheets();
        Frame* frame = document().frame();
        if (!frame)
            return;
        FrameView* view = frame->view();
        if (!view)
            return;
        view->setScrollPosition(IntPoint(static_cast<int>(scrollLeft * frame->pageZoomFactor() * frame->frameScaleFactor()), view->scrollY()));
    }
    HTMLElement::setScrollLeft(scrollLeft);
}

int HTMLBodyElement::scrollTop()
{
    if (isFirstBodyElementOfDocument()) {
        document().updateLayoutIgnorePendingStylesheets();
        Frame* frame = document().frame();
        if (!frame)
            return 0;
        FrameView* view = frame->view();
        if (!view)
            return 0;
        return adjustForZoom(view->contentsScrollPosition().y(), *frame);
    }
    return HTMLElement::scrollTop();
}

void HTMLBodyElement::setScrollTop(int scrollTop)
{
    if (isFirstBodyElementOfDocument()) {
        document().updateLayoutIgnorePendingStylesheets();
        Frame* frame = document().frame();
        if (!frame)
            return;
        FrameView* view = frame->view();
        if (!view)
            return;
        view->setScrollPosition(IntPoint(view->scrollX(), static_cast<int>(scrollTop * frame->pageZoomFactor() * frame->frameScaleFactor())));
    }
    return HTMLElement::setScrollTop(scrollTop);
}

int HTMLBodyElement::scrollHeight()
{
    if (isFirstBodyElementOfDocument()) {
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
    return HTMLElement::scrollHeight();
}

int HTMLBodyElement::scrollWidth()
{
    if (isFirstBodyElementOfDocument()) {
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
    return HTMLElement::scrollWidth();
}

void HTMLBodyElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(fastGetAttribute(backgroundAttr)));
}

} // namespace WebCore

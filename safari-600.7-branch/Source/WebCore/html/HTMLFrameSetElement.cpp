/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLNames.h"
#include "Length.h"
#include "MouseEvent.h"
#include "RenderFrameSet.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFrameSetElement::HTMLFrameSetElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_totalRows(1)
    , m_totalCols(1)
    , m_border(6)
    , m_borderSet(false)
    , m_borderColorSet(false)
    , m_frameborder(true)
    , m_frameborderSet(false)
    , m_noresize(false)
{
    ASSERT(hasTagName(framesetTag));
    setHasCustomStyleResolveCallbacks();
}

PassRefPtr<HTMLFrameSetElement> HTMLFrameSetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLFrameSetElement(tagName, document));
}

bool HTMLFrameSetElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == bordercolorAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLFrameSetElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == bordercolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
    else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLFrameSetElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == rowsAttr) {
        if (!value.isNull()) {
            m_rowLengths = newLengthArray(value.string(), m_totalRows);
            setNeedsStyleRecalc();
        }
    } else if (name == colsAttr) {
        if (!value.isNull()) {
            m_colLengths = newLengthArray(value.string(), m_totalCols);
            setNeedsStyleRecalc();
        }
    } else if (name == frameborderAttr) {
        if (!value.isNull()) {
            if (equalIgnoringCase(value, "no") || equalIgnoringCase(value, "0")) {
                m_frameborder = false;
                m_frameborderSet = true;
            } else if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, "1")) {
                m_frameborderSet = true;
            }
        } else {
            m_frameborder = false;
            m_frameborderSet = false;
        }
    } else if (name == noresizeAttr) {
        m_noresize = true;
    } else if (name == borderAttr) {
        if (!value.isNull()) {
            m_border = value.toInt();
            m_borderSet = true;
        } else
            m_borderSet = false;
    } else if (name == bordercolorAttr)
        m_borderColorSet = !value.isEmpty();
    else if (name == onloadAttr)
        document().setWindowAttributeEventListener(eventNames().loadEvent, name, value);
    else if (name == onbeforeunloadAttr)
        document().setWindowAttributeEventListener(eventNames().beforeunloadEvent, name, value);
    else if (name == onunloadAttr)
        document().setWindowAttributeEventListener(eventNames().unloadEvent, name, value);
    else if (name == onblurAttr)
        document().setWindowAttributeEventListener(eventNames().blurEvent, name, value);
    else if (name == onfocusAttr)
        document().setWindowAttributeEventListener(eventNames().focusEvent, name, value);
    else if (name == onfocusinAttr)
        document().setWindowAttributeEventListener(eventNames().focusinEvent, name, value);
    else if (name == onfocusoutAttr)
        document().setWindowAttributeEventListener(eventNames().focusoutEvent, name, value);
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
    else if (name == onstorageAttr)
        document().setWindowAttributeEventListener(eventNames().storageEvent, name, value);
    else if (name == ononlineAttr)
        document().setWindowAttributeEventListener(eventNames().onlineEvent, name, value);
    else if (name == onofflineAttr)
        document().setWindowAttributeEventListener(eventNames().offlineEvent, name, value);
    else if (name == onpopstateAttr)
        document().setWindowAttributeEventListener(eventNames().popstateEvent, name, value);
    else
        HTMLElement::parseAttribute(name, value);
}

bool HTMLFrameSetElement::rendererIsNeeded(const RenderStyle& style)
{
    // For compatibility, frames render even when display: none is set.
    // However, we delay creating a renderer until stylesheets have loaded. 
    return style.isStyleAvailable();
}

RenderPtr<RenderElement> HTMLFrameSetElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (style.get().hasContent())
        return RenderElement::createFor(*this, WTF::move(style));
    
    return createRenderer<RenderFrameSet>(*this, WTF::move(style));
}

HTMLFrameSetElement* HTMLFrameSetElement::findContaining(Element* descendant)
{
    return ancestorsOfType<HTMLFrameSetElement>(*descendant).first();
}

void HTMLFrameSetElement::willAttachRenderers()
{
    // Inherit default settings from parent frameset.
    // FIXME: This is not dynamic.
    const HTMLFrameSetElement* containingFrameSet = findContaining(this);
    if (!containingFrameSet)
        return;
    if (!m_frameborderSet)
        m_frameborder = containingFrameSet->hasFrameBorder();
    if (m_frameborder) {
        if (!m_borderSet)
            m_border = containingFrameSet->border();
        if (!m_borderColorSet)
            m_borderColorSet = containingFrameSet->hasBorderColor();
    }
    if (!m_noresize)
        m_noresize = containingFrameSet->noResize();
}

void HTMLFrameSetElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && !m_noresize && renderer() && renderer()->isFrameSet()) {
        if (toRenderFrameSet(renderer())->userResize(toMouseEvent(evt))) {
            evt->setDefaultHandled();
            return;
        }
    }
    HTMLElement::defaultEventHandler(evt);
}

bool HTMLFrameSetElement::willRecalcStyle(Style::Change)
{
    if (needsStyleRecalc() && renderer()) {
        renderer()->setNeedsLayout();
        clearNeedsStyleRecalc();
    }
    return true;
}

Node::InsertionNotificationRequest HTMLFrameSetElement::insertedInto(ContainerNode& insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint.inDocument()) {
        if (Frame* frame = document().frame())
            frame->loader().client().dispatchDidBecomeFrameset(document().isFrameSet());
    }

    return InsertionDone;
}

void HTMLFrameSetElement::removedFrom(ContainerNode& insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (insertionPoint.inDocument()) {
        if (Frame* frame = document().frame())
            frame->loader().client().dispatchDidBecomeFrameset(document().isFrameSet());
    }
}

} // namespace WebCore

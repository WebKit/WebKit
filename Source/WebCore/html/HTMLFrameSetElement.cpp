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

#include "CSSPropertyNames.h"
#include "Document.h"
#include "ElementIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLBodyElement.h"
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

Ref<HTMLFrameSetElement> HTMLFrameSetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLFrameSetElement(tagName, document));
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
        // FIXME: What is the right thing to do when removing this attribute?
        // Why not treat it the same way we treat setting it to the empty string?
        if (!value.isNull()) {
            m_rowLengths = newLengthArray(value.string(), m_totalRows);
            // FIXME: Would be nice to optimize the case where m_rowLengths did not change.
            setNeedsStyleRecalc();
        }
        return;
    }

    if (name == colsAttr) {
        // FIXME: What is the right thing to do when removing this attribute?
        // Why not treat it the same way we treat setting it to the empty string?
        if (!value.isNull()) {
            m_colLengths = newLengthArray(value.string(), m_totalCols);
            // FIXME: Would be nice to optimize the case where m_colLengths did not change.
            setNeedsStyleRecalc();
        }
        return;
    }

    if (name == frameborderAttr) {
        if (!value.isNull()) {
            if (equalLettersIgnoringASCIICase(value, "no") || value == "0") {
                m_frameborder = false;
                m_frameborderSet = true;
            } else if (equalLettersIgnoringASCIICase(value, "yes") || value == "1") {
                m_frameborderSet = true;
            }
        } else {
            m_frameborder = false;
            m_frameborderSet = false;
        }
        // FIXME: Do we need to trigger repainting?
        return;
    }

    if (name == noresizeAttr) {
        // FIXME: This should set m_noresize to false if the value is null.
        m_noresize = true;
        return;
    }

    if (name == borderAttr) {
        if (!value.isNull()) {
            m_border = value.toInt();
            m_borderSet = true;
        } else
            m_borderSet = false;
        // FIXME: Do we need to trigger repainting?
        return;
    }

    if (name == bordercolorAttr) {
        m_borderColorSet = !value.isEmpty();
        // FIXME: Clearly wrong: This can overwrite the value inherited from the parent frameset.
        // FIXME: Do we need to trigger repainting?
        return;
    }

    auto& eventName = HTMLBodyElement::eventNameForWindowEventHandlerAttribute(name);
    if (!eventName.isNull()) {
        document().setWindowAttributeEventListener(eventName, name, value);
        return;
    }

    HTMLElement::parseAttribute(name, value);
}

bool HTMLFrameSetElement::rendererIsNeeded(const RenderStyle& style)
{
    // For compatibility, frames render even when display: none is set.
    // However, we delay creating a renderer until stylesheets have loaded. 
    return !style.isPlaceholderStyle();
}

RenderPtr<RenderElement> HTMLFrameSetElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (style.hasContent())
        return RenderElement::createFor(*this, WTFMove(style));
    
    return createRenderer<RenderFrameSet>(*this, WTFMove(style));
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

void HTMLFrameSetElement::defaultEventHandler(Event& event)
{
    if (is<MouseEvent>(event) && !m_noresize && is<RenderFrameSet>(renderer())) {
        if (downcast<RenderFrameSet>(*renderer()).userResize(downcast<MouseEvent>(event))) {
            event.setDefaultHandled();
            return;
        }
    }
    HTMLElement::defaultEventHandler(event);
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

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
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

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "NodeRenderingContext.h"
#include "RenderIFrame.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(const QualifiedName& tagName, Document* document)
    : HTMLFrameElementBase(tagName, document)
{
    ASSERT(hasTagName(iframeTag));
    setHasCustomCallbacks();
}

PassRefPtr<HTMLIFrameElement> HTMLIFrameElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLIFrameElement(tagName, document));
}

bool HTMLIFrameElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == alignAttr || name == frameborderAttr || name == seamlessAttr)
        return true;
    return HTMLFrameElementBase::isPresentationAttribute(name);
}

void HTMLIFrameElement::collectStyleForAttribute(const Attribute& attribute, StylePropertySet* style)
{
    if (attribute.name() == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, attribute.value());
    else if (attribute.name() == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, attribute.value());
    else if (attribute.name() == alignAttr)
        applyAlignmentAttributeToStyle(attribute, style);
    else if (attribute.name() == frameborderAttr) {
        // Frame border doesn't really match the HTML4 spec definition for iframes. It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!attribute.isNull() && !attribute.value().toInt()) {
            // Add a rule that nulls out our border width.
            addPropertyToAttributeStyle(style, CSSPropertyBorderWidth, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else
        HTMLFrameElementBase::collectStyleForAttribute(attribute, style);
}

void HTMLIFrameElement::parseAttribute(const Attribute& attribute)
{
    if (attribute.name() == nameAttr) {
        const AtomicString& newName = attribute.value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeExtraNamedItem(m_name);
            document->addExtraNamedItem(newName);
        }
        m_name = newName;
    } else if (attribute.name() == sandboxAttr)
        setSandboxFlags(attribute.isNull() ? SandboxNone : SecurityContext::parseSandboxPolicy(attribute.value()));
    else if (attribute.name() == seamlessAttr) {
        // If we're adding or removing the seamless attribute, we need to force the content document to recalculate its StyleResolver.
        if (contentDocument())
            contentDocument()->styleResolverChanged(DeferRecalcStyle);
    } else
        HTMLFrameElementBase::parseAttribute(attribute);
}

bool HTMLIFrameElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    return isURLAllowed() && context.style()->display() != NONE;
}

RenderObject* HTMLIFrameElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::insertedInto(Node* insertionPoint)
{
    InsertionNotificationRequest result = HTMLFrameElementBase::insertedInto(insertionPoint);
    if (insertionPoint->inDocument() && document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->addExtraNamedItem(m_name);
    return result;
}

void HTMLIFrameElement::removedFrom(Node* insertionPoint)
{
    HTMLFrameElementBase::removedFrom(insertionPoint);
    if (insertionPoint->inDocument() && document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->removeExtraNamedItem(m_name);
}

bool HTMLIFrameElement::shouldDisplaySeamlessly() const
{
    return contentDocument() && contentDocument()->mayDisplaySeamlessWithParent() && hasAttribute(seamlessAttr);
}

void HTMLIFrameElement::didRecalcStyle(StyleChange styleChange)
{
    if (!shouldDisplaySeamlessly())
        return;
    Document* childDocument = contentDocument();
    if (styleChange >= Inherit || childDocument->childNeedsStyleRecalc() || childDocument->needsStyleRecalc())
        contentDocument()->recalcStyle(styleChange);
}

#if ENABLE(MICRODATA)
String HTMLIFrameElement::itemValueText() const
{
    return getURLAttribute(srcAttr);
}

void HTMLIFrameElement::setItemValueText(const String& value, ExceptionCode&)
{
    setAttribute(srcAttr, value);
}
#endif

}

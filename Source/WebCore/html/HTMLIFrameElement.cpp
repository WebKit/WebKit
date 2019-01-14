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

#include "CSSPropertyNames.h"
#include "DOMTokenList.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "RenderIFrame.h"
#include "ScriptableDocumentParser.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLIFrameElement);

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameElementBase(tagName, document)
{
    ASSERT(hasTagName(iframeTag));
}

Ref<HTMLIFrameElement> HTMLIFrameElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLIFrameElement(tagName, document));
}

DOMTokenList& HTMLIFrameElement::sandbox()
{
    if (!m_sandbox)
        m_sandbox = std::make_unique<DOMTokenList>(*this, sandboxAttr, [](Document&, StringView token) {
            return SecurityContext::isSupportedSandboxPolicy(token);
        });
    return *m_sandbox;
}

bool HTMLIFrameElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == frameborderAttr)
        return true;
    return HTMLFrameElementBase::isPresentationAttribute(name);
}

void HTMLIFrameElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == frameborderAttr) {
        // Frame border doesn't really match the HTML4 spec definition for iframes. It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!value.toInt()) {
            // Add a rule that nulls out our border width.
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else
        HTMLFrameElementBase::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLIFrameElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == sandboxAttr) {
        if (m_sandbox)
            m_sandbox->associatedAttributeValueChanged(value);

        String invalidTokens;
        setSandboxFlags(value.isNull() ? SandboxNone : SecurityContext::parseSandboxPolicy(value, invalidTokens));
        if (!invalidTokens.isNull())
            document().addConsoleMessage(MessageSource::Other, MessageLevel::Error, "Error while parsing the 'sandbox' attribute: " + invalidTokens);
    } else if (name == allowAttr)
        m_allow = value;
    else
        HTMLFrameElementBase::parseAttribute(name, value);
}

bool HTMLIFrameElement::rendererIsNeeded(const RenderStyle& style)
{
    return isURLAllowed() && style.display() != DisplayType::None;
}

RenderPtr<RenderElement> HTMLIFrameElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderIFrame>(*this, WTFMove(style));
}

}

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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
#include "HTMLFrameElement.h"

#include "Frame.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "RenderFrame.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFrameElement);

using namespace HTMLNames;

inline HTMLFrameElement::HTMLFrameElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameElementBase(tagName, document)
{
    ASSERT(hasTagName(frameTag));
    setHasCustomStyleResolveCallbacks();
}

Ref<HTMLFrameElement> HTMLFrameElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLFrameElement(tagName, document));
}

bool HTMLFrameElement::rendererIsNeeded(const RenderStyle&)
{
    // For compatibility, frames render even when display: none is set.
    return canLoad();
}

RenderPtr<RenderElement> HTMLFrameElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderFrame>(*this, WTFMove(style));
}

bool HTMLFrameElement::noResize() const
{
    return hasAttributeWithoutSynchronization(noresizeAttr);
}

void HTMLFrameElement::didAttachRenderers()
{
    HTMLFrameElementBase::didAttachRenderers();
    const auto containingFrameSet = HTMLFrameSetElement::findContaining(this);
    if (!containingFrameSet)
        return;
    if (!m_frameBorderSet)
        m_frameBorder = containingFrameSet->hasFrameBorder();
}

void HTMLFrameElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == frameborderAttr) {
        m_frameBorder = value.toInt();
        m_frameBorderSet = !value.isNull();
        // FIXME: If we are already attached, this has no effect.
    } else if (name == noresizeAttr) {
        if (auto* renderer = this->renderer())
            renderer->updateFromElement();
    } else
        HTMLFrameElementBase::parseAttribute(name, value);
}

} // namespace WebCore

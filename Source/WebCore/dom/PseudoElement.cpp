/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PseudoElement.h"

#include "ContentData.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffectStack.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderQuote.h"
#include "RenderStyleInlines.h"
#include "StyleResolver.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PseudoElement);

const String& pseudoElementNodeName()
{
    static NeverDestroyed<String> name = "<pseudo>"_s;
    return name;
}

PseudoElement::PseudoElement(Element& host, PseudoId pseudoId)
    : Node(host.document(), 0, TypeFlag::HasCustomStyleResolveCallbacks)
    , m_hostElement(host)
    , m_pseudoId(pseudoId)
{
    setEventTargetFlag(EventTargetFlag::IsConnected);
    ASSERT(pseudoId == PseudoId::Before || pseudoId == PseudoId::After);
}

PseudoElement::~PseudoElement()
{
    ASSERT(!m_hostElement);
}

Ref<PseudoElement> PseudoElement::create(Element& host, PseudoId pseudoId)
{
    Ref pseudoElement = adoptRef(*new PseudoElement(host, pseudoId));

    InspectorInstrumentation::pseudoElementCreated(host.document().protectedPage().get(), pseudoElement.get());

    return pseudoElement;
}


void PseudoElement::clearHostElement()
{
    InspectorInstrumentation::pseudoElementDestroyed(document().protectedPage().get(), *this);

    Styleable::fromElement(*this).elementWasRemoved();

    m_hostElement = nullptr;
}

bool PseudoElement::rendererIsNeeded(const RenderStyle& style)
{
    if (pseudoElementRendererIsNeeded(&style))
        return true;

    if (RefPtr element = m_hostElement.get()) {
        if (auto* stack = element->keyframeEffectStack(Style::PseudoElementIdentifier { pseudoId() }))
            return stack->requiresPseudoElement();
    }
    return false;
}

void PseudoElement::storeDisplayContentsOrNoneStyle(std::unique_ptr<RenderStyle> style)
{
    // This is used by RenderTreeBuilder to store the style for Elements with display:{contents|none}.
    // Normally style is held in renderers but display:contents doesn't generate one.
    // This is kept distinct from ElementRareData::computedStyle() which can update outside style resolution.
    // This way renderOrDisplayContentsStyle() always returns consistent styles matching the rendering state.
    ASSERT(style && (style->display() == DisplayType::Contents || style->display() == DisplayType::None));
    m_displayContentsOrNoneStyle = WTFMove(style);
}

void PseudoElement::clearDisplayContentsOrNoneStyle()
{
    m_displayContentsOrNoneStyle = nullptr;
}

Ref<Node> PseudoElement::cloneNodeInternal(Document& targetDocument, CloningOperation type)
{
    switch (type) {
    case CloningOperation::OnlySelf:
    case CloningOperation::SelfWithTemplateContent: {
        Ref clone = cloneElementWithoutChildren(targetDocument);
        ScriptDisallowedScope::EventAllowedScope eventAllowedScope { clone };
        cloneShadowTreeIfPossible(clone);
        return clone;
    }
    case CloningOperation::Everything:
        break;
    }
    return cloneElementWithChildren(targetDocument);
}

} // namespace

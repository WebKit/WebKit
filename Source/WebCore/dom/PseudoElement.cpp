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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PseudoElement);

const QualifiedName& pseudoElementTagName()
{
    static NeverDestroyed<QualifiedName> name(nullAtom(), "<pseudo>"_s, nullAtom());
    return name;
}

PseudoElement::PseudoElement(Element& host, PseudoId pseudoId)
    : Element(pseudoElementTagName(), host.document(), TypeFlag::HasCustomStyleResolveCallbacks)
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
        if (auto* stack = element->keyframeEffectStack(pseudoId()))
            return stack->requiresPseudoElement();
    }
    return false;
}

} // namespace

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

#include "CSSAnimationController.h"
#include "ContentData.h"
#include "DocumentTimeline.h"
#include "InspectorInstrumentation.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderQuote.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PseudoElement);

const QualifiedName& pseudoElementTagName()
{
    static NeverDestroyed<QualifiedName> name(nullAtom(), "<pseudo>", nullAtom());
    return name;
}

String PseudoElement::pseudoElementNameForEvents(PseudoId pseudoId)
{
    static NeverDestroyed<const String> after(MAKE_STATIC_STRING_IMPL("::after"));
    static NeverDestroyed<const String> before(MAKE_STATIC_STRING_IMPL("::before"));
    switch (pseudoId) {
    case PseudoId::After:
        return after;
    case PseudoId::Before:
        return before;
    default:
        return emptyString();
    }
}

PseudoElement::PseudoElement(Element& host, PseudoId pseudoId)
    : Element(pseudoElementTagName(), host.document(), CreatePseudoElement)
    , m_hostElement(&host)
    , m_pseudoId(pseudoId)
{
    ASSERT(pseudoId == PseudoId::Before || pseudoId == PseudoId::After);
    setHasCustomStyleResolveCallbacks();
}

PseudoElement::~PseudoElement()
{
    ASSERT(!m_hostElement);
}

Ref<PseudoElement> PseudoElement::create(Element& host, PseudoId pseudoId)
{
    auto pseudoElement = adoptRef(*new PseudoElement(host, pseudoId));

    InspectorInstrumentation::pseudoElementCreated(host.document().page(), pseudoElement.get());

    return pseudoElement;
}

void PseudoElement::clearHostElement()
{
    InspectorInstrumentation::pseudoElementDestroyed(document().page(), *this);

    if (auto* timeline = document().existingTimeline())
        timeline->removeAnimationsForElement(*this);
    if (auto* frame = document().frame())
        frame->animation().cancelAnimations(*this);

    m_hostElement = nullptr;
}

bool PseudoElement::rendererIsNeeded(const RenderStyle& style)
{
    return pseudoElementRendererIsNeeded(&style);
}

} // namespace

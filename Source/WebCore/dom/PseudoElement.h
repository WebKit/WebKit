/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#include "Element.h"
#include <wtf/Forward.h>

namespace WebCore {

class PseudoElement final : public Element {
    WTF_MAKE_ISO_ALLOCATED(PseudoElement);
public:
    static Ref<PseudoElement> create(Element& host, PseudoId);
    virtual ~PseudoElement();

    Element* hostElement() const { return m_hostElement; }
    void clearHostElement();

    bool rendererIsNeeded(const RenderStyle&) override;
    bool isTargetedByKeyframeEffectRequiringPseudoElement();

    bool canStartSelection() const override { return false; }
    bool canContainRangeEndPoint() const override { return false; }

    static String pseudoElementNameForEvents(PseudoId);

private:
    PseudoElement(Element&, PseudoId);

    PseudoId customPseudoId() const override { return m_pseudoId; }

    Element* m_hostElement;
    PseudoId m_pseudoId;
};

const QualifiedName& pseudoElementTagName();

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PseudoElement)
    static bool isType(const WebCore::Node& node) { return node.isPseudoElement(); }
SPECIALIZE_TYPE_TRAITS_END()

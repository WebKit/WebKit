/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGFilterGraph.h"

namespace WebCore {

class FilterEffect;

class SVGFilterEffectGraph final : public SVGFilterGraph<FilterEffect> {
public:
    SVGFilterEffectGraph(Ref<FilterEffect>&& sourceGraphic, Ref<FilterEffect>&& sourceAlpha)
    {
        m_sourceNodes.add(SourceGraphic::effectName(), WTFMove(sourceGraphic));
        m_sourceNodes.add(SourceAlpha::effectName(), WTFMove(sourceAlpha));

        setNodeInputs(Ref { *this->sourceGraphic() }, NodeVector { });
        setNodeInputs(Ref { *this->sourceAlpha() }, NodeVector { *this->sourceGraphic() });
    }

    void addNamedNode(const AtomString& name, Ref<FilterEffect>&& node) override
    {
        if (name.isEmpty()) {
            m_lastNode = WTFMove(node);
            return;
        }

        if (m_sourceNodes.contains(name))
            return;

        m_lastNode = WTFMove(node);
        m_namedNodes.set(name, Ref { *m_lastNode });
    }

private:
    FilterEffect* sourceGraphic() const
    {
        return m_sourceNodes.get(FilterEffect::sourceGraphicName());
    }

    FilterEffect* sourceAlpha() const
    {
        return m_sourceNodes.get(FilterEffect::sourceAlphaName());
    }

    RefPtr<FilterEffect> getNamedNode(const AtomString& name) const override
    {
        if (!name.isEmpty()) {
            if (RefPtr sourceNode = m_sourceNodes.get(name))
                return sourceNode;

            if (RefPtr namedNode = m_namedNodes.get(name))
                return namedNode;
        }

        if (m_lastNode)
            return m_lastNode;

        // Fallback to the 'sourceGraphic' input.
        return sourceGraphic();
    }

    HashMap<AtomString, Ref<FilterEffect>> m_sourceNodes;
};

} // namespace WebCore

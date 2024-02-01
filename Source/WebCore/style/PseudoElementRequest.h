/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PseudoElementIdentifier.h"
#include "RenderStyleConstants.h"
#include "StyleScrollbarState.h"
#include <wtf/text/AtomString.h>

namespace WebCore::Style {

class PseudoElementRequest {
public:
    PseudoElementRequest(PseudoId pseudoId, std::optional<StyleScrollbarState> scrollbarState = std::nullopt)
        : m_identifier({ pseudoId })
        , m_scrollbarState(scrollbarState)
    {
    }

    PseudoElementRequest(PseudoId pseudoId, const AtomString& nameArgument)
        : m_identifier({ pseudoId, nameArgument })
    {
        ASSERT(pseudoId == PseudoId::Highlight || pseudoId == PseudoId::ViewTransitionGroup || pseudoId == PseudoId::ViewTransitionImagePair || pseudoId == PseudoId::ViewTransitionOld || pseudoId == PseudoId::ViewTransitionNew);
    }

    PseudoElementRequest(const PseudoElementIdentifier& pseudoElementIdentifier)
        : m_identifier(pseudoElementIdentifier)
    {
    }

    const PseudoElementIdentifier& identifier() const { return m_identifier; }
    PseudoId pseudoId() const { return m_identifier.pseudoId; }
    const AtomString& nameArgument() const { return m_identifier.nameArgument; }
    const std::optional<StyleScrollbarState>& scrollbarState() const { return m_scrollbarState; }

private:
    PseudoElementIdentifier m_identifier;
    std::optional<StyleScrollbarState> m_scrollbarState;
};

} // namespace WebCore

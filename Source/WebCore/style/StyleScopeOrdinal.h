/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {
namespace Style {

// This is used to identify style scopes that can affect an element.
// Scopes are in tree-of-trees order. Styles from earlier scopes win over later ones (modulo !important).
enum class ScopeOrdinal : int {
    ContainingHost = -1, // ::part rules and author-exposed UA pseudo classes from the host tree scope.
    Element = 0, // Normal rules in the same tree where the element is.
    FirstSlot = 1, // ::slotted rules in the parent's shadow tree. Values greater than FirstSlot indicate subsequent slots in the chain.
    Shadow = std::numeric_limits<int>::max(), // :host rules in element's own shadow tree.
};

inline ScopeOrdinal& operator++(ScopeOrdinal& ordinal)
{
    ASSERT(ordinal < ScopeOrdinal::Shadow);
    return ordinal = static_cast<ScopeOrdinal>(static_cast<int>(ordinal) + 1);
}

}
}

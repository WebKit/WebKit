/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleChange.h"

#include "RenderStyle.h"

namespace WebCore {
namespace Style {

Change determineChange(const RenderStyle& s1, const RenderStyle& s2)
{
    if (s1.display() != s2.display())
        return Detach;
    if (s1.hasPseudoStyle(PseudoId::FirstLetter) != s2.hasPseudoStyle(PseudoId::FirstLetter))
        return Detach;
    // We just detach if a renderer acquires or loses a column-span, since spanning elements
    // typically won't contain much content.
    if (s1.columnSpan() != s2.columnSpan())
        return Detach;
    if (!s1.contentDataEquivalent(&s2))
        return Detach;
    // When text-combine property has been changed, we need to prepare a separate renderer object.
    // When text-combine is on, we use RenderCombineText, otherwise RenderText.
    // https://bugs.webkit.org/show_bug.cgi?id=55069
    if (s1.hasTextCombine() != s2.hasTextCombine())
        return Detach;

    if (!s1.inheritedEqual(s2))
        return Inherit;

    if (!s1.descendantAffectingNonInheritedPropertiesEqual(s2))
        return Inherit;

    if (s1 != s2)
        return NoInherit;

    // If the pseudoStyles have changed, we want any StyleChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (s1.hasAnyPublicPseudoStyles()) {
        for (PseudoId pseudoId = PseudoId::FirstPublicPseudoId; pseudoId < PseudoId::FirstInternalPseudoId; pseudoId = static_cast<PseudoId>(static_cast<unsigned>(pseudoId) + 1)) {
            if (s1.hasPseudoStyle(pseudoId)) {
                RenderStyle* ps2 = s2.getCachedPseudoStyle(pseudoId);
                if (!ps2)
                    return NoInherit;
                RenderStyle* ps1 = s1.getCachedPseudoStyle(pseudoId);
                if (!ps1 || *ps1 != *ps2)
                    return NoInherit;
            }
        }
    }

    return NoChange;
}

}
}

/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>

namespace WebCore {

class Element;
class RenderStyle;

namespace Style {

class Update;

struct Relation {
    enum Type {
        AffectedByEmpty,
        AffectedByPreviousSibling,
        DescendantsAffectedByPreviousSibling,
        // For AffectsNextSibling 'value' tells how many element siblings to mark starting with 'element'.
        AffectsNextSibling,
        ChildrenAffectedByForwardPositionalRules,
        DescendantsAffectedByForwardPositionalRules,
        ChildrenAffectedByBackwardPositionalRules,
        DescendantsAffectedByBackwardPositionalRules,
        ChildrenAffectedByFirstChildRules,
        ChildrenAffectedByPropertyBasedBackwardPositionalRules,
        ChildrenAffectedByLastChildRules,
        FirstChild,
        LastChild,
        NthChildIndex,
        Unique,
    };
    const Element* element;
    Type type;
    unsigned value;

    Relation(const Element& element, Type type, unsigned value = 1)
        : element(&element)
        , type(type)
        , value(value)
    { }
};

using Relations = Vector<Relation, 8>;

std::unique_ptr<Relations> commitRelationsToRenderStyle(RenderStyle&, const Element&, const Relations&);
void commitRelations(std::unique_ptr<Relations>, Update&);

}
}

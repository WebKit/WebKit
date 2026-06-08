/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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

#if ENABLE(MATHML)

#include "MathMLOperatorDictionary.h"
#include "MathMLTokenElement.h"
#include <array>

namespace WebCore {

class MathMLOperatorElement final : public MathMLTokenElement {
    WTF_MAKE_TZONE_ALLOCATED(MathMLOperatorElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MathMLOperatorElement);
public:
    static Ref<MathMLOperatorElement> create(const QualifiedName&, Document&);
    struct OperatorChar {
        // Exactly one of character or characters is valid, determined by
        // hasTwoCharacters. For multi-character operators (e.g. "&&", "!=",
        // "->") the two code units are stored in characters and used for
        // multi-character dictionary lookup.
        union {
            char32_t character;
            std::array<char16_t, 2> characters;
        };
        bool isVertical { true };
        bool hasTwoCharacters { false };
        OperatorChar()
            : character(0)
            {
            }
    };
    static OperatorChar parseOperatorChar(const String&);
    const OperatorChar& operatorChar() LIFETIME_BOUND;
    void setOperatorFormDirty();
    MathMLOperatorDictionary::Form form() { return dictionaryProperty().form; }
    bool hasProperty(MathMLOperatorDictionary::Flag);
    Length defaultLeadingSpace();
    Length defaultTrailingSpace();
    const Length& leadingSpace() LIFETIME_BOUND;
    const Length& trailingSpace() LIFETIME_BOUND;
    const Length& minSize() LIFETIME_BOUND;
    const Length& maxSize() LIFETIME_BOUND;

private:
    MathMLOperatorElement(const QualifiedName&, Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void childrenChanged(const ChildChange&) final;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    std::optional<OperatorChar> m_operatorChar;

    std::optional<MathMLOperatorDictionary::Property> m_dictionaryProperty;
    MathMLOperatorDictionary::Property computeDictionaryProperty();
    const MathMLOperatorDictionary::Property& dictionaryProperty() LIFETIME_BOUND;

    struct OperatorProperties {
        unsigned short flags;
        unsigned short dirtyFlags { MathMLOperatorDictionary::allFlags };
    };
    OperatorProperties m_properties;
    void computeOperatorFlag(MathMLOperatorDictionary::Flag);

    std::optional<Length> m_leadingSpace;
    std::optional<Length> m_trailingSpace;
    std::optional<Length> m_minSize;
    std::optional<Length> m_maxSize;
};

} // namespace WebCore

#endif // ENABLE(MATHML)

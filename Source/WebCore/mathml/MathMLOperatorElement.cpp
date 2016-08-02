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
 *
 */

#include "config.h"

#if ENABLE(MATHML)
#include "MathMLOperatorElement.h"

#include "RenderMathMLOperator.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace MathMLNames;
using namespace MathMLOperatorDictionary;

MathMLOperatorElement::MathMLOperatorElement(const QualifiedName& tagName, Document& document)
    : MathMLTextElement(tagName, document)
{
}

Ref<MathMLOperatorElement> MathMLOperatorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLOperatorElement(tagName, document));
}

UChar MathMLOperatorElement::parseOperatorText(const String& string)
{
    // We collapse the whitespace and replace the hyphens by minus signs.
    AtomicString textContent = string.stripWhiteSpace().simplifyWhiteSpace().replace(hyphenMinus, minusSign).impl();

    // We verify whether the operator text can be represented by a single UChar.
    // FIXME: This is a really inefficient way to extract a character from a string (https://webkit.org/b/160241#c7).
    // FIXME: This does not handle surrogate pairs (https://webkit.org/b/122296).
    // FIXME: This does not handle <mo> operators with multiple characters (https://webkit.org/b/124828).
    return textContent.length() == 1 ? textContent[0] : 0;
}

UChar MathMLOperatorElement::operatorText()
{
    if (!m_operatorText)
        m_operatorText = parseOperatorText(textContent());
    return m_operatorText.value();
}

MathMLOperatorElement::DictionaryProperty MathMLOperatorElement::computeDictionaryProperty()
{
    DictionaryProperty dictionaryProperty;

    // We first determine the form attribute and use the default spacing and properties.
    const auto& value = attributeWithoutSynchronization(formAttr);
    bool explicitForm = true;
    if (value == "prefix")
        dictionaryProperty.form = Prefix;
    else if (value == "infix")
        dictionaryProperty.form = Infix;
    else if (value == "postfix")
        dictionaryProperty.form = Postfix;
    else {
        // FIXME: We should use more advanced heuristics indicated in the specification to determine the operator form (https://bugs.webkit.org/show_bug.cgi?id=124829).
        explicitForm = false;
        if (!previousSibling() && nextSibling())
            dictionaryProperty.form = Prefix;
        else if (previousSibling() && !nextSibling())
            dictionaryProperty.form = Postfix;
        else
            dictionaryProperty.form = Infix;
    }

    // We then try and find an entry in the operator dictionary to override the default values.
    if (auto entry = search(operatorText(), dictionaryProperty.form, explicitForm)) {
        dictionaryProperty.form = static_cast<MathMLOperatorDictionary::Form>(entry.value().form);
        dictionaryProperty.leadingSpaceInMathUnit = entry.value().lspace;
        dictionaryProperty.trailingSpaceInMathUnit = entry.value().rspace;
        dictionaryProperty.flags = entry.value().flags;
    }

    return dictionaryProperty;
}

const MathMLOperatorElement::DictionaryProperty& MathMLOperatorElement::dictionaryProperty()
{
    if (!m_dictionaryProperty)
        m_dictionaryProperty = computeDictionaryProperty();
    return m_dictionaryProperty.value();
}

static const QualifiedName& propertyFlagToAttributeName(MathMLOperatorDictionary::Flag flag)
{
    switch (flag) {
    case Accent:
        return accentAttr;
    case Fence:
        return fenceAttr;
    case LargeOp:
        return largeopAttr;
    case MovableLimits:
        return movablelimitsAttr;
    case Separator:
        return separatorAttr;
    case Stretchy:
        return stretchyAttr;
    case Symmetric:
        return symmetricAttr;
    }
    ASSERT_NOT_REACHED();
}

void MathMLOperatorElement::computeOperatorFlag(MathMLOperatorDictionary::Flag flag)
{
    ASSERT(m_properties.dirtyFlags & flag);

    Optional<BooleanValue> property;
    const auto& name = propertyFlagToAttributeName(flag);
    const BooleanValue& value = cachedBooleanAttribute(name, property);
    switch (value) {
    case BooleanValue::True:
        m_properties.flags |= flag;
        break;
    case BooleanValue::False:
        m_properties.flags &= ~flag;
        break;
    case BooleanValue::Default:
        // By default, we use the value specified in the operator dictionary.
        if (dictionaryProperty().flags & flag)
            m_properties.flags |= flag;
        else
            m_properties.flags &= ~flag;
        break;
    }
}

bool MathMLOperatorElement::hasProperty(MathMLOperatorDictionary::Flag flag)
{
    if (m_properties.dirtyFlags & flag) {
        computeOperatorFlag(flag);
        m_properties.dirtyFlags &= ~flag;
    }
    return m_properties.flags & flag;
}

MathMLElement::Length MathMLOperatorElement::defaultLeadingSpace()
{
    Length space;
    space.type = LengthType::MathUnit;
    space.value = static_cast<float>(dictionaryProperty().leadingSpaceInMathUnit);
    return space;
}

MathMLElement::Length MathMLOperatorElement::defaultTrailingSpace()
{
    Length space;
    space.type = LengthType::MathUnit;
    space.value = static_cast<float>(dictionaryProperty().trailingSpaceInMathUnit);
    return space;
}

void MathMLOperatorElement::childrenChanged(const ChildChange& change)
{
    m_operatorText = Nullopt;
    m_dictionaryProperty = Nullopt;
    m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    MathMLTextElement::childrenChanged(change);
}

static Optional<MathMLOperatorDictionary::Flag> attributeNameToPropertyFlag(const QualifiedName& name)
{
    if (name == accentAttr)
        return Accent;
    if (name == fenceAttr)
        return Fence;
    if (name == largeopAttr)
        return LargeOp;
    if (name == movablelimitsAttr)
        return MovableLimits;
    if (name == separatorAttr)
        return Separator;
    if (name == stretchyAttr)
        return Stretchy;
    if (name == symmetricAttr)
        return Symmetric;
    return Nullopt;
}

void MathMLOperatorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == formAttr) {
        m_dictionaryProperty = Nullopt;
        m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    } else if (auto flag = attributeNameToPropertyFlag(name))
        m_properties.dirtyFlags |= flag.value();

    if ((name == stretchyAttr || name == lspaceAttr || name == rspaceAttr || name == movablelimitsAttr) && renderer()) {
        downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        return;
    }

    MathMLTextElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> MathMLOperatorElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(MathMLNames::moTag));
    return createRenderer<RenderMathMLOperator>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)

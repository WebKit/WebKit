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
#include "MathMLOperatorElement.h"

#if ENABLE(MATHML)

#include "ElementInlines.h"
#include "NodeName.h"
#include "RenderMathMLOperator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLOperatorElement);

using namespace MathMLNames;
using namespace MathMLOperatorDictionary;

MathMLOperatorElement::MathMLOperatorElement(const QualifiedName& tagName, Document& document)
    : MathMLTokenElement(tagName, document)
{
}

Ref<MathMLOperatorElement> MathMLOperatorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLOperatorElement(tagName, document));
}

MathMLOperatorElement::OperatorChar MathMLOperatorElement::parseOperatorChar(const String& string)
{
    OperatorChar operatorChar;
    // FIXME: This operator dictionary does not accept multiple characters (https://webkit.org/b/124828).
    if (auto codePoint = convertToSingleCodePoint(string)) {
        auto character = codePoint.value();
        // The minus sign renders better than the hyphen sign used in some MathML formulas.
        if (character == hyphenMinus)
            character = minusSign;
        operatorChar.character = character;
        operatorChar.isVertical = isVertical(operatorChar.character);
    }
    return operatorChar;
}

const MathMLOperatorElement::OperatorChar& MathMLOperatorElement::operatorChar()
{
    if (!m_operatorChar)
        m_operatorChar = parseOperatorChar(textContent());
    return m_operatorChar.value();
}

Property MathMLOperatorElement::computeDictionaryProperty()
{
    Property dictionaryProperty;

    // We first determine the form attribute and use the default spacing and properties.
    const auto& value = attributeWithoutSynchronization(formAttr);
    bool explicitForm = true;
    if (value == "prefix"_s)
        dictionaryProperty.form = Prefix;
    else if (value == "infix"_s)
        dictionaryProperty.form = Infix;
    else if (value == "postfix"_s)
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
    if (auto entry = search(operatorChar().character, dictionaryProperty.form, explicitForm))
        dictionaryProperty = entry.value();

    return dictionaryProperty;
}

const Property& MathMLOperatorElement::dictionaryProperty()
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
    return nullQName();
}

void MathMLOperatorElement::computeOperatorFlag(MathMLOperatorDictionary::Flag flag)
{
    ASSERT(m_properties.dirtyFlags & flag);

    std::optional<BooleanValue> property;
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

const MathMLElement::Length& MathMLOperatorElement::leadingSpace()
{
    return cachedMathMLLength(MathMLNames::lspaceAttr, m_leadingSpace);
}

const MathMLElement::Length& MathMLOperatorElement::trailingSpace()
{
    return cachedMathMLLength(MathMLNames::rspaceAttr, m_trailingSpace);
}

const MathMLElement::Length& MathMLOperatorElement::minSize()
{
    return cachedMathMLLength(MathMLNames::minsizeAttr, m_minSize);
}

const MathMLElement::Length& MathMLOperatorElement::maxSize()
{
    return cachedMathMLLength(MathMLNames::maxsizeAttr, m_maxSize);
}

void MathMLOperatorElement::childrenChanged(const ChildChange& change)
{
    m_operatorChar = std::nullopt;
    m_dictionaryProperty = std::nullopt;
    m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    MathMLTokenElement::childrenChanged(change);
}

void MathMLOperatorElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::formAttr:
        m_dictionaryProperty = std::nullopt;
        m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
        break;
    case AttributeNames::lspaceAttr:
        m_leadingSpace = std::nullopt;
        if (renderer())
            downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        break;
    case AttributeNames::rspaceAttr:
        m_trailingSpace = std::nullopt;
        if (renderer())
            downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        break;
    case AttributeNames::minsizeAttr:
        m_minSize = std::nullopt;
        break;
    case AttributeNames::maxsizeAttr:
        m_maxSize = std::nullopt;
        break;
    case AttributeNames::stretchyAttr:
        m_properties.dirtyFlags |= Stretchy;
        if (renderer())
            downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        break;
    case AttributeNames::movablelimitsAttr:
        m_properties.dirtyFlags |= MovableLimits;
        if (renderer())
            downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        break;
    case AttributeNames::accentAttr:
        m_properties.dirtyFlags |= Accent;
        break;
    case AttributeNames::fenceAttr:
        m_properties.dirtyFlags |= Fence;
        break;
    case AttributeNames::largeopAttr:
        m_properties.dirtyFlags |= LargeOp;
        break;
    case AttributeNames::separatorAttr:
        m_properties.dirtyFlags |= Separator;
        break;
    case AttributeNames::symmetricAttr:
        m_properties.dirtyFlags |= Symmetric;
        break;
    default:
        break;
    }

    MathMLTokenElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

RenderPtr<RenderElement> MathMLOperatorElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(MathMLNames::moTag));
    return createRenderer<RenderMathMLOperator>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)

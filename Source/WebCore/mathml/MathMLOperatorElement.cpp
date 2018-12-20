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
    if (m_maxSize)
        return m_maxSize.value();

    const AtomicString& value = attributeWithoutSynchronization(MathMLNames::maxsizeAttr);
    if (value == "infinity") {
        Length maxsize;
        maxsize.type = LengthType::Infinity;
        m_maxSize = maxsize;
    } else
        m_maxSize = parseMathMLLength(value);

    return m_maxSize.value();
}

void MathMLOperatorElement::childrenChanged(const ChildChange& change)
{
    m_operatorChar = WTF::nullopt;
    m_dictionaryProperty = WTF::nullopt;
    m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    MathMLTokenElement::childrenChanged(change);
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
    return WTF::nullopt;
}

void MathMLOperatorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == formAttr) {
        m_dictionaryProperty = WTF::nullopt;
        m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    } else if (auto flag = attributeNameToPropertyFlag(name))
        m_properties.dirtyFlags |= flag.value();
    else if (name == lspaceAttr)
        m_leadingSpace = WTF::nullopt;
    else if (name == rspaceAttr)
        m_trailingSpace = WTF::nullopt;
    else if (name == minsizeAttr)
        m_minSize = WTF::nullopt;
    else if (name == maxsizeAttr)
        m_maxSize = WTF::nullopt;

    if ((name == stretchyAttr || name == lspaceAttr || name == rspaceAttr || name == movablelimitsAttr) && renderer()) {
        downcast<RenderMathMLOperator>(*renderer()).updateFromElement();
        return;
    }

    MathMLTokenElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> MathMLOperatorElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(MathMLNames::moTag));
    return createRenderer<RenderMathMLOperator>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)

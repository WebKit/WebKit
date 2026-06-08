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

#include "ContainerNodeInlines.h"
#include "ElementInlines.h"
#include "NodeName.h"
#include "RenderMathMLOperator.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MathMLOperatorElement);

using namespace MathMLNames;
using namespace MathMLOperatorDictionary;

MathMLOperatorElement::MathMLOperatorElement(const QualifiedName& tagName, Document& document)
    : MathMLTokenElement(tagName, document)
{
    ASSERT(hasTagName(MathMLNames::moTag));
}

Ref<MathMLOperatorElement> MathMLOperatorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLOperatorElement(tagName, document));
}

MathMLOperatorElement::OperatorChar MathMLOperatorElement::parseOperatorChar(const String& string)
{
    OperatorChar operatorChar;
    String trimmed = string.trim(isASCIIWhitespaceWithoutFF<UChar>);
    if (trimmed.isEmpty())
        return operatorChar;

    auto setOperatorChar = [&](char32_t character) {
        operatorChar.character = character;
        operatorChar.isVertical = isVertical(operatorChar.character);
    };

    // https://w3c.github.io/mathml-core/#dfn-algorithm-to-determine-the-category-of-an-operator
    // Only operators with UTF-16 length 1 or 2 are recognized.
    unsigned length = trimmed.length();
    if (length == 1) {
        char32_t character = trimmed[0];
        // The minus sign renders better than the hyphen sign used in some MathML formulas.
        // This is not in MathML Core, see https://github.com/w3c/mathml-core/issues/70
        if (character == hyphenMinus)
            character = minusSign;
        setOperatorChar(character);
        return operatorChar;
    }

    if (length == 2) {
        // A surrogate pair encoding a single code point (e.g. U+1EEF0, U+1EEF1).
        if (auto codePoint = StringView(trimmed).convertToSingleCodePoint()) {
            setOperatorChar(codePoint.value());
            return operatorChar;
        }
        // Base character followed by U+0338 COMBINING LONG SOLIDUS OVERLAY
        // or U+20D2 COMBINING LONG VERTICAL LINE OVERLAY. The base character
        // is used for dictionary lookup and must not be substituted.
        constexpr UChar combiningLongSolidusOverlay = 0x0338;
        constexpr UChar combiningLongVerticalLineOverlay = 0x20D2;
        UChar second = trimmed[1];
        if (second == combiningLongSolidusOverlay || second == combiningLongVerticalLineOverlay) {
            setOperatorChar(trimmed[0]);
            return operatorChar;
        }
        // Otherwise, a multi-character operator such as "&&", "!=" or "->".
        // Store both code units for a second-pass lookup in the multi-character
        // operator dictionary.
        operatorChar.hasTwoCharacters = true;
        operatorChar.characters = { trimmed[0], trimmed[1] };
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
    const auto& value = attributeWithoutSynchronization(MathMLNames::formAttr);
    bool explicitForm = true;
    if (value == "prefix"_s)
        dictionaryProperty.form = Form::Prefix;
    else if (value == "infix"_s)
        dictionaryProperty.form = Form::Infix;
    else if (value == "postfix"_s)
        dictionaryProperty.form = Form::Postfix;
    else {
        // FIXME: We should use more advanced heuristics indicated in the specification to determine the operator form (https://bugs.webkit.org/show_bug.cgi?id=124829).
        explicitForm = false;
        if (!previousSibling() && nextSibling())
            dictionaryProperty.form = Form::Prefix;
        else if (previousSibling() && !nextSibling())
            dictionaryProperty.form = Form::Postfix;
        else
            dictionaryProperty.form = Form::Infix;
    }

    // We then try and find an entry in the operator dictionary to override the default values.
    if (!operatorChar().hasTwoCharacters) {
        if (auto entry = search(operatorChar().character, dictionaryProperty.form, explicitForm))
            dictionaryProperty = entry.value();
    } else if (auto entry = search(operatorChar().characters, dictionaryProperty.form, explicitForm))
        dictionaryProperty = entry.value();

    return dictionaryProperty;
}

const Property& MathMLOperatorElement::dictionaryProperty()
{
    if (!m_dictionaryProperty)
        m_dictionaryProperty = computeDictionaryProperty();
    return m_dictionaryProperty.value();
}

static const QualifiedName& NODELETE propertyFlagToAttributeName(MathMLOperatorDictionary::Flag flag)
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

void MathMLOperatorElement::setOperatorFormDirty()
{
    // Nothing to invalidate if the dictionary entry has never been computed; the
    // renderer will read the up-to-date value the first time it asks for it.
    if (!m_dictionaryProperty)
        return;

    // Invalidate the cached dictionary entry and ensure the renderer re-reads it so
    // that spacing changes from a sibling-triggered form switch take effect at layout.
    m_dictionaryProperty = std::nullopt;
    m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
    if (CheckedPtr renderOperator = dynamicDowncast<RenderMathMLOperator>(renderer())) {
        renderOperator->updateFromElement();
        renderOperator->setNeedsLayoutAndInvalidateContentLogicalWidths();
    }
}

void MathMLOperatorElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    bool affectsLayout = false;
    switch (name.nodeName()) {
    case AttributeNames::formAttr:
        m_dictionaryProperty = std::nullopt;
        m_properties.dirtyFlags = MathMLOperatorDictionary::allFlags;
        affectsLayout = true;
        break;
    case AttributeNames::lspaceAttr:
        m_leadingSpace = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::rspaceAttr:
        m_trailingSpace = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::minsizeAttr:
        m_minSize = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::maxsizeAttr:
        m_maxSize = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::stretchyAttr:
        m_properties.dirtyFlags |= Stretchy;
        affectsLayout = true;
        break;
    case AttributeNames::movablelimitsAttr:
        m_properties.dirtyFlags |= MovableLimits;
        affectsLayout = true;
        break;
    case AttributeNames::accentAttr:
        m_properties.dirtyFlags |= Accent;
        affectsLayout = true;
        break;
    case AttributeNames::fenceAttr:
        m_properties.dirtyFlags |= Fence;
        break;
    case AttributeNames::largeopAttr:
        m_properties.dirtyFlags |= LargeOp;
        affectsLayout = true;
        break;
    case AttributeNames::separatorAttr:
        m_properties.dirtyFlags |= Separator;
        break;
    case AttributeNames::symmetricAttr:
        m_properties.dirtyFlags |= Symmetric;
        affectsLayout = true;
        break;
    default:
        break;
    }

    if (affectsLayout) {
        if (CheckedPtr renderOperator = dynamicDowncast<RenderMathMLOperator>(renderer())) {
            renderOperator->updateFromElement();
            renderOperator->setNeedsLayoutAndInvalidateContentLogicalWidths();
        }
    }

    MathMLTokenElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

RenderPtr<RenderElement> MathMLOperatorElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMathMLOperator>(RenderObject::Type::MathMLOperator, *this, WTF::move(style));
}

} // namespace WebCore

#endif // ENABLE(MATHML)

/*
 * Copyright (C) 2007, 2008, 2009, 2013 Apple Inc.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "EditingStyle.h"

#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "ColorSerialization.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "Frame.h"
#include "HTMLFontElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "MutableStyleProperties.h"
#include "Node.h"
#include "NodeTraversal.h"
#include "QualifiedName.h"
#include "Range.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SimpleRange.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyledElement.h"
#include "VisibleUnits.h"

namespace WebCore {

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be copied to the new paragraph.
static constexpr CSSPropertyID editingProperties[] = {
    CSSPropertyCaretColor,
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariantCaps,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
#if ENABLE(TOUCH_EVENTS)
    CSSPropertyWebkitTapHighlightColor,
#endif
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
#if ENABLE(TEXT_AUTOSIZING)
    CSSPropertyWebkitTextSizeAdjust,
#endif
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,

    // Non-inheritable properties
    CSSPropertyBackgroundColor,
    CSSPropertyTextDecorationLine,
};

const unsigned numAllEditingProperties = std::size(editingProperties);
const unsigned numInheritableEditingProperties = numAllEditingProperties - 2;

enum EditingPropertiesToInclude { OnlyInheritableEditingProperties, AllEditingProperties };
template <class StyleDeclarationType>
static Ref<MutableStyleProperties> copyEditingProperties(StyleDeclarationType* style, EditingPropertiesToInclude type)
{
    if (type == AllEditingProperties)
        return style->copyProperties(editingProperties);
    return style->copyProperties({ editingProperties, numInheritableEditingProperties });
}

static inline bool isEditingProperty(int id)
{
    for (auto& editingProperty : editingProperties) {
        if (editingProperty == id)
            return true;
    }
    return false;
}

static Ref<MutableStyleProperties> copyPropertiesFromComputedStyle(ComputedStyleExtractor& computedStyle, EditingStyle::PropertiesToInclude propertiesToInclude)
{
    switch (propertiesToInclude) {
    case EditingStyle::OnlyEditingInheritableProperties:
        return copyEditingProperties(&computedStyle, OnlyInheritableEditingProperties);
    case EditingStyle::EditingPropertiesInEffect:
        return copyEditingProperties(&computedStyle, AllEditingProperties);
    case EditingStyle::AllProperties:
        break;
    }
    return computedStyle.copyProperties();
}

static Ref<MutableStyleProperties> copyPropertiesFromComputedStyle(Node* node, EditingStyle::PropertiesToInclude propertiesToInclude)
{
    ComputedStyleExtractor computedStyle(node);
    return copyPropertiesFromComputedStyle(computedStyle, propertiesToInclude);
}

static RefPtr<CSSValue> extractPropertyValue(const StyleProperties& style, CSSPropertyID propertyID)
{
    return style.getPropertyCSSValue(propertyID);
}

static RefPtr<CSSValue> extractPropertyValue(ComputedStyleExtractor& computedStyle, CSSPropertyID propertyID)
{
    return computedStyle.propertyValue(propertyID);
}

// This synthesizes CSSValueBold and CSSValueItalic when appropriate, and never returns CSSValueOblique.
template<typename T> CSSValueID identifierForStyleProperty(T& style, CSSPropertyID propertyID)
{
    auto value = extractPropertyValue(style, propertyID);
    if (auto fontStyleValue = dynamicDowncast<CSSFontStyleWithAngleValue>(value.get())) {
        ASSERT(propertyID == CSSPropertyFontStyle);
        return fontStyleValue->obliqueAngle().doubleValue(CSSUnitType::CSS_DEG) >= italicThreshold() ? CSSValueItalic : CSSValueNormal;
    }
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value.get())) {
        if (propertyID == CSSPropertyFontWeight && primitiveValue->doubleValue(CSSUnitType::CSS_NUMBER) >= boldThreshold())
            return CSSValueBold;
        auto identifier = primitiveValue->valueID();
        if (identifier == CSSValueOblique) {
            ASSERT(propertyID == CSSPropertyFontStyle);
            return CSSValueItalic;
        }
        return identifier;
    }
    return CSSValueInvalid;
}

template<typename T> Ref<MutableStyleProperties> getPropertiesNotIn(StyleProperties& styleWithRedundantProperties, T& baseStyle);
enum LegacyFontSizeMode { AlwaysUseLegacyFontSize, UseLegacyFontSizeOnlyIfPixelValuesMatch };
static int legacyFontSizeFromCSSValue(Document&, CSSPrimitiveValue*, bool shouldUseFixedFontDefaultSize, LegacyFontSizeMode);
static bool hasTransparentBackgroundColor(StyleProperties*);
static RefPtr<CSSValue> backgroundColorInEffect(Node*);

class HTMLElementEquivalent {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HTMLElementEquivalent(CSSPropertyID, CSSValueID primitiveValue, const QualifiedName& tagName);
    virtual ~HTMLElementEquivalent() = default;

    virtual bool matches(const Element& element) const { return !m_tagName || element.hasTagName(*m_tagName); }
    virtual bool hasAttribute() const { return false; }
    virtual bool propertyExistsInStyle(const EditingStyle& style) const { return style.m_mutableStyle && style.m_mutableStyle->getPropertyCSSValue(m_propertyID); }
    virtual bool valueIsPresentInStyle(Element&, const EditingStyle&) const;
    virtual void addToStyle(Element*, EditingStyle*) const;

protected:
    HTMLElementEquivalent(CSSPropertyID);
    HTMLElementEquivalent(CSSPropertyID, const QualifiedName& tagName);
    const CSSPropertyID m_propertyID;
    const RefPtr<CSSPrimitiveValue> m_primitiveValue;
    const QualifiedName* m_tagName { nullptr }; // We can store a pointer because HTML tag names are const global.
};

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id)
    : m_propertyID(id)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, const QualifiedName& tagName)
    : m_propertyID(id)
    , m_tagName(&tagName)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, CSSValueID primitiveValue, const QualifiedName& tagName)
    : m_propertyID(id)
    , m_primitiveValue(CSSPrimitiveValue::create(primitiveValue))
    , m_tagName(&tagName)
{
    ASSERT(primitiveValue != CSSValueInvalid);
}

bool HTMLElementEquivalent::valueIsPresentInStyle(Element& element, const EditingStyle& style) const
{
    RefPtr<CSSValue> value = style.m_mutableStyle->getPropertyCSSValue(m_propertyID);
    return matches(element) && is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(*value).valueID() == m_primitiveValue->valueID();
}

void HTMLElementEquivalent::addToStyle(Element*, EditingStyle* style) const
{
    style->setProperty(m_propertyID, m_primitiveValue->cssText());
}

class HTMLTextDecorationEquivalent : public HTMLElementEquivalent {
public:
    HTMLTextDecorationEquivalent(CSSValueID primitiveValue, const QualifiedName& tagName)
        : HTMLElementEquivalent(CSSPropertyTextDecorationLine, primitiveValue, tagName)
        , m_isUnderline(primitiveValue == CSSValueUnderline)
    {
    }

    bool propertyExistsInStyle(const EditingStyle& style) const override
    {
        if (changeInStyle(style) != TextDecorationChange::None)
            return true;

        if (!style.m_mutableStyle)
            return false;

        auto& mutableStyle = *style.m_mutableStyle;
        return mutableStyle.getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect)
            || mutableStyle.getPropertyCSSValue(CSSPropertyTextDecorationLine);
    }

    bool valueIsPresentInStyle(Element& element, const EditingStyle& style) const override
    {
        if (!matches(element))
            return false;
        auto change = changeInStyle(style);
        if (change != TextDecorationChange::None)
            return change == TextDecorationChange::Add;
        auto styleValue = style.m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
        if (!styleValue)
            styleValue = style.m_mutableStyle->getPropertyCSSValue(CSSPropertyTextDecorationLine);
        if (!m_primitiveValue)
            return false;
        return is<CSSValueList>(styleValue) && downcast<CSSValueList>(*styleValue).hasValue(*m_primitiveValue);
    }

private:
    TextDecorationChange changeInStyle(const EditingStyle& style) const
    {
        return m_isUnderline ? style.underlineChange() : style.strikeThroughChange();
    }

    bool m_isUnderline;
};

static bool fontWeightValueIsBold(CSSValue& fontWeight)
{
    if (!is<CSSPrimitiveValue>(fontWeight))
        return false;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(fontWeight);
    auto valueID = primitiveValue.valueID();

    if (isCSSWideKeyword(valueID))
        return false;

    switch (valueID) {
    case CSSValueNormal:
        return false;
    case CSSValueBold:
        return true;
    default:
        break;
    }

    if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
        return false;

    ASSERT(primitiveValue.isNumber());
    return primitiveValue.floatValue() >= static_cast<float>(boldThreshold());
}

class HTMLFontWeightEquivalent : public HTMLElementEquivalent {
public:
    HTMLFontWeightEquivalent(const QualifiedName& tagName)
        : HTMLElementEquivalent(CSSPropertyFontWeight, CSSValueBold, tagName)
    {
    }

private:
    bool valueIsPresentInStyle(Element& element, const EditingStyle& style) const
    {
        RefPtr<CSSValue> value = style.m_mutableStyle->getPropertyCSSValue(m_propertyID);
        return matches(element) && value && fontWeightValueIsBold(*value);
    }
};

class HTMLAttributeEquivalent : public HTMLElementEquivalent {
public:
    HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& tagName, const QualifiedName& attrName);
    HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& attrName);

    bool matches(const Element& element) const override { return HTMLElementEquivalent::matches(element) && element.hasAttribute(m_attrName); }
    bool hasAttribute() const override { return true; }
    bool valueIsPresentInStyle(Element&, const EditingStyle&) const override;
    void addToStyle(Element*, EditingStyle*) const override;
    virtual RefPtr<CSSValue> attributeValueAsCSSValue(Element*) const;
    inline const QualifiedName& attributeName() const { return m_attrName; }

protected:
    const QualifiedName& m_attrName; // We can store a reference because HTML attribute names are const global.
};

HTMLAttributeEquivalent::HTMLAttributeEquivalent(CSSPropertyID id, const QualifiedName& tagName, const QualifiedName& attrName)
    : HTMLElementEquivalent(id, tagName)
    , m_attrName(attrName)
{
}

HTMLAttributeEquivalent::HTMLAttributeEquivalent(CSSPropertyID id, const QualifiedName& attrName)
    : HTMLElementEquivalent(id)
    , m_attrName(attrName)
{
}

bool HTMLAttributeEquivalent::valueIsPresentInStyle(Element& element, const EditingStyle& style) const
{
    RefPtr<CSSValue> value = attributeValueAsCSSValue(&element);
    RefPtr<CSSValue> styleValue = style.m_mutableStyle->getPropertyCSSValue(m_propertyID);
    
    return compareCSSValuePtr(value, styleValue);
}

void HTMLAttributeEquivalent::addToStyle(Element* element, EditingStyle* style) const
{
    if (RefPtr<CSSValue> value = attributeValueAsCSSValue(element))
        style->setProperty(m_propertyID, value->cssText());
}

RefPtr<CSSValue> HTMLAttributeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    ASSERT(element);
    const AtomString& value = element->getAttribute(m_attrName);
    if (value.isNull())
        return nullptr;
    
    RefPtr<MutableStyleProperties> dummyStyle;
    dummyStyle = MutableStyleProperties::create();
    dummyStyle->setProperty(m_propertyID, value);
    return dummyStyle->getPropertyCSSValue(m_propertyID);
}

class HTMLFontSizeEquivalent : public HTMLAttributeEquivalent {
public:
    HTMLFontSizeEquivalent();

    RefPtr<CSSValue> attributeValueAsCSSValue(Element*) const override;
};

HTMLFontSizeEquivalent::HTMLFontSizeEquivalent()
    : HTMLAttributeEquivalent(CSSPropertyFontSize, HTMLNames::fontTag, HTMLNames::sizeAttr)
{
}

RefPtr<CSSValue> HTMLFontSizeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    ASSERT(element);
    const AtomString& value = element->getAttribute(m_attrName);
    if (value.isNull())
        return nullptr;
    CSSValueID size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return nullptr;
    return CSSPrimitiveValue::create(size);
}

float EditingStyle::NoFontDelta = 0.0f;

EditingStyle::EditingStyle()
    : m_shouldUseFixedDefaultFontSize(false)
    , m_underlineChange(static_cast<unsigned>(TextDecorationChange::None))
    , m_strikeThroughChange(static_cast<unsigned>(TextDecorationChange::None))
{
}

EditingStyle::EditingStyle(Node* node, PropertiesToInclude propertiesToInclude)
    : EditingStyle()
{
    init(node, propertiesToInclude);
}

EditingStyle::EditingStyle(const Position& position, PropertiesToInclude propertiesToInclude)
    : EditingStyle()
{
    init(position.deprecatedNode(), propertiesToInclude);
}

EditingStyle::EditingStyle(const CSSStyleDeclaration* style)
    : EditingStyle()
{
    if (style)
        m_mutableStyle = style->copyProperties();
    extractFontSizeDelta();
}

EditingStyle::EditingStyle(const StyleProperties* style)
    : EditingStyle()
{
    if (style)
        m_mutableStyle = style->mutableCopy();
    extractFontSizeDelta();
}

EditingStyle::EditingStyle(CSSPropertyID propertyID, const String& value)
    : EditingStyle()
{
    setProperty(propertyID, value);
    extractFontSizeDelta();
}

EditingStyle::EditingStyle(CSSPropertyID propertyID, CSSValueID value)
    : EditingStyle()
{
    m_mutableStyle = MutableStyleProperties::create();
    m_mutableStyle->setProperty(propertyID, value);
    extractFontSizeDelta();
}

EditingStyle::~EditingStyle() = default;

static Color cssValueToColor(CSSValue* colorValue)
{
    if (!is<CSSPrimitiveValue>(colorValue))
        return Color::transparentBlack;
    
    if (colorValue->isColor())
        return colorValue->color();
    
    return CSSParser::parseColorWithoutContext(colorValue->cssText());
}

template<typename T>
static inline Color textColorFromStyle(T& style)
{
    return cssValueToColor(extractPropertyValue(style, CSSPropertyColor).get());
}

template<typename T>
static inline Color caretColorFromStyle(T& style)
{
    return cssValueToColor(extractPropertyValue(style, CSSPropertyCaretColor).get());
}

template<typename T>
static inline Color backgroundColorFromStyle(T& style)
{
    return cssValueToColor(extractPropertyValue(style, CSSPropertyBackgroundColor).get());
}

static inline Color rgbaBackgroundColorInEffect(Node* node)
{
    return cssValueToColor(backgroundColorInEffect(node).get());
}

static int textAlignResolvingStartAndEnd(int textAlign, int direction)
{
    switch (textAlign) {
    case CSSValueCenter:
    case CSSValueWebkitCenter:
        return CSSValueCenter;
    case CSSValueJustify:
        return CSSValueJustify;
    case CSSValueLeft:
    case CSSValueWebkitLeft:
        return CSSValueLeft;
    case CSSValueRight:
    case CSSValueWebkitRight:
        return CSSValueRight;
    case CSSValueStart:
        return direction != CSSValueRtl ? CSSValueLeft : CSSValueRight;
    case CSSValueEnd:
        return direction == CSSValueRtl ? CSSValueRight : CSSValueLeft;
    }
    return CSSValueInvalid;
}

template<typename T>
static int textAlignResolvingStartAndEnd(T& style)
{
    return textAlignResolvingStartAndEnd(identifierForStyleProperty(style, CSSPropertyTextAlign), identifierForStyleProperty(style, CSSPropertyDirection));
}

void EditingStyle::init(Node* node, PropertiesToInclude propertiesToInclude)
{
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node)->parentNode();
    else if (isTabSpanNode(node))
        node = node->parentNode();

    ComputedStyleExtractor computedStyleAtPosition(node);
    // FIXME: It's strange to not set background-color and text-decoration when propertiesToInclude is EditingPropertiesInEffect.
    // However editing/selection/contains-boundaries.html fails without this ternary.
    m_mutableStyle = copyPropertiesFromComputedStyle(computedStyleAtPosition,
        propertiesToInclude == EditingPropertiesInEffect ? OnlyEditingInheritableProperties : propertiesToInclude);

    if (propertiesToInclude == EditingPropertiesInEffect) {
        if (RefPtr<CSSValue> value = backgroundColorInEffect(node))
            m_mutableStyle->setProperty(CSSPropertyBackgroundColor, value->cssText());
        if (RefPtr<CSSValue> value = computedStyleAtPosition.propertyValue(CSSPropertyWebkitTextDecorationsInEffect)) {
            m_mutableStyle->setProperty(CSSPropertyTextDecorationLine, value->cssText());
            m_mutableStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
        }
    }

    if (node && node->computedStyle()) {
        auto* renderStyle = node->computedStyle();
        removeTextFillAndStrokeColorsIfNeeded(renderStyle);
        if (renderStyle->fontDescription().keywordSize()) {
            if (auto cssValue = computedStyleAtPosition.getFontSizeCSSValuePreferringKeyword())
                m_mutableStyle->setProperty(CSSPropertyFontSize, cssValue->cssText());
        }
    }

    m_shouldUseFixedDefaultFontSize = computedStyleAtPosition.useFixedFontDefaultSize();
    extractFontSizeDelta();
}

void EditingStyle::removeTextFillAndStrokeColorsIfNeeded(const RenderStyle* renderStyle)
{
    if (RenderStyle::isCurrentColor(renderStyle->textFillColor()))
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextFillColor);
    if (RenderStyle::isCurrentColor(renderStyle->textStrokeColor()))
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextStrokeColor);
}

void EditingStyle::setProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    if (!m_mutableStyle)
        m_mutableStyle = MutableStyleProperties::create();

    m_mutableStyle->setProperty(propertyID, value, important);
}

void EditingStyle::extractFontSizeDelta()
{
    if (!m_mutableStyle)
        return;

    if (m_mutableStyle->getPropertyCSSValue(CSSPropertyFontSize)) {
        // Explicit font size overrides any delta.
        m_mutableStyle->removeProperty(CSSPropertyWebkitFontSizeDelta);
        return;
    }

    // Get the adjustment amount out of the style.
    RefPtr<CSSValue> value = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitFontSizeDelta);
    if (!is<CSSPrimitiveValue>(value))
        return;

    CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);

    // Only PX handled now. If we handle more types in the future, perhaps
    // a switch statement here would be more appropriate.
    if (!primitiveValue.isPx())
        return;

    m_fontSizeDelta = primitiveValue.floatValue();
    m_mutableStyle->removeProperty(CSSPropertyWebkitFontSizeDelta);
}

bool EditingStyle::isEmpty() const
{
    return (!m_mutableStyle || m_mutableStyle->isEmpty()) && m_fontSizeDelta == NoFontDelta
        && underlineChange() == TextDecorationChange::None && strikeThroughChange() == TextDecorationChange::None;
}

Ref<MutableStyleProperties> EditingStyle::styleWithResolvedTextDecorations() const
{
    bool hasTextDecorationChanges = underlineChange() != TextDecorationChange::None || strikeThroughChange() != TextDecorationChange::None;
    if (m_mutableStyle && !hasTextDecorationChanges)
        return *m_mutableStyle;

    Ref<MutableStyleProperties> style = m_mutableStyle ? m_mutableStyle->mutableCopy() : MutableStyleProperties::create();

    CSSValueListBuilder valueList;
    if (underlineChange() == TextDecorationChange::Add)
        valueList.append(CSSPrimitiveValue::create(CSSValueUnderline));
    if (strikeThroughChange() == TextDecorationChange::Add)
        valueList.append(CSSPrimitiveValue::create(CSSValueLineThrough));
    if (valueList.isEmpty())
        style->setProperty(CSSPropertyTextDecorationLine, CSSPrimitiveValue::create(CSSValueNone));
    else
        style->setProperty(CSSPropertyTextDecorationLine, CSSValueList::createSpaceSeparated(WTFMove(valueList)));

    return style;
}

std::optional<WritingDirection> EditingStyle::textDirection() const
{
    if (!m_mutableStyle)
        return std::nullopt;

    RefPtr<CSSValue> unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    if (!is<CSSPrimitiveValue>(unicodeBidi))
        return std::nullopt;

    CSSValueID unicodeBidiValue = downcast<CSSPrimitiveValue>(*unicodeBidi).valueID();
    if (unicodeBidiValue == CSSValueEmbed) {
        RefPtr<CSSValue> direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
        if (!is<CSSPrimitiveValue>(direction))
            return std::nullopt;

        return downcast<CSSPrimitiveValue>(*direction).valueID() == CSSValueLtr ? WritingDirection::LeftToRight : WritingDirection::RightToLeft;
    }

    if (unicodeBidiValue == CSSValueNormal)
        return WritingDirection::Natural;

    return std::nullopt;
}

void EditingStyle::setStyle(RefPtr<MutableStyleProperties>&& style)
{
    m_mutableStyle = WTFMove(style);
    // FIXME: We should be able to figure out whether or not font is fixed width for mutable style.
    // We need to check font-family is monospace as in FontDescription but we don't want to duplicate code here.
    m_shouldUseFixedDefaultFontSize = false;
    extractFontSizeDelta();
}

void EditingStyle::overrideWithStyle(const StyleProperties& style)
{
    return mergeStyle(&style, OverrideValues);
}

static void applyTextDecorationChangeToValueList(CSSValueList& valueList, TextDecorationChange change, Ref<CSSPrimitiveValue>&& value)
{
    switch (change) {
    case TextDecorationChange::None:
        break;
    case TextDecorationChange::Add:
        valueList.append(WTFMove(value));
        break;
    case TextDecorationChange::Remove:
        valueList.removeAll(value);
        break;
    }
}

void EditingStyle::overrideTypingStyleAt(const EditingStyle& style, const Position& position)
{
    mergeStyle(style.m_mutableStyle.get(), OverrideValues);

    m_fontSizeDelta += style.m_fontSizeDelta;

    prepareToApplyAt(position, EditingStyle::PreserveWritingDirection);

    auto underlineChange = style.underlineChange();
    auto strikeThroughChange = style.strikeThroughChange();
    if (underlineChange == TextDecorationChange::None && strikeThroughChange == TextDecorationChange::None)
        return;

    if (!m_mutableStyle)
        m_mutableStyle = MutableStyleProperties::create();

    Ref<CSSPrimitiveValue> underline = CSSPrimitiveValue::create(CSSValueUnderline);
    Ref<CSSPrimitiveValue> lineThrough = CSSPrimitiveValue::create(CSSValueLineThrough);
    RefPtr<CSSValue> value = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    RefPtr<CSSValueList> valueList;
    if (value && value->isValueList()) {
        valueList = downcast<CSSValueList>(*value).copy();
        applyTextDecorationChangeToValueList(*valueList, underlineChange, WTFMove(underline));
        applyTextDecorationChangeToValueList(*valueList, strikeThroughChange, WTFMove(lineThrough));
    } else {
        valueList = CSSValueList::createSpaceSeparated();
        if (underlineChange == TextDecorationChange::Add)
            valueList->append(WTFMove(underline));
        if (strikeThroughChange == TextDecorationChange::Add)
            valueList->append(WTFMove(lineThrough));
    }
    m_mutableStyle->setProperty(CSSPropertyWebkitTextDecorationsInEffect, valueList.get());
}

void EditingStyle::clear()
{
    m_mutableStyle = nullptr;
    m_shouldUseFixedDefaultFontSize = false;
    m_fontSizeDelta = NoFontDelta;
    setUnderlineChange(TextDecorationChange::None);
    setStrikeThroughChange(TextDecorationChange::None);
}

Ref<EditingStyle> EditingStyle::copy() const
{
    auto copy = EditingStyle::create();
    if (m_mutableStyle)
        copy->m_mutableStyle = m_mutableStyle->mutableCopy();
    copy->m_shouldUseFixedDefaultFontSize = m_shouldUseFixedDefaultFontSize;
    copy->m_underlineChange = m_underlineChange;
    copy->m_strikeThroughChange = m_strikeThroughChange;
    copy->m_fontSizeDelta = m_fontSizeDelta;
    return copy;
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static constexpr CSSPropertyID blockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyColumnCount,
    CSSPropertyColumnGap,
    CSSPropertyRowGap,
    CSSPropertyColumnRuleColor,
    CSSPropertyColumnRuleStyle,
    CSSPropertyColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextAlignLast,
    CSSPropertyTextJustify,
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

Ref<EditingStyle> EditingStyle::extractAndRemoveBlockProperties()
{
    auto result = EditingStyle::create();
    if (m_mutableStyle) {
        result->m_mutableStyle = m_mutableStyle->copyProperties(blockProperties);
        m_mutableStyle->removeProperties(blockProperties);
    }
    return result;
}

Ref<EditingStyle> EditingStyle::extractAndRemoveTextDirection()
{
    auto textDirection = EditingStyle::create();
    textDirection->m_mutableStyle = MutableStyleProperties::create();
    textDirection->m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed, m_mutableStyle->propertyIsImportant(CSSPropertyUnicodeBidi));
    textDirection->m_mutableStyle->setProperty(CSSPropertyDirection, m_mutableStyle->getPropertyValue(CSSPropertyDirection),
        m_mutableStyle->propertyIsImportant(CSSPropertyDirection));

    m_mutableStyle->removeProperty(CSSPropertyUnicodeBidi);
    m_mutableStyle->removeProperty(CSSPropertyDirection);

    return textDirection;
}

void EditingStyle::removeBlockProperties()
{
    if (!m_mutableStyle)
        return;

    m_mutableStyle->removeProperties(blockProperties);
}

void EditingStyle::removeStyleAddedByNode(Node* node)
{
    if (!node || !node->parentNode())
        return;
    auto parentStyle = copyPropertiesFromComputedStyle(node->parentNode(), EditingPropertiesInEffect);
    auto nodeStyle = copyPropertiesFromComputedStyle(node, EditingPropertiesInEffect);
    removeEquivalentProperties(parentStyle.get());
    removeEquivalentProperties(nodeStyle.get());
}

void EditingStyle::removeStyleConflictingWithStyleOfNode(Node& node)
{
    if (!node.parentNode() || !m_mutableStyle)
        return;

    auto parentStyle = copyPropertiesFromComputedStyle(node.parentNode(), EditingPropertiesInEffect);
    auto nodeStyle = EditingStyle::create(&node, EditingPropertiesInEffect);
    nodeStyle->removeEquivalentProperties(parentStyle.get());

    for (auto property : *nodeStyle->style())
        m_mutableStyle->removeProperty(property.id());
}

void EditingStyle::collapseTextDecorationProperties()
{
    if (!m_mutableStyle)
        return;

    RefPtr<CSSValue> textDecorationsInEffect = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (!textDecorationsInEffect)
        return;

    if (textDecorationsInEffect->isValueList())
        m_mutableStyle->setProperty(CSSPropertyTextDecorationLine, textDecorationsInEffect->cssText(), m_mutableStyle->propertyIsImportant(CSSPropertyTextDecorationLine));
    else
        m_mutableStyle->removeProperty(CSSPropertyTextDecorationLine);
    m_mutableStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
}

// CSS properties that create a visual difference only when applied to text.
static const CSSPropertyID textOnlyProperties[] = {
    CSSPropertyTextDecorationLine,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyFontStyle,
    CSSPropertyFontWeight,
    CSSPropertyColor,
};

TriState EditingStyle::triStateOfStyle(EditingStyle* style) const
{
    if (!style || !style->m_mutableStyle)
        return TriState::False;
    return triStateOfStyle(*style->m_mutableStyle, DoNotIgnoreTextOnlyProperties);
}

template<typename T>
TriState EditingStyle::triStateOfStyle(T& styleToCompare, ShouldIgnoreTextOnlyProperties shouldIgnoreTextOnlyProperties) const
{
    if (!m_mutableStyle)
        return TriState::True;

    auto difference = getPropertiesNotIn(*m_mutableStyle, styleToCompare);

    if (shouldIgnoreTextOnlyProperties == IgnoreTextOnlyProperties)
        difference->removeProperties(textOnlyProperties);

    if (difference->isEmpty())
        return TriState::True;
    if (difference->propertyCount() == m_mutableStyle->propertyCount())
        return TriState::False;

    return TriState::Indeterminate;
}

TriState EditingStyle::triStateOfStyle(const VisibleSelection& selection) const
{
    if (!selection.isCaretOrRange())
        return TriState::False;

    if (selection.isCaret())
        return triStateOfStyle(EditingStyle::styleAtSelectionStart(selection).get());

    TriState state = TriState::False;
    bool nodeIsStart = true;
    for (Node* node = selection.start().deprecatedNode(); node; node = NodeTraversal::next(*node)) {
        if (node->renderer() && node->hasEditableStyle()) {
            ComputedStyleExtractor computedStyle(node);
            TriState nodeState = triStateOfStyle(computedStyle, node->isTextNode() ? EditingStyle::DoNotIgnoreTextOnlyProperties : EditingStyle::IgnoreTextOnlyProperties);
            if (nodeIsStart) {
                state = nodeState;
                nodeIsStart = false;
            } else if (state != nodeState && node->isTextNode()) {
                state = TriState::Indeterminate;
                break;
            }
        }

        if (node == selection.end().deprecatedNode())
            break;
    }

    return state;
}

static RefPtr<CSSValueList> textDecorationValueList(const StyleProperties& properties)
{
    RefPtr<CSSValue> value = properties.getPropertyCSSValue(CSSPropertyTextDecorationLine);
    if (!is<CSSValueList>(value))
        return nullptr;
    return downcast<CSSValueList>(value.get());
}

bool EditingStyle::conflictsWithInlineStyleOfElement(StyledElement& element, RefPtr<MutableStyleProperties>* newInlineStylePtr, EditingStyle* extractedStyle) const
{
    const StyleProperties* inlineStyle = element.inlineStyle();
    if (!inlineStyle)
        return false;
    bool conflicts = false;
    RefPtr<MutableStyleProperties> newInlineStyle;
    if (newInlineStylePtr) {
        newInlineStyle = inlineStyle->mutableCopy();
        *newInlineStylePtr = newInlineStyle;
    }

    bool shouldRemoveUnderline = underlineChange() == TextDecorationChange::Remove;
    bool shouldRemoveStrikeThrough = strikeThroughChange() == TextDecorationChange::Remove;
    if (shouldRemoveUnderline || shouldRemoveStrikeThrough) {
        if (RefPtr<CSSValueList> valueList = textDecorationValueList(*inlineStyle)) {
            auto newValueList = valueList->copy();
            auto extractedValueList = CSSValueList::createSpaceSeparated();

            if (shouldRemoveUnderline && valueList->hasValue(CSSValueUnderline)) {
                if (!newInlineStyle)
                    return true;
                newValueList->removeAll(CSSValueUnderline);
                extractedValueList->append(CSSPrimitiveValue::create(CSSValueUnderline));
            }

            if (shouldRemoveStrikeThrough && valueList->hasValue(CSSValueLineThrough)) {
                if (!newInlineStyle)
                    return true;
                newValueList->removeAll(CSSValueLineThrough);
                extractedValueList->append(CSSPrimitiveValue::create(CSSValueLineThrough));
            }

            if (extractedValueList->length()) {
                conflicts = true;
                if (newValueList->length())
                    newInlineStyle->setProperty(CSSPropertyTextDecorationLine, WTFMove(newValueList));
                else
                    newInlineStyle->removeProperty(CSSPropertyTextDecorationLine);

                if (extractedStyle) {
                    bool isImportant = inlineStyle->propertyIsImportant(CSSPropertyTextDecorationLine);
                    extractedStyle->setProperty(CSSPropertyTextDecorationLine, extractedValueList->cssText(), isImportant);
                }
            }
        }
    }

    if (!m_mutableStyle)
        return conflicts;

    for (auto property : *m_mutableStyle) {
        auto propertyID = property.id();

        // We don't override whitespace property of a tab span because that would collapse the tab into a space.
        if (propertyID == CSSPropertyWhiteSpace && isTabSpanNode(&element))
            continue;

        if (propertyID == CSSPropertyWebkitTextDecorationsInEffect && inlineStyle->getPropertyCSSValue(CSSPropertyTextDecorationLine)) {
            if (!newInlineStyle)
                return true;
            conflicts = true;
            newInlineStyle->removeProperty(CSSPropertyTextDecorationLine);
            if (extractedStyle)
                extractedStyle->setProperty(CSSPropertyTextDecorationLine, inlineStyle->getPropertyValue(CSSPropertyTextDecorationLine), inlineStyle->propertyIsImportant(CSSPropertyTextDecorationLine));
        }

        if (!inlineStyle->getPropertyCSSValue(propertyID))
            continue;

        if (propertyID == CSSPropertyUnicodeBidi && inlineStyle->getPropertyCSSValue(CSSPropertyDirection)) {
            if (!newInlineStyle)
                return true;
            conflicts = true;
            newInlineStyle->removeProperty(CSSPropertyDirection);
            if (extractedStyle)
                extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
        }

        if (!newInlineStyle)
            return true;

        conflicts = true;
        newInlineStyle->removeProperty(propertyID);
        if (extractedStyle)
            extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
    }

    return conflicts;
}

static Span<const HTMLElementEquivalent* const> htmlElementEquivalents()
{
    static const HTMLElementEquivalent* const equivalents[] = {
        new HTMLFontWeightEquivalent(HTMLNames::bTag),
        new HTMLFontWeightEquivalent(HTMLNames::strongTag),

        new HTMLElementEquivalent(CSSPropertyVerticalAlign, CSSValueSub, HTMLNames::subTag),
        new HTMLElementEquivalent(CSSPropertyVerticalAlign, CSSValueSuper, HTMLNames::supTag),
        new HTMLElementEquivalent(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::iTag),
        new HTMLElementEquivalent(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::emTag),

        new HTMLTextDecorationEquivalent(CSSValueUnderline, HTMLNames::uTag),
        new HTMLTextDecorationEquivalent(CSSValueLineThrough, HTMLNames::sTag),
        new HTMLTextDecorationEquivalent(CSSValueLineThrough, HTMLNames::strikeTag),
    };
    return equivalents;
}

bool EditingStyle::conflictsWithImplicitStyleOfElement(HTMLElement& element, EditingStyle* extractedStyle, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    if (isEmpty())
        return false;

    for (auto& equivalent : htmlElementEquivalents()) {
        if (equivalent->matches(element) && equivalent->propertyExistsInStyle(*this)
            && (shouldExtractMatchingStyle == ExtractMatchingStyle || !equivalent->valueIsPresentInStyle(element, *this))) {
            if (extractedStyle)
                equivalent->addToStyle(&element, extractedStyle);
            return true;
        }
    }
    return false;
}

static Span<const HTMLAttributeEquivalent* const> htmlAttributeEquivalents()
{
    static const HTMLAttributeEquivalent* const equivalents[] = {
        // elementIsStyledSpanOrHTMLEquivalent depends on the fact each HTMLAttriuteEquivalent matches exactly one attribute
        // of exactly one element except dirAttr.
        new HTMLAttributeEquivalent(CSSPropertyColor, HTMLNames::fontTag, HTMLNames::colorAttr),
        new HTMLAttributeEquivalent(CSSPropertyFontFamily, HTMLNames::fontTag, HTMLNames::faceAttr),
        new HTMLFontSizeEquivalent,

        new HTMLAttributeEquivalent(CSSPropertyDirection, HTMLNames::dirAttr),
        new HTMLAttributeEquivalent(CSSPropertyUnicodeBidi, HTMLNames::dirAttr),
    };
    return equivalents;
}

bool EditingStyle::conflictsWithImplicitStyleOfAttributes(HTMLElement& element) const
{
    if (isEmpty())
        return false;

    for (auto& equivalent : htmlAttributeEquivalents()) {
        if (equivalent->matches(element) && equivalent->propertyExistsInStyle(*this) && !equivalent->valueIsPresentInStyle(element, *this))
            return true;
    }

    return false;
}

bool EditingStyle::extractConflictingImplicitStyleOfAttributes(HTMLElement& element, ShouldPreserveWritingDirection shouldPreserveWritingDirection,
    EditingStyle* extractedStyle, Vector<QualifiedName>& conflictingAttributes, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    // HTMLAttributeEquivalent::addToStyle doesn't support unicode-bidi and direction properties
    ASSERT(!extractedStyle || shouldPreserveWritingDirection == PreserveWritingDirection);
    if (!m_mutableStyle)
        return false;

    bool removed = false;
    for (auto& equivalent : htmlAttributeEquivalents()) {
        // unicode-bidi and direction are pushed down separately so don't push down with other styles.
        if (shouldPreserveWritingDirection == PreserveWritingDirection && equivalent->attributeName() == HTMLNames::dirAttr)
            continue;

        if (!equivalent->matches(element) || !equivalent->propertyExistsInStyle(*this)
            || (shouldExtractMatchingStyle == DoNotExtractMatchingStyle && equivalent->valueIsPresentInStyle(element, *this)))
            continue;

        if (extractedStyle)
            equivalent->addToStyle(&element, extractedStyle);
        conflictingAttributes.append(equivalent->attributeName());
        removed = true;
    }

    return removed;
}

bool EditingStyle::styleIsPresentInComputedStyleOfNode(Node& node) const
{
    if (isEmpty())
        return true;
    ComputedStyleExtractor computedStyle(&node);

    bool shouldAddUnderline = underlineChange() == TextDecorationChange::Add;
    bool shouldAddLineThrough = strikeThroughChange() == TextDecorationChange::Add;
    if (shouldAddUnderline || shouldAddLineThrough) {
        bool hasUnderline = false;
        bool hasLineThrough = false;
        if (RefPtr<CSSValue> value = computedStyle.propertyValue(CSSPropertyTextDecorationLine)) {
            if (value->isValueList()) {
                const CSSValueList& valueList = downcast<CSSValueList>(*value);
                hasUnderline = valueList.hasValue(CSSValueUnderline);
                hasLineThrough = valueList.hasValue(CSSValueLineThrough);
            }
        }
        if ((shouldAddUnderline && !hasUnderline) || (shouldAddLineThrough && !hasLineThrough))
            return false;
    }

    return !m_mutableStyle || getPropertiesNotIn(*m_mutableStyle, computedStyle)->isEmpty();
}

bool EditingStyle::elementIsStyledSpanOrHTMLEquivalent(const HTMLElement& element)
{
    bool elementIsSpanOrElementEquivalent = false;
    if (element.hasTagName(HTMLNames::spanTag))
        elementIsSpanOrElementEquivalent = true;
    else {
        for (auto& equivalent : htmlElementEquivalents()) {
            if (equivalent->matches(element)) {
                elementIsSpanOrElementEquivalent = true;
                break;
            }
        }
    }

    if (!element.hasAttributes())
        return elementIsSpanOrElementEquivalent; // span, b, etc... without any attributes

    unsigned matchedAttributes = 0;
    for (auto& equivalent : htmlAttributeEquivalents()) {
        if (equivalent->matches(element) && equivalent->attributeName() != HTMLNames::dirAttr)
            matchedAttributes++;
    }

    if (!elementIsSpanOrElementEquivalent && !matchedAttributes)
        return false; // element is not a span, a html element equivalent, or font element.
    
    if (element.attributeWithoutSynchronization(HTMLNames::classAttr) == AppleStyleSpanClass)
        matchedAttributes++;

    if (element.hasAttribute(HTMLNames::styleAttr)) {
        if (const auto* style = element.inlineStyle()) {
            for (auto property : *style) {
                if (!isEditingProperty(property.id()))
                    return false;
            }
        }
        matchedAttributes++;
    }

    // font with color attribute, span with style attribute, etc...
    ASSERT(matchedAttributes <= element.attributeCount());
    return matchedAttributes >= element.attributeCount();
}

void EditingStyle::prepareToApplyAt(const Position& position, ShouldPreserveWritingDirection shouldPreserveWritingDirection)
{
    if (!m_mutableStyle)
        return;

    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    auto editingStyleAtPosition = EditingStyle::create(position, EditingPropertiesInEffect);
    StyleProperties* styleAtPosition = editingStyleAtPosition->m_mutableStyle.get();

    RefPtr<CSSValue> unicodeBidi;
    RefPtr<CSSValue> direction;
    if (shouldPreserveWritingDirection == PreserveWritingDirection) {
        unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
    }

    removeEquivalentProperties(*styleAtPosition);

    if (textAlignResolvingStartAndEnd(*m_mutableStyle) == textAlignResolvingStartAndEnd(*styleAtPosition))
        m_mutableStyle->removeProperty(CSSPropertyTextAlign);

    if (equalIgnoringSemanticColor(textColorFromStyle(*m_mutableStyle), textColorFromStyle(*styleAtPosition)))
        m_mutableStyle->removeProperty(CSSPropertyColor);

    if (equalIgnoringSemanticColor(caretColorFromStyle(*m_mutableStyle), caretColorFromStyle(*styleAtPosition)))
        m_mutableStyle->removeProperty(CSSPropertyCaretColor);

    if (hasTransparentBackgroundColor(m_mutableStyle.get())
        || cssValueToColor(m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor).get()) == rgbaBackgroundColorInEffect(position.containerNode()))
        m_mutableStyle->removeProperty(CSSPropertyBackgroundColor);

    if (is<CSSPrimitiveValue>(unicodeBidi)) {
        m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, static_cast<CSSValueID>(downcast<CSSPrimitiveValue>(*unicodeBidi).valueID()));
        if (is<CSSPrimitiveValue>(direction))
            m_mutableStyle->setProperty(CSSPropertyDirection, static_cast<CSSValueID>(downcast<CSSPrimitiveValue>(*direction).valueID()));
    }
}

void EditingStyle::mergeTypingStyle(Document& document)
{
    RefPtr<EditingStyle> typingStyle = document.frame()->selection().typingStyle();
    if (!typingStyle || typingStyle == this)
        return;

    mergeStyle(typingStyle->style(), OverrideValues);
}

void EditingStyle::mergeInlineStyleOfElement(StyledElement& element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude)
{
    if (!element.inlineStyle())
        return;

    switch (propertiesToInclude) {
    case AllProperties:
        mergeStyle(element.inlineStyle(), mode);
        return;
    case OnlyEditingInheritableProperties:
        mergeStyle(copyEditingProperties(element.inlineStyle(), OnlyInheritableEditingProperties).ptr(), mode);
        return;
    case EditingPropertiesInEffect:
        mergeStyle(copyEditingProperties(element.inlineStyle(), AllEditingProperties).ptr(), mode);
        return;
    }
}

static inline bool elementMatchesAndPropertyIsNotInInlineStyleDecl(const HTMLElementEquivalent& equivalent, const StyledElement& element,
    EditingStyle::CSSPropertyOverrideMode mode, EditingStyle& style)
{
    if (!equivalent.matches(element))
        return false;
    if (mode != EditingStyle::OverrideValues && equivalent.propertyExistsInStyle(style))
        return false;

    return !element.inlineStyle() || !equivalent.propertyExistsInStyle(EditingStyle::create(element.inlineStyle()).get());
}

static RefPtr<MutableStyleProperties> extractEditingProperties(const StyleProperties* style, EditingStyle::PropertiesToInclude propertiesToInclude)
{
    if (!style)
        return nullptr;

    switch (propertiesToInclude) {
    case EditingStyle::OnlyEditingInheritableProperties:
        return copyEditingProperties(style, OnlyInheritableEditingProperties);
    case EditingStyle::AllProperties:
    case EditingStyle::EditingPropertiesInEffect:
        break;
    }
    return copyEditingProperties(style, AllEditingProperties);
}

void EditingStyle::mergeInlineAndImplicitStyleOfElement(StyledElement& element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude, StandardFontFamilySerializationMode standardFontFamilySerializationMode)
{
    auto styleFromRules = EditingStyle::create();
    styleFromRules->mergeStyleFromRulesForSerialization(element, standardFontFamilySerializationMode);

    if (element.inlineStyle())
        styleFromRules->m_mutableStyle->mergeAndOverrideOnConflict(*element.inlineStyle());

    styleFromRules->m_mutableStyle = extractEditingProperties(styleFromRules->m_mutableStyle.get(), propertiesToInclude);
    mergeStyle(styleFromRules->m_mutableStyle.get(), mode);

    for (auto& equivalent : htmlElementEquivalents()) {
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(*equivalent, element, mode, *this))
            equivalent->addToStyle(&element, this);
    }

    for (auto& equivalent : htmlAttributeEquivalents()) {
        if (equivalent->attributeName() == HTMLNames::dirAttr)
            continue; // We don't want to include directionality
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(*equivalent, element, mode, *this))
            equivalent->addToStyle(&element, this);
    }
}

Ref<EditingStyle> EditingStyle::wrappingStyleForSerialization(Node& context, bool shouldAnnotate, StandardFontFamilySerializationMode standardFontFamilySerializationMode)
{
    if (shouldAnnotate) {
        auto wrappingStyle = EditingStyle::create(&context, EditingStyle::EditingPropertiesInEffect);

        // Styles that Mail blockquotes contribute should only be placed on the Mail blockquote,
        // to help us differentiate those styles from ones that the user has applied.
        // This helps us get the color of content pasted into blockquotes right.
        wrappingStyle->removeStyleAddedByNode(enclosingNodeOfType(firstPositionInOrBeforeNode(&context), isMailBlockquote, CanCrossEditingBoundary));

        // Call collapseTextDecorationProperties first or otherwise it'll copy the value over from in-effect to text-decorations.
        wrappingStyle->collapseTextDecorationProperties();
        
        return wrappingStyle;
    }

    auto wrappingStyle = EditingStyle::create();

    // When not annotating for interchange, we only preserve inline style declarations.
    for (Node* node = &context; node && !node->isDocumentNode(); node = node->parentNode()) {
        if (is<StyledElement>(*node) && !isMailBlockquote(node))
            wrappingStyle->mergeInlineAndImplicitStyleOfElement(downcast<StyledElement>(*node), DoNotOverrideValues, EditingPropertiesInEffect, standardFontFamilySerializationMode);
    }

    return wrappingStyle;
}


static void mergeTextDecorationValues(CSSValueList& mergedValue, const CSSValueList& valueToMerge)
{
    if (valueToMerge.hasValue(CSSValueUnderline) && !mergedValue.hasValue(CSSValueUnderline))
        mergedValue.append(CSSPrimitiveValue::create(CSSValueUnderline));

    if (valueToMerge.hasValue(CSSValueLineThrough) && !mergedValue.hasValue(CSSValueLineThrough))
        mergedValue.append(CSSPrimitiveValue::create(CSSValueLineThrough));
}

void EditingStyle::mergeStyle(const StyleProperties* style, CSSPropertyOverrideMode mode)
{
    if (!style)
        return;

    if (!m_mutableStyle) {
        m_mutableStyle = style->mutableCopy();
        return;
    }

    for (auto property : *style) {
        auto value = m_mutableStyle->getPropertyCSSValue(property.id());

        // text decorations never override values.
        if ((property.id() == CSSPropertyTextDecorationLine || property.id() == CSSPropertyWebkitTextDecorationsInEffect)
            && is<CSSValueList>(*property.value()) && value) {
            if (is<CSSValueList>(*value)) {
                auto newValue = downcast<CSSValueList>(*value).copy();
                mergeTextDecorationValues(newValue, downcast<CSSValueList>(*property.value()));
                m_mutableStyle->setProperty(property.id(), WTFMove(newValue), property.isImportant());
                continue;
            }
            value = nullptr; // text-decoration: none is equivalent to not having the property.
        }

        if (mode == OverrideValues || (mode == DoNotOverrideValues && !value))
            m_mutableStyle->setProperty(property.id(), property.value(), property.isImportant());
    }

    int oldFontSizeDelta = m_fontSizeDelta;
    extractFontSizeDelta();
    m_fontSizeDelta += oldFontSizeDelta;
}

static Ref<MutableStyleProperties> styleFromMatchedRulesForElement(Element& element, unsigned rulesToInclude)
{
    auto style = MutableStyleProperties::create();
    for (auto& matchedRule : element.styleResolver().styleRulesForElement(&element, rulesToInclude))
        style->mergeAndOverrideOnConflict(matchedRule->properties());
    
    return style;
}

void EditingStyle::mergeStyleFromRules(StyledElement& element)
{
    auto styleFromMatchedRules = styleFromMatchedRulesForElement(element, Style::Resolver::AuthorCSSRules);
    // Styles from the inline style declaration, held in the variable "style", take precedence 
    // over those from matched rules.
    if (m_mutableStyle)
        styleFromMatchedRules->mergeAndOverrideOnConflict(*m_mutableStyle);

    clear();
    m_mutableStyle = WTFMove(styleFromMatchedRules);
}

static String loneFontFamilyName(const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value))
        return downcast<CSSPrimitiveValue>(value).stringValue();
    if (!is<CSSValueList>(value) || downcast<CSSValueList>(value).length() != 1)
        return { };
    auto& item = *downcast<CSSValueList>(value).item(0);
    if (!is<CSSPrimitiveValue>(item))
        return { };
    return downcast<CSSPrimitiveValue>(item).stringValue();
}

void EditingStyle::mergeStyleFromRulesForSerialization(StyledElement& element, StandardFontFamilySerializationMode standardFontFamilySerializationMode)
{
    mergeStyleFromRules(element);

    // The property value, if it's a percentage, may not reflect the actual computed value.  
    // For example: style="height: 1%; overflow: visible;" in quirksmode
    // FIXME: There are others like this, see <rdar://problem/5195123> Slashdot copy/paste fidelity problem
    auto fromComputedStyle = MutableStyleProperties::create();
    ComputedStyleExtractor computedStyle(&element);

    bool shouldRemoveFontFamily = false;
    {
        for (auto property : *m_mutableStyle) {
            auto& value = *property.value();
            if (property.id() == CSSPropertyFontFamily) {
                auto familyName = loneFontFamilyName(value);
                if (FontCache::isSystemFontForbiddenForEditing(familyName)
                    || (standardFontFamilySerializationMode == StandardFontFamilySerializationMode::Strip && familyName == standardFamily))
                    shouldRemoveFontFamily = true;
                continue;
            }
            if (!is<CSSPrimitiveValue>(value))
                continue;
            if (downcast<CSSPrimitiveValue>(value).isPercentage()) {
                if (auto computedPropertyValue = computedStyle.propertyValue(property.id()))
                    fromComputedStyle->addParsedProperty(CSSProperty(property.id(), WTFMove(computedPropertyValue)));
            }
        }
    }
    if (shouldRemoveFontFamily) {
        m_mutableStyle->removeProperty(CSSPropertyFontFamily);
        fromComputedStyle->removeProperty(CSSPropertyFontFamily);
    }
    m_mutableStyle->mergeAndOverrideOnConflict(fromComputedStyle.get());
}

static void removePropertiesInStyle(MutableStyleProperties& styleToRemovePropertiesFrom, MutableStyleProperties& style)
{
    styleToRemovePropertiesFrom.removeProperties(map(style, [](auto property) {
        return property.id();
    }).span());
}

void EditingStyle::removeStyleFromRulesAndContext(StyledElement& element, Node* context)
{
    if (!m_mutableStyle)
        return;

    // 1. Remove style from matched rules because style remain without repeating it in inline style declaration
    auto styleFromMatchedRules = styleFromMatchedRulesForElement(element, Style::Resolver::AllButEmptyCSSRules);
    if (!styleFromMatchedRules->isEmpty())
        m_mutableStyle = getPropertiesNotIn(*m_mutableStyle, styleFromMatchedRules.get());

    // 2. Remove style present in context and not overridden by matched rules.
    auto computedStyle = EditingStyle::create(context, EditingPropertiesInEffect);
    if (computedStyle->m_mutableStyle) {
        if (!computedStyle->m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor))
            computedStyle->m_mutableStyle->setProperty(CSSPropertyBackgroundColor, CSSValueTransparent);

        RefPtr<EditingStyle> computedStyleOfElement;
        auto replaceSemanticColorWithComputedValue = [&](const CSSPropertyID id) {
            auto color = m_mutableStyle->propertyAsColor(id);
            if (!color || (color->isVisible() && !color->isSemantic()))
                return;

            if (!computedStyleOfElement)
                computedStyleOfElement = EditingStyle::create(&element, EditingPropertiesInEffect);

            if (!computedStyleOfElement->m_mutableStyle)
                return;

            auto computedValue = computedStyleOfElement->m_mutableStyle->getPropertyValue(id);
            if (!computedValue)
                return;

            m_mutableStyle->setProperty(id, computedValue);
        };

        // Replace semantic color identifiers like -apple-system-label with RGB values so that comparsions in getPropertiesNotIn below would work.
        replaceSemanticColorWithComputedValue(CSSPropertyColor);
        replaceSemanticColorWithComputedValue(CSSPropertyCaretColor);
        replaceSemanticColorWithComputedValue(CSSPropertyBackgroundColor);

        removePropertiesInStyle(*computedStyle->m_mutableStyle, styleFromMatchedRules.get());
        m_mutableStyle = getPropertiesNotIn(*m_mutableStyle, *computedStyle->m_mutableStyle);
    }

    // 3. If this element is a span and has display: inline or float: none, remove them unless they are overridden by rules.
    // These rules are added by serialization code to wrap text nodes.
    if (isStyleSpanOrSpanWithOnlyStyleAttribute(element)) {
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyDisplay) && identifierForStyleProperty(*m_mutableStyle, CSSPropertyDisplay) == CSSValueInline)
            m_mutableStyle->removeProperty(CSSPropertyDisplay);
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyFloat) && identifierForStyleProperty(*m_mutableStyle, CSSPropertyFloat) == CSSValueNone)
            m_mutableStyle->removeProperty(CSSPropertyFloat);
    }
}

void EditingStyle::removePropertiesInElementDefaultStyle(Element& element)
{
    if (!m_mutableStyle || m_mutableStyle->isEmpty())
        return;

    auto defaultStyle = styleFromMatchedRulesForElement(element, Style::Resolver::UAAndUserCSSRules);

    removePropertiesInStyle(*m_mutableStyle, defaultStyle.get());
}

template<typename T>
void EditingStyle::removeEquivalentProperties(T& style)
{
    Vector<CSSPropertyID> propertiesToRemove;
    for (auto& property : m_mutableStyle->m_propertyVector) {
        if (style.propertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }
    // FIXME: This should use mass removal.
    for (auto& property : propertiesToRemove)
        m_mutableStyle->removeProperty(property);
}

void EditingStyle::forceInline()
{
    if (!m_mutableStyle)
        m_mutableStyle = MutableStyleProperties::create();
    const bool propertyIsImportant = true;
    m_mutableStyle->setProperty(CSSPropertyDisplay, CSSValueInline, propertyIsImportant);
}

void EditingStyle::addDisplayContents()
{
    if (!m_mutableStyle)
        m_mutableStyle = MutableStyleProperties::create();
    m_mutableStyle->setProperty(CSSPropertyDisplay, CSSValueContents);
}

bool EditingStyle::convertPositionStyle()
{
    if (!m_mutableStyle)
        return false;

    RefPtr<CSSPrimitiveValue> sticky = CSSPrimitiveValue::create(CSSValueSticky);
    if (m_mutableStyle->propertyMatches(CSSPropertyPosition, sticky.get())) {
        m_mutableStyle->setProperty(CSSPropertyPosition, CSSPrimitiveValue::create(CSSValueStatic), m_mutableStyle->propertyIsImportant(CSSPropertyPosition));
        return false;
    }
    RefPtr<CSSPrimitiveValue> fixed = CSSPrimitiveValue::create(CSSValueFixed);
    if (m_mutableStyle->propertyMatches(CSSPropertyPosition, fixed.get())) {
        m_mutableStyle->setProperty(CSSPropertyPosition, CSSPrimitiveValue::create(CSSValueAbsolute), m_mutableStyle->propertyIsImportant(CSSPropertyPosition));
        return true;
    }
    RefPtr<CSSPrimitiveValue> absolute = CSSPrimitiveValue::create(CSSValueAbsolute);
    if (m_mutableStyle->propertyMatches(CSSPropertyPosition, absolute.get()))
        return true;
    return false;
}

bool EditingStyle::isFloating()
{
    RefPtr<CSSValue> v = m_mutableStyle->getPropertyCSSValue(CSSPropertyFloat);
    RefPtr<CSSPrimitiveValue> noneValue = CSSPrimitiveValue::create(CSSValueNone);
    return v && !v->equals(*noneValue);
}

int EditingStyle::legacyFontSize(Document& document) const
{
    RefPtr<CSSValue> cssValue = m_mutableStyle->getPropertyCSSValue(CSSPropertyFontSize);
    if (!is<CSSPrimitiveValue>(cssValue))
        return 0;
    return legacyFontSizeFromCSSValue(document, downcast<CSSPrimitiveValue>(cssValue.get()),
        m_shouldUseFixedDefaultFontSize, AlwaysUseLegacyFontSize);
}

bool EditingStyle::hasStyle(CSSPropertyID propertyID, const String& value)
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(this) != TriState::False;
}

RefPtr<EditingStyle> EditingStyle::styleAtSelectionStart(const VisibleSelection& selection, bool shouldUseBackgroundColorInEffect)
{
    if (selection.isNone())
        return nullptr;

    Position position = adjustedSelectionStartForStyleComputation(selection);

    // If the pos is at the end of a text node, then this node is not fully selected. 
    // Move it to the next deep equivalent position to avoid removing the style from this node. 
    // e.g. if pos was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead. 
    // We only do this for range because caret at Position("hello", 5) in <b>hello</b>world should give you font-weight: bold. 
    Node* positionNode = position.containerNode(); 
    if (selection.isRange() && is<Text>(positionNode) && static_cast<unsigned>(position.computeOffsetInContainerNode()) == downcast<Text>(*positionNode).length())
        position = nextVisuallyDistinctCandidate(position);

    Element* element = position.element();
    if (!element)
        return nullptr;

    auto style = EditingStyle::create(element, EditingStyle::AllProperties);
    style->mergeTypingStyle(element->document());

    // If background color is transparent, traverse parent nodes until we hit a different value or document root
    // Also, if the selection is a range, ignore the background color at the start of selection,
    // and find the background color of the common ancestor.
    if (shouldUseBackgroundColorInEffect && (selection.isRange() || hasTransparentBackgroundColor(style->m_mutableStyle.get()))) {
        if (auto range = selection.toNormalizedRange()) {
            if (auto value = backgroundColorInEffect(commonInclusiveAncestor<ComposedTree>(*range)))
                style->setProperty(CSSPropertyBackgroundColor, value->cssText());
        }
    }

    return style;
}

WritingDirection EditingStyle::textDirectionForSelection(const VisibleSelection& selection, EditingStyle* typingStyle, bool& hasNestedOrMultipleEmbeddings)
{
    hasNestedOrMultipleEmbeddings = true;

    if (selection.isNone())
        return WritingDirection::Natural;

    Position position = selection.start().downstream();

    Node* node = position.deprecatedNode();
    if (!node)
        return WritingDirection::Natural;

    Position end;
    if (selection.isRange()) {
        end = selection.end().upstream();
        for (auto& intersectingNode : intersectingNodes(*makeSimpleRange(position, end))) {
            if (!intersectingNode.isStyledElement())
                continue;
            auto valueObject = ComputedStyleExtractor(&intersectingNode).propertyValue(CSSPropertyUnicodeBidi);
            auto value = is<CSSPrimitiveValue>(valueObject) ? downcast<CSSPrimitiveValue>(*valueObject).valueID() : CSSValueInvalid;
            if (value == CSSValueEmbed || value == CSSValueBidiOverride)
                return WritingDirection::Natural;
        }
    }

    if (selection.isCaret()) {
        if (typingStyle) {
            if (auto direction = typingStyle->textDirection()) {
                hasNestedOrMultipleEmbeddings = false;
                return *direction;
            }
        }
        node = selection.visibleStart().deepEquivalent().deprecatedNode();
    }

    // The selection is either a caret with no typing attributes or a range in which no embedding is added, so just use the start position
    // to decide.
    Node* block = enclosingBlock(node);
    auto foundDirection = WritingDirection::Natural;

    for (; node != block; node = node->parentNode()) {
        if (!node->isStyledElement())
            continue;

        ComputedStyleExtractor computedStyle(node);
        RefPtr<CSSValue> unicodeBidi = computedStyle.propertyValue(CSSPropertyUnicodeBidi);
        if (!is<CSSPrimitiveValue>(unicodeBidi))
            continue;

        CSSValueID unicodeBidiValue = downcast<CSSPrimitiveValue>(*unicodeBidi).valueID();
        if (unicodeBidiValue == CSSValueNormal)
            continue;

        if (unicodeBidiValue == CSSValueBidiOverride)
            return WritingDirection::Natural;

        ASSERT(unicodeBidiValue == CSSValueEmbed);
        RefPtr<CSSValue> direction = computedStyle.propertyValue(CSSPropertyDirection);
        if (!is<CSSPrimitiveValue>(direction))
            continue;

        CSSValueID directionValue = downcast<CSSPrimitiveValue>(*direction).valueID();
        if (directionValue != CSSValueLtr && directionValue != CSSValueRtl)
            continue;

        if (foundDirection != WritingDirection::Natural)
            return WritingDirection::Natural;

        // In the range case, make sure that the embedding element persists until the end of the range.
        if (selection.isRange() && !end.deprecatedNode()->isDescendantOf(*node))
            return WritingDirection::Natural;
        
        foundDirection = directionValue == CSSValueLtr ? WritingDirection::LeftToRight : WritingDirection::RightToLeft;
    }
    hasNestedOrMultipleEmbeddings = false;
    return foundDirection;
}

Ref<EditingStyle> EditingStyle::inverseTransformColorIfNeeded(Element& element)
{
    auto* renderer = element.renderer();
    if (!m_mutableStyle || !renderer || !renderer->style().hasAppleColorFilter())
        return *this;

    auto colorForPropertyIfInvertible = [&](CSSPropertyID id) -> std::optional<Color> {
        auto color = m_mutableStyle->propertyAsColor(id);
        if (!color || !color->isVisible() || color->isSemantic())
            return std::nullopt;
        return color;
    };

    auto color = colorForPropertyIfInvertible(CSSPropertyColor);
    auto caretColor = colorForPropertyIfInvertible(CSSPropertyCaretColor);
    auto backgroundColor = colorForPropertyIfInvertible(CSSPropertyBackgroundColor);
    if (!color && !caretColor && !backgroundColor)
        return *this;

    auto styleWithInvertedColors = copy();
    ASSERT(styleWithInvertedColors->m_mutableStyle);

    const auto& colorFilter = renderer->style().appleColorFilter();
    auto invertedColor = [&](CSSPropertyID propertyID) {
        Color newColor = cssValueToColor(extractPropertyValue(*m_mutableStyle, propertyID).get());
        colorFilter.inverseTransformColor(newColor);
        styleWithInvertedColors->m_mutableStyle->setProperty(propertyID, serializationForCSS(newColor));
    };

    if (color)
        invertedColor(CSSPropertyColor);

    if (caretColor)
        invertedColor(CSSPropertyCaretColor);

    if (backgroundColor)
        invertedColor(CSSPropertyBackgroundColor);

    return styleWithInvertedColors;
}

static void reconcileTextDecorationProperties(MutableStyleProperties& style)
{    
    auto textDecorationsInEffect = style.getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    auto textDecoration = style.getPropertyCSSValue(CSSPropertyTextDecorationLine);
    // We shouldn't have both text-decoration and -webkit-text-decorations-in-effect because that wouldn't make sense.
    ASSERT(!textDecorationsInEffect || !textDecoration);
    if (textDecorationsInEffect) {
        style.setProperty(CSSPropertyTextDecorationLine, textDecorationsInEffect->cssText());
        style.removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
        textDecoration = textDecorationsInEffect;
    }

    // If text-decoration is set to "none", remove the property because we don't want to add redundant "text-decoration: none".
    if (textDecoration && !textDecoration->isValueList())
        style.removeProperty(CSSPropertyTextDecorationLine);
}

StyleChange::StyleChange(EditingStyle* style, const Position& position)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applyUnderline(false)
    , m_applyLineThrough(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
{
    Document* document = position.deprecatedNode() ? &position.deprecatedNode()->document() : 0;
    if (!style || style->isEmpty() || !document || !document->frame())
        return;

    Node* node = position.containerNode();
    if (!node)
        return;

    ComputedStyleExtractor computedStyle(node);

    // FIXME: take care of background-color in effect
    auto mutableStyle = style->style() ? getPropertiesNotIn(*style->style(), computedStyle) : MutableStyleProperties::create();

    reconcileTextDecorationProperties(mutableStyle.get());
    bool shouldStyleWithCSS = document->editor().shouldStyleWithCSS();
    if (!shouldStyleWithCSS)
        extractTextStyles(*document, mutableStyle.get(), computedStyle.useFixedFontDefaultSize());

    bool shouldAddUnderline = style->underlineChange() == TextDecorationChange::Add;
    bool shouldAddStrikeThrough = style->strikeThroughChange() == TextDecorationChange::Add;
    if (shouldAddUnderline || shouldAddStrikeThrough) {
        RefPtr<CSSValue> value = computedStyle.propertyValue(CSSPropertyWebkitTextDecorationsInEffect);
        if (!is<CSSValueList>(value))
            value = computedStyle.propertyValue(CSSPropertyTextDecorationLine);

        RefPtr<CSSValueList> valueList;
        if (is<CSSValueList>(value))
            valueList = downcast<CSSValueList>(value.get());

        bool hasUnderline = valueList && valueList->hasValue(CSSValueUnderline);
        bool hasLineThrough = valueList && valueList->hasValue(CSSValueLineThrough);

        if (shouldStyleWithCSS) {
            valueList = valueList ? valueList->copy() : CSSValueList::createSpaceSeparated();
            if (shouldAddUnderline && !hasUnderline)
                valueList->append(CSSPrimitiveValue::create(CSSValueUnderline));
            if (shouldAddStrikeThrough && !hasLineThrough)
                valueList->append(CSSPrimitiveValue::create(CSSValueLineThrough));
            mutableStyle->setProperty(CSSPropertyTextDecorationLine, valueList.get());
        } else {
            m_applyUnderline = shouldAddUnderline && !hasUnderline;
            m_applyLineThrough = shouldAddStrikeThrough && !hasLineThrough;
        }
    }

    // Changing the whitespace style in a tab span would collapse the tab into a space.
    if (isTabSpanTextNode(position.deprecatedNode()) || isTabSpanNode((position.deprecatedNode())))
        mutableStyle->removeProperty(CSSPropertyWhiteSpace);

    // If unicode-bidi is present in mutableStyle and direction is not, then add direction to mutableStyle.
    // FIXME: Shouldn't this be done in getPropertiesNotIn?
    if (mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi) && !style->style()->getPropertyCSSValue(CSSPropertyDirection))
        mutableStyle->setProperty(CSSPropertyDirection, style->style()->getPropertyValue(CSSPropertyDirection));

    if (!mutableStyle->isEmpty())
        m_cssStyle = WTFMove(mutableStyle);
}

StyleChange::~StyleChange() = default;

bool StyleChange::operator==(const StyleChange& other)
{
    if (m_applyBold != other.m_applyBold
        || m_applyItalic != other.m_applyItalic
        || m_applyUnderline != other.m_applyUnderline
        || m_applyLineThrough != other.m_applyLineThrough
        || m_applySubscript != other.m_applySubscript
        || m_applySuperscript != other.m_applySuperscript
        || m_applyFontColor != other.m_applyFontColor
        || m_applyFontFace != other.m_applyFontFace
        || m_applyFontSize != other.m_applyFontSize)
        return false;

    return (!m_cssStyle && !other.m_cssStyle)
        || (m_cssStyle && other.m_cssStyle && m_cssStyle->asText() == other.m_cssStyle->asText());
}

static void setTextDecorationProperty(MutableStyleProperties& style, const CSSValueList& newTextDecoration, CSSPropertyID propertyID)
{
    if (newTextDecoration.length())
        style.setProperty(propertyID, newTextDecoration.cssText(), style.propertyIsImportant(propertyID));
    else {
        // text-decoration: none is redundant since it does not remove any text decorations.
        style.removeProperty(propertyID);
    }
}

void StyleChange::extractTextStyles(Document& document, MutableStyleProperties& style, bool shouldUseFixedFontDefaultSize)
{
    if (identifierForStyleProperty(style, CSSPropertyFontWeight) == CSSValueBold) {
        style.removeProperty(CSSPropertyFontWeight);
        m_applyBold = true;
    }

    int fontStyle = identifierForStyleProperty(style, CSSPropertyFontStyle);
    if (fontStyle == CSSValueItalic) {
        style.removeProperty(CSSPropertyFontStyle);
        m_applyItalic = true;
    }

    // Assuming reconcileTextDecorationProperties has been called, there should not be -webkit-text-decorations-in-effect
    // Furthermore, text-decoration: none has been trimmed so that text-decoration property is always a CSSValueList.
    auto textDecoration = style.getPropertyCSSValue(CSSPropertyTextDecorationLine);
    if (is<CSSValueList>(textDecoration)) {
        auto newTextDecoration = downcast<CSSValueList>(*textDecoration).copy();
        if (newTextDecoration->removeAll(CSSValueUnderline))
            m_applyUnderline = true;
        if (newTextDecoration->removeAll(CSSValueLineThrough))
            m_applyLineThrough = true;

        // If trimTextDecorations, delete underline and line-through
        setTextDecorationProperty(style, newTextDecoration.get(), CSSPropertyTextDecorationLine);
    }

    int verticalAlign = identifierForStyleProperty(style, CSSPropertyVerticalAlign);
    switch (verticalAlign) {
    case CSSValueSub:
        style.removeProperty(CSSPropertyVerticalAlign);
        m_applySubscript = true;
        break;
    case CSSValueSuper:
        style.removeProperty(CSSPropertyVerticalAlign);
        m_applySuperscript = true;
        break;
    }

    if (style.getPropertyCSSValue(CSSPropertyColor)) {
        auto color = textColorFromStyle(style);
        if (color.isOpaque()) {
            m_applyFontColor = AtomString { serializationForHTML(color) };
            style.removeProperty(CSSPropertyColor);
        }
    }

    // Remove quotes for Outlook 2007 compatibility. See https://bugs.webkit.org/show_bug.cgi?id=79448
    m_applyFontFace = AtomString { makeStringByReplacingAll(style.getPropertyValue(CSSPropertyFontFamily), '\"', ""_s) };
    style.removeProperty(CSSPropertyFontFamily);

    if (RefPtr<CSSValue> fontSize = style.getPropertyCSSValue(CSSPropertyFontSize)) {
        if (!is<CSSPrimitiveValue>(*fontSize))
            style.removeProperty(CSSPropertyFontSize); // Can't make sense of the number. Put no font size.
        else if (int legacyFontSize = legacyFontSizeFromCSSValue(document, downcast<CSSPrimitiveValue>(fontSize.get()),
                shouldUseFixedFontDefaultSize, UseLegacyFontSizeOnlyIfPixelValuesMatch)) {
            m_applyFontSize = AtomString::number(legacyFontSize);
            style.removeProperty(CSSPropertyFontSize);
        }
    }
}

static void diffTextDecorations(MutableStyleProperties& style, CSSPropertyID propertID, CSSValue* refTextDecoration)
{
    auto textDecoration = style.getPropertyCSSValue(propertID);
    if (!is<CSSValueList>(textDecoration) || !is<CSSValueList>(refTextDecoration))
        return;

    auto newTextDecoration = downcast<CSSValueList>(*textDecoration).copy();

    for (auto& value :  downcast<CSSValueList>(*refTextDecoration))
        newTextDecoration->removeAll(value);

    setTextDecorationProperty(style, newTextDecoration.get(), propertID);
}

template<typename T>
static bool fontWeightIsBold(T& style)
{
    RefPtr<CSSValue> fontWeight = extractPropertyValue(style, CSSPropertyFontWeight);
    return fontWeight && fontWeightValueIsBold(*fontWeight);
}

template<typename T>
static Ref<MutableStyleProperties> extractPropertiesNotIn(StyleProperties& styleWithRedundantProperties, T& baseStyle)
{
    auto result = EditingStyle::create(&styleWithRedundantProperties);
    result->removeEquivalentProperties(baseStyle);
    ASSERT(result->style());
    Ref mutableStyle = *result->style();

    auto baseTextDecorationsInEffect = extractPropertyValue(baseStyle, CSSPropertyWebkitTextDecorationsInEffect);
    diffTextDecorations(mutableStyle.get(), CSSPropertyTextDecorationLine, baseTextDecorationsInEffect.get());
    diffTextDecorations(mutableStyle.get(), CSSPropertyWebkitTextDecorationsInEffect, baseTextDecorationsInEffect.get());

    if (extractPropertyValue(baseStyle, CSSPropertyFontWeight) && fontWeightIsBold(mutableStyle.get()) == fontWeightIsBold(baseStyle))
        mutableStyle->removeProperty(CSSPropertyFontWeight);

    if (extractPropertyValue(baseStyle, CSSPropertyColor) && equalIgnoringSemanticColor(textColorFromStyle(mutableStyle.get()), textColorFromStyle(baseStyle)))
        mutableStyle->removeProperty(CSSPropertyColor);

    if (extractPropertyValue(baseStyle, CSSPropertyCaretColor) && equalIgnoringSemanticColor(caretColorFromStyle(mutableStyle.get()), caretColorFromStyle(baseStyle)))
        mutableStyle->removeProperty(CSSPropertyCaretColor);

    if (extractPropertyValue(baseStyle, CSSPropertyTextAlign) && textAlignResolvingStartAndEnd(mutableStyle.get()) == textAlignResolvingStartAndEnd(baseStyle))
        mutableStyle->removeProperty(CSSPropertyTextAlign);

    if (extractPropertyValue(baseStyle, CSSPropertyBackgroundColor) && equalIgnoringSemanticColor(backgroundColorFromStyle(mutableStyle.get()), backgroundColorFromStyle(baseStyle)))
        mutableStyle->removeProperty(CSSPropertyBackgroundColor);

    return mutableStyle;
}

template<typename T>
Ref<MutableStyleProperties> getPropertiesNotIn(StyleProperties& styleWithRedundantProperties, T& baseStyle)
{
    return extractPropertiesNotIn(styleWithRedundantProperties, baseStyle);
}

static bool isCSSValueLength(CSSPrimitiveValue* value)
{
    return value->isFontIndependentLength();
}

int legacyFontSizeFromCSSValue(Document& document, CSSPrimitiveValue* value, bool shouldUseFixedFontDefaultSize, LegacyFontSizeMode mode)
{
    if (isCSSValueLength(value)) {
        int pixelFontSize = value->intValue(CSSUnitType::CSS_PX);
        int legacyFontSize = Style::legacyFontSizeForPixelSize(pixelFontSize, shouldUseFixedFontDefaultSize, document);
        // Use legacy font size only if pixel value matches exactly to that of legacy font size.
        int cssPrimitiveEquivalent = legacyFontSize - 1 + CSSValueXSmall;
        if (mode == AlwaysUseLegacyFontSize || Style::fontSizeForKeyword(cssPrimitiveEquivalent, shouldUseFixedFontDefaultSize, document) == pixelFontSize)
            return legacyFontSize;

        return 0;
    }

    if (CSSValueXSmall <= value->valueID() && value->valueID() <= CSSValueXxxLarge)
        return value->valueID() - CSSValueXSmall + 1;

    return 0;
}

static bool isTransparentColorValue(CSSValue* value)
{
    if (!value)
        return true;
    return value->isColor() ? !value->color().isVisible() : value->valueID() == CSSValueTransparent;
}

bool hasTransparentBackgroundColor(StyleProperties* style)
{
    return isTransparentColorValue(style->getPropertyCSSValue(CSSPropertyBackgroundColor).get());
}

RefPtr<CSSValue> backgroundColorInEffect(Node* node)
{
    for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        auto value = ComputedStyleExtractor(ancestor).propertyValue(CSSPropertyBackgroundColor);
        if (!isTransparentColorValue(value.get()))
            return value;
    }
    return nullptr;
}

}

/*
 * Copyright (C) 2007, 2008, 2009 Apple Computer, Inc.
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
#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLFontElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "Node.h"
#include "Position.h"
#include "QualifiedName.h"
#include "RenderStyle.h"
#include "StyledElement.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/HashSet.h>

namespace WebCore {

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be copied to the new paragraph.
static const int editingProperties[] = {
    CSSPropertyBackgroundColor,
    CSSPropertyTextDecoration,

    // CSS inheritable properties
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSizeAdjust,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};

enum EditingPropertiesType { OnlyInheritableEditingProperties, AllEditingProperties };

template <class StyleDeclarationType>
static PassRefPtr<CSSMutableStyleDeclaration> copyEditingProperties(StyleDeclarationType* style, EditingPropertiesType type = OnlyInheritableEditingProperties)
{
    if (type == AllEditingProperties)
        return style->copyPropertiesInSet(editingProperties, WTF_ARRAY_LENGTH(editingProperties));
    return style->copyPropertiesInSet(editingProperties + 2, WTF_ARRAY_LENGTH(editingProperties) - 2);
}

static inline bool isEditingProperty(int id)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(editingProperties); ++i) {
        if (editingProperties[i] == id)
            return true;
    }
    return false;
}

static PassRefPtr<CSSMutableStyleDeclaration> editingStyleFromComputedStyle(PassRefPtr<CSSComputedStyleDeclaration> style, EditingPropertiesType type = OnlyInheritableEditingProperties)
{
    if (!style)
        return CSSMutableStyleDeclaration::create();
    return copyEditingProperties(style.get(), type);
}

static RefPtr<CSSMutableStyleDeclaration> getPropertiesNotIn(CSSStyleDeclaration* styleWithRedundantProperties, CSSStyleDeclaration* baseStyle);
enum LegacyFontSizeMode { AlwaysUseLegacyFontSize, UseLegacyFontSizeOnlyIfPixelValuesMatch };
static int legacyFontSizeFromCSSValue(Document*, CSSPrimitiveValue*, bool shouldUseFixedFontDefaultSize, LegacyFontSizeMode);
static bool hasTransparentBackgroundColor(CSSStyleDeclaration*);
static PassRefPtr<CSSValue> backgroundColorInEffect(Node*);

class HTMLElementEquivalent {
public:
    static PassOwnPtr<HTMLElementEquivalent> create(CSSPropertyID propertyID, int primitiveValue, const QualifiedName& tagName)
    {
        return adoptPtr(new HTMLElementEquivalent(propertyID, primitiveValue, tagName));
    }

    virtual ~HTMLElementEquivalent() { }
    virtual bool matches(const Element* element) const { return !m_tagName || element->hasTagName(*m_tagName); }
    virtual bool hasAttribute() const { return false; }
    virtual bool propertyExistsInStyle(CSSMutableStyleDeclaration* style) const { return style && style->getPropertyCSSValue(m_propertyID); }
    virtual bool valueIsPresentInStyle(Element*, CSSMutableStyleDeclaration*) const;
    virtual void addToStyle(Element*, EditingStyle*) const;

protected:
    HTMLElementEquivalent(CSSPropertyID);
    HTMLElementEquivalent(CSSPropertyID, const QualifiedName& tagName);
    HTMLElementEquivalent(CSSPropertyID, int primitiveValue, const QualifiedName& tagName);
    const int m_propertyID;
    const RefPtr<CSSPrimitiveValue> m_primitiveValue;
    const QualifiedName* m_tagName; // We can store a pointer because HTML tag names are const global.
};

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id)
    : m_propertyID(id)
    , m_tagName(0)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, const QualifiedName& tagName)
    : m_propertyID(id)
    , m_tagName(&tagName)
{
}

HTMLElementEquivalent::HTMLElementEquivalent(CSSPropertyID id, int primitiveValue, const QualifiedName& tagName)
    : m_propertyID(id)
    , m_primitiveValue(CSSPrimitiveValue::createIdentifier(primitiveValue))
    , m_tagName(&tagName)
{
    ASSERT(primitiveValue != CSSValueInvalid);
}

bool HTMLElementEquivalent::valueIsPresentInStyle(Element* element, CSSMutableStyleDeclaration* style) const
{
    RefPtr<CSSValue> value = style->getPropertyCSSValue(m_propertyID);
    return matches(element) && value && value->isPrimitiveValue() && static_cast<CSSPrimitiveValue*>(value.get())->getIdent() == m_primitiveValue->getIdent();
}

void HTMLElementEquivalent::addToStyle(Element*, EditingStyle* style) const
{
    style->setProperty(m_propertyID, m_primitiveValue->cssText());
}

class HTMLTextDecorationEquivalent : public HTMLElementEquivalent {
public:
    static PassOwnPtr<HTMLElementEquivalent> create(int primitiveValue, const QualifiedName& tagName)
    {
        return adoptPtr(new HTMLTextDecorationEquivalent(primitiveValue, tagName));
    }
    virtual bool propertyExistsInStyle(CSSMutableStyleDeclaration*) const;
    virtual bool valueIsPresentInStyle(Element*, CSSMutableStyleDeclaration*) const;

private:
    HTMLTextDecorationEquivalent(int primitiveValue, const QualifiedName& tagName);
};

HTMLTextDecorationEquivalent::HTMLTextDecorationEquivalent(int primitiveValue, const QualifiedName& tagName)
    : HTMLElementEquivalent(CSSPropertyTextDecoration, primitiveValue, tagName)
    // m_propertyID is used in HTMLElementEquivalent::addToStyle
{
}

bool HTMLTextDecorationEquivalent::propertyExistsInStyle(CSSMutableStyleDeclaration* style) const
{
    return style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect) || style->getPropertyCSSValue(CSSPropertyTextDecoration);
}

bool HTMLTextDecorationEquivalent::valueIsPresentInStyle(Element* element, CSSMutableStyleDeclaration* style) const
{
    RefPtr<CSSValue> styleValue = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (!styleValue)
        styleValue = style->getPropertyCSSValue(CSSPropertyTextDecoration);
    return matches(element) && styleValue && styleValue->isValueList() && static_cast<CSSValueList*>(styleValue.get())->hasValue(m_primitiveValue.get());
}

class HTMLAttributeEquivalent : public HTMLElementEquivalent {
public:
    static PassOwnPtr<HTMLAttributeEquivalent> create(CSSPropertyID propertyID, const QualifiedName& tagName, const QualifiedName& attrName)
    {
        return adoptPtr(new HTMLAttributeEquivalent(propertyID, tagName, attrName));
    }
    static PassOwnPtr<HTMLAttributeEquivalent> create(CSSPropertyID propertyID, const QualifiedName& attrName)
    {
        return adoptPtr(new HTMLAttributeEquivalent(propertyID, attrName));
    }

    bool matches(const Element* elem) const { return HTMLElementEquivalent::matches(elem) && elem->hasAttribute(m_attrName); }
    virtual bool hasAttribute() const { return true; }
    virtual bool valueIsPresentInStyle(Element*, CSSMutableStyleDeclaration*) const;
    virtual void addToStyle(Element*, EditingStyle*) const;
    virtual PassRefPtr<CSSValue> attributeValueAsCSSValue(Element*) const;
    inline const QualifiedName& attributeName() const { return m_attrName; }

protected:
    HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& tagName, const QualifiedName& attrName);
    HTMLAttributeEquivalent(CSSPropertyID, const QualifiedName& attrName);
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

bool HTMLAttributeEquivalent::valueIsPresentInStyle(Element* element, CSSMutableStyleDeclaration* style) const
{
    RefPtr<CSSValue> value = attributeValueAsCSSValue(element);
    RefPtr<CSSValue> styleValue = style->getPropertyCSSValue(m_propertyID);
    
    // FIXME: This is very inefficient way of comparing values
    // but we can't string compare attribute value and CSS property value.
    return value && styleValue && value->cssText() == styleValue->cssText();
}

void HTMLAttributeEquivalent::addToStyle(Element* element, EditingStyle* style) const
{
    if (RefPtr<CSSValue> value = attributeValueAsCSSValue(element))
        style->setProperty(m_propertyID, value->cssText());
}

PassRefPtr<CSSValue> HTMLAttributeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    ASSERT(element);
    if (!element->hasAttribute(m_attrName))
        return 0;
    
    RefPtr<CSSMutableStyleDeclaration> dummyStyle;
    dummyStyle = CSSMutableStyleDeclaration::create();
    dummyStyle->setProperty(m_propertyID, element->getAttribute(m_attrName));
    return dummyStyle->getPropertyCSSValue(m_propertyID);
}

class HTMLFontSizeEquivalent : public HTMLAttributeEquivalent {
public:
    static PassOwnPtr<HTMLFontSizeEquivalent> create()
    {
        return adoptPtr(new HTMLFontSizeEquivalent());
    }
    virtual PassRefPtr<CSSValue> attributeValueAsCSSValue(Element*) const;

private:
    HTMLFontSizeEquivalent();
};

HTMLFontSizeEquivalent::HTMLFontSizeEquivalent()
    : HTMLAttributeEquivalent(CSSPropertyFontSize, HTMLNames::fontTag, HTMLNames::sizeAttr)
{
}

PassRefPtr<CSSValue> HTMLFontSizeEquivalent::attributeValueAsCSSValue(Element* element) const
{
    ASSERT(element);
    if (!element->hasAttribute(m_attrName))
        return 0;
    int size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(element->getAttribute(m_attrName), size))
        return 0;
    return CSSPrimitiveValue::createIdentifier(size);
}

float EditingStyle::NoFontDelta = 0.0f;

EditingStyle::EditingStyle()
    : m_shouldUseFixedDefaultFontSize(false)
    , m_fontSizeDelta(NoFontDelta)
{
}

EditingStyle::EditingStyle(Node* node, PropertiesToInclude propertiesToInclude)
    : m_shouldUseFixedDefaultFontSize(false)
    , m_fontSizeDelta(NoFontDelta)
{
    init(node, propertiesToInclude);
}

EditingStyle::EditingStyle(const Position& position, PropertiesToInclude propertiesToInclude)
    : m_shouldUseFixedDefaultFontSize(false)
    , m_fontSizeDelta(NoFontDelta)
{
    init(position.deprecatedNode(), propertiesToInclude);
}

EditingStyle::EditingStyle(const CSSStyleDeclaration* style)
    : m_mutableStyle(style ? style->copy() : 0)
    , m_shouldUseFixedDefaultFontSize(false)
    , m_fontSizeDelta(NoFontDelta)
{
    extractFontSizeDelta();
}

EditingStyle::EditingStyle(int propertyID, const String& value)
    : m_mutableStyle(0)
    , m_shouldUseFixedDefaultFontSize(false)
    , m_fontSizeDelta(NoFontDelta)
{
    setProperty(propertyID, value);
}

EditingStyle::~EditingStyle()
{
}

static RGBA32 cssValueToRGBA(CSSValue* colorValue)
{
    if (!colorValue || !colorValue->isPrimitiveValue())
        return Color::transparent;
    
    CSSPrimitiveValue* primitiveColor = static_cast<CSSPrimitiveValue*>(colorValue);
    if (primitiveColor->isRGBColor())
        return primitiveColor->getRGBA32Value();
    
    RGBA32 rgba = 0;
    CSSParser::parseColor(rgba, colorValue->cssText());
    return rgba;
}

static inline RGBA32 getRGBAFontColor(CSSStyleDeclaration* style)
{
    return cssValueToRGBA(style->getPropertyCSSValueInternal(CSSPropertyColor).get());
}

static inline RGBA32 rgbaBackgroundColorInEffect(Node* node)
{
    return cssValueToRGBA(backgroundColorInEffect(node).get());
}

void EditingStyle::init(Node* node, PropertiesToInclude propertiesToInclude)
{
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node)->parentNode();
    else if (isTabSpanNode(node))
        node = node->parentNode();

    RefPtr<CSSComputedStyleDeclaration> computedStyleAtPosition = computedStyle(node);
    m_mutableStyle = propertiesToInclude == AllProperties && computedStyleAtPosition ? computedStyleAtPosition->copy() : editingStyleFromComputedStyle(computedStyleAtPosition);

    if (propertiesToInclude == EditingPropertiesInEffect) {
        if (RefPtr<CSSValue> value = backgroundColorInEffect(node))
            m_mutableStyle->setProperty(CSSPropertyBackgroundColor, value->cssText());
        if (RefPtr<CSSValue> value = computedStyleAtPosition->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect))
            m_mutableStyle->setProperty(CSSPropertyTextDecoration, value->cssText());
    }

    if (node && node->computedStyle()) {
        RenderStyle* renderStyle = node->computedStyle();
        removeTextFillAndStrokeColorsIfNeeded(renderStyle);
        replaceFontSizeByKeywordIfPossible(renderStyle, computedStyleAtPosition.get());
    }

    m_shouldUseFixedDefaultFontSize = computedStyleAtPosition->useFixedFontDefaultSize();
    extractFontSizeDelta();
}

void EditingStyle::removeTextFillAndStrokeColorsIfNeeded(RenderStyle* renderStyle)
{
    // If a node's text fill color is invalid, then its children use 
    // their font-color as their text fill color (they don't
    // inherit it).  Likewise for stroke color.
    if (!renderStyle->textFillColor().isValid())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextFillColor);
    if (!renderStyle->textStrokeColor().isValid())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextStrokeColor);
}

void EditingStyle::setProperty(int propertyID, const String& value, bool important)
{
    if (!m_mutableStyle)
        m_mutableStyle = CSSMutableStyleDeclaration::create();

    m_mutableStyle->setProperty(propertyID, value, important);
}

void EditingStyle::replaceFontSizeByKeywordIfPossible(RenderStyle* renderStyle, CSSComputedStyleDeclaration* computedStyle)
{
    ASSERT(renderStyle);
    if (renderStyle->fontDescription().keywordSize())
        m_mutableStyle->setProperty(CSSPropertyFontSize, computedStyle->getFontSizeCSSValuePreferringKeyword()->cssText());
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
    if (!value || !value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value.get());

    // Only PX handled now. If we handle more types in the future, perhaps
    // a switch statement here would be more appropriate.
    if (!primitiveValue->isPx())
        return;

    m_fontSizeDelta = primitiveValue->getFloatValue();
    m_mutableStyle->removeProperty(CSSPropertyWebkitFontSizeDelta);
}

bool EditingStyle::isEmpty() const
{
    return (!m_mutableStyle || m_mutableStyle->isEmpty()) && m_fontSizeDelta == NoFontDelta;
}

bool EditingStyle::textDirection(WritingDirection& writingDirection) const
{
    if (!m_mutableStyle)
        return false;

    RefPtr<CSSValue> unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    if (!unicodeBidi || !unicodeBidi->isPrimitiveValue())
        return false;

    int unicodeBidiValue = static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent();
    if (unicodeBidiValue == CSSValueEmbed) {
        RefPtr<CSSValue> direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
        if (!direction || !direction->isPrimitiveValue())
            return false;

        writingDirection = static_cast<CSSPrimitiveValue*>(direction.get())->getIdent() == CSSValueLtr ? LeftToRightWritingDirection : RightToLeftWritingDirection;

        return true;
    }

    if (unicodeBidiValue == CSSValueNormal) {
        writingDirection = NaturalWritingDirection;
        return true;
    }

    return false;
}

void EditingStyle::setStyle(PassRefPtr<CSSMutableStyleDeclaration> style)
{
    m_mutableStyle = style;
    // FIXME: We should be able to figure out whether or not font is fixed width for mutable style.
    // We need to check font-family is monospace as in FontDescription but we don't want to duplicate code here.
    m_shouldUseFixedDefaultFontSize = false;
    extractFontSizeDelta();
}

void EditingStyle::overrideWithStyle(const CSSMutableStyleDeclaration* style)
{
    if (!style || style->isEmpty())
        return;
    if (!m_mutableStyle)
        m_mutableStyle = CSSMutableStyleDeclaration::create();
    m_mutableStyle->merge(style);
    extractFontSizeDelta();
}

void EditingStyle::clear()
{
    m_mutableStyle.clear();
    m_shouldUseFixedDefaultFontSize = false;
    m_fontSizeDelta = NoFontDelta;
}

PassRefPtr<EditingStyle> EditingStyle::copy() const
{
    RefPtr<EditingStyle> copy = EditingStyle::create();
    if (m_mutableStyle)
        copy->m_mutableStyle = m_mutableStyle->copy();
    copy->m_shouldUseFixedDefaultFontSize = m_shouldUseFixedDefaultFontSize;
    copy->m_fontSizeDelta = m_fontSizeDelta;
    return copy;
}

PassRefPtr<EditingStyle> EditingStyle::extractAndRemoveBlockProperties()
{
    RefPtr<EditingStyle> blockProperties = EditingStyle::create();
    if (!m_mutableStyle)
        return blockProperties;

    blockProperties->m_mutableStyle = m_mutableStyle->copyBlockProperties();
    m_mutableStyle->removeBlockProperties();

    return blockProperties;
}

PassRefPtr<EditingStyle> EditingStyle::extractAndRemoveTextDirection()
{
    RefPtr<EditingStyle> textDirection = EditingStyle::create();
    textDirection->m_mutableStyle = CSSMutableStyleDeclaration::create();
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

    m_mutableStyle->removeBlockProperties();
}

void EditingStyle::removeStyleAddedByNode(Node* node)
{
    if (!node || !node->parentNode())
        return;
    RefPtr<CSSMutableStyleDeclaration> parentStyle = editingStyleFromComputedStyle(computedStyle(node->parentNode()), AllEditingProperties);
    RefPtr<CSSMutableStyleDeclaration> nodeStyle = editingStyleFromComputedStyle(computedStyle(node), AllEditingProperties);
    nodeStyle->removeEquivalentProperties(parentStyle.get());
    m_mutableStyle->removeEquivalentProperties(nodeStyle.get());
}

void EditingStyle::removeStyleConflictingWithStyleOfNode(Node* node)
{
    if (!node || !node->parentNode() || !m_mutableStyle)
        return;

    RefPtr<CSSMutableStyleDeclaration> parentStyle = editingStyleFromComputedStyle(computedStyle(node->parentNode()), AllEditingProperties);
    RefPtr<CSSMutableStyleDeclaration> nodeStyle = editingStyleFromComputedStyle(computedStyle(node), AllEditingProperties);
    nodeStyle->removeEquivalentProperties(parentStyle.get());

    unsigned propertyCount = nodeStyle->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i)
        m_mutableStyle->removeProperty(nodeStyle->propertyAt(i).id());
}

void EditingStyle::removeNonEditingProperties()
{
    if (m_mutableStyle)
        m_mutableStyle = copyEditingProperties(m_mutableStyle.get());
}

void EditingStyle::collapseTextDecorationProperties()
{
    if (!m_mutableStyle)
        return;

    RefPtr<CSSValue> textDecorationsInEffect = m_mutableStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (!textDecorationsInEffect)
        return;

    if (textDecorationsInEffect->isValueList())
        m_mutableStyle->setProperty(CSSPropertyTextDecoration, textDecorationsInEffect->cssText(), m_mutableStyle->propertyIsImportant(CSSPropertyTextDecoration));
    else
        m_mutableStyle->removeProperty(CSSPropertyTextDecoration);
    m_mutableStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
}

// CSS properties that create a visual difference only when applied to text.
static const int textOnlyProperties[] = {
    CSSPropertyTextDecoration,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyFontStyle,
    CSSPropertyFontWeight,
    CSSPropertyColor,
};

TriState EditingStyle::triStateOfStyle(EditingStyle* style) const
{
    if (!style || !style->m_mutableStyle)
        return FalseTriState;
    return triStateOfStyle(style->m_mutableStyle.get(), DoNotIgnoreTextOnlyProperties);
}

TriState EditingStyle::triStateOfStyle(CSSStyleDeclaration* styleToCompare, ShouldIgnoreTextOnlyProperties shouldIgnoreTextOnlyProperties) const
{
    RefPtr<CSSMutableStyleDeclaration> difference = getPropertiesNotIn(m_mutableStyle.get(), styleToCompare);

    if (shouldIgnoreTextOnlyProperties == IgnoreTextOnlyProperties)
        difference->removePropertiesInSet(textOnlyProperties, WTF_ARRAY_LENGTH(textOnlyProperties));

    if (difference->isEmpty())
        return TrueTriState;
    if (difference->propertyCount() == m_mutableStyle->propertyCount())
        return FalseTriState;

    return MixedTriState;
}

TriState EditingStyle::triStateOfStyle(const VisibleSelection& selection) const
{
    if (!selection.isCaretOrRange())
        return FalseTriState;

    if (selection.isCaret())
        return triStateOfStyle(EditingStyle::styleAtSelectionStart(selection).get());

    TriState state = FalseTriState;
    for (Node* node = selection.start().deprecatedNode(); node; node = node->traverseNextNode()) {
        RefPtr<CSSComputedStyleDeclaration> nodeStyle = computedStyle(node);
        if (nodeStyle) {
            TriState nodeState = triStateOfStyle(nodeStyle.get(), node->isTextNode() ? EditingStyle::DoNotIgnoreTextOnlyProperties : EditingStyle::IgnoreTextOnlyProperties);
            if (node == selection.start().deprecatedNode())
                state = nodeState;
            else if (state != nodeState && node->isTextNode()) {
                state = MixedTriState;
                break;
            }
        }
        if (node == selection.end().deprecatedNode())
            break;
    }

    return state;
}

bool EditingStyle::conflictsWithInlineStyleOfElement(StyledElement* element, EditingStyle* extractedStyle, Vector<CSSPropertyID>* conflictingProperties) const
{
    ASSERT(element);
    ASSERT(!conflictingProperties || conflictingProperties->isEmpty());

    CSSMutableStyleDeclaration* inlineStyle = element->inlineStyleDecl();
    if (!m_mutableStyle || !inlineStyle)
        return false;

    unsigned propertyCount = m_mutableStyle->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(m_mutableStyle->propertyAt(i).id());

        // We don't override whitespace property of a tab span because that would collapse the tab into a space.
        if (propertyID == CSSPropertyWhiteSpace && isTabSpanNode(element))
            continue;

        if (propertyID == CSSPropertyWebkitTextDecorationsInEffect && inlineStyle->getPropertyCSSValue(CSSPropertyTextDecoration)) {
            if (!conflictingProperties)
                return true;
            conflictingProperties->append(CSSPropertyTextDecoration);
            if (extractedStyle)
                extractedStyle->setProperty(CSSPropertyTextDecoration, inlineStyle->getPropertyValue(CSSPropertyTextDecoration), inlineStyle->propertyIsImportant(CSSPropertyTextDecoration));
            continue;
        }

        if (!inlineStyle->getPropertyCSSValue(propertyID))
            continue;

        if (propertyID == CSSPropertyUnicodeBidi && inlineStyle->getPropertyCSSValue(CSSPropertyDirection)) {
            if (!conflictingProperties)
                return true;
            conflictingProperties->append(CSSPropertyDirection);
            if (extractedStyle)
                extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
        }

        if (!conflictingProperties)
            return true;

        conflictingProperties->append(propertyID);

        if (extractedStyle)
            extractedStyle->setProperty(propertyID, inlineStyle->getPropertyValue(propertyID), inlineStyle->propertyIsImportant(propertyID));
    }

    return conflictingProperties && !conflictingProperties->isEmpty();
}

static const Vector<OwnPtr<HTMLElementEquivalent> >& htmlElementEquivalents()
{
    DEFINE_STATIC_LOCAL(Vector<OwnPtr<HTMLElementEquivalent> >, HTMLElementEquivalents, ());

    if (!HTMLElementEquivalents.size()) {
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontWeight, CSSValueBold, HTMLNames::bTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontWeight, CSSValueBold, HTMLNames::strongTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyVerticalAlign, CSSValueSub, HTMLNames::subTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyVerticalAlign, CSSValueSuper, HTMLNames::supTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::iTag));
        HTMLElementEquivalents.append(HTMLElementEquivalent::create(CSSPropertyFontStyle, CSSValueItalic, HTMLNames::emTag));

        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueUnderline, HTMLNames::uTag));
        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueLineThrough, HTMLNames::sTag));
        HTMLElementEquivalents.append(HTMLTextDecorationEquivalent::create(CSSValueLineThrough, HTMLNames::strikeTag));
    }

    return HTMLElementEquivalents;
}


bool EditingStyle::conflictsWithImplicitStyleOfElement(HTMLElement* element, EditingStyle* extractedStyle, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    if (!m_mutableStyle)
        return false;

    const Vector<OwnPtr<HTMLElementEquivalent> >& HTMLElementEquivalents = htmlElementEquivalents();
    for (size_t i = 0; i < HTMLElementEquivalents.size(); ++i) {
        const HTMLElementEquivalent* equivalent = HTMLElementEquivalents[i].get();
        if (equivalent->matches(element) && equivalent->propertyExistsInStyle(m_mutableStyle.get())
            && (shouldExtractMatchingStyle == ExtractMatchingStyle || !equivalent->valueIsPresentInStyle(element, m_mutableStyle.get()))) {
            if (extractedStyle)
                equivalent->addToStyle(element, extractedStyle);
            return true;
        }
    }
    return false;
}

static const Vector<OwnPtr<HTMLAttributeEquivalent> >& htmlAttributeEquivalents()
{
    DEFINE_STATIC_LOCAL(Vector<OwnPtr<HTMLAttributeEquivalent> >, HTMLAttributeEquivalents, ());

    if (!HTMLAttributeEquivalents.size()) {
        // elementIsStyledSpanOrHTMLEquivalent depends on the fact each HTMLAttriuteEquivalent matches exactly one attribute
        // of exactly one element except dirAttr.
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyColor, HTMLNames::fontTag, HTMLNames::colorAttr));
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyFontFamily, HTMLNames::fontTag, HTMLNames::faceAttr));
        HTMLAttributeEquivalents.append(HTMLFontSizeEquivalent::create());

        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyDirection, HTMLNames::dirAttr));
        HTMLAttributeEquivalents.append(HTMLAttributeEquivalent::create(CSSPropertyUnicodeBidi, HTMLNames::dirAttr));
    }

    return HTMLAttributeEquivalents;
}

bool EditingStyle::conflictsWithImplicitStyleOfAttributes(HTMLElement* element) const
{
    ASSERT(element);
    if (!m_mutableStyle)
        return false;

    const Vector<OwnPtr<HTMLAttributeEquivalent> >& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    for (size_t i = 0; i < HTMLAttributeEquivalents.size(); ++i) {
        if (HTMLAttributeEquivalents[i]->matches(element) && HTMLAttributeEquivalents[i]->propertyExistsInStyle(m_mutableStyle.get())
            && !HTMLAttributeEquivalents[i]->valueIsPresentInStyle(element, m_mutableStyle.get()))
            return true;
    }

    return false;
}

bool EditingStyle::extractConflictingImplicitStyleOfAttributes(HTMLElement* element, ShouldPreserveWritingDirection shouldPreserveWritingDirection,
    EditingStyle* extractedStyle, Vector<QualifiedName>& conflictingAttributes, ShouldExtractMatchingStyle shouldExtractMatchingStyle) const
{
    ASSERT(element);
    // HTMLAttributeEquivalent::addToStyle doesn't support unicode-bidi and direction properties
    ASSERT(!extractedStyle || shouldPreserveWritingDirection == PreserveWritingDirection);
    if (!m_mutableStyle)
        return false;

    const Vector<OwnPtr<HTMLAttributeEquivalent> >& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    bool removed = false;
    for (size_t i = 0; i < HTMLAttributeEquivalents.size(); ++i) {
        const HTMLAttributeEquivalent* equivalent = HTMLAttributeEquivalents[i].get();

        // unicode-bidi and direction are pushed down separately so don't push down with other styles.
        if (shouldPreserveWritingDirection == PreserveWritingDirection && equivalent->attributeName() == HTMLNames::dirAttr)
            continue;

        if (!equivalent->matches(element) || !equivalent->propertyExistsInStyle(m_mutableStyle.get())
            || (shouldExtractMatchingStyle == DoNotExtractMatchingStyle && equivalent->valueIsPresentInStyle(element, m_mutableStyle.get())))
            continue;

        if (extractedStyle)
            equivalent->addToStyle(element, extractedStyle);
        conflictingAttributes.append(equivalent->attributeName());
        removed = true;
    }

    return removed;
}

bool EditingStyle::styleIsPresentInComputedStyleOfNode(Node* node) const
{
    return !m_mutableStyle || getPropertiesNotIn(m_mutableStyle.get(), computedStyle(node).get())->isEmpty();
}

bool EditingStyle::elementIsStyledSpanOrHTMLEquivalent(const HTMLElement* element)
{
    bool elementIsSpanOrElementEquivalent = false;
    if (element->hasTagName(HTMLNames::spanTag))
        elementIsSpanOrElementEquivalent = true;
    else {
        const Vector<OwnPtr<HTMLElementEquivalent> >& HTMLElementEquivalents = htmlElementEquivalents();
        size_t i;
        for (i = 0; i < HTMLElementEquivalents.size(); ++i) {
            if (HTMLElementEquivalents[i]->matches(element)) {
                elementIsSpanOrElementEquivalent = true;
                break;
            }
        }
    }

    const NamedNodeMap* attributeMap = element->attributeMap();
    if (!attributeMap || attributeMap->isEmpty())
        return elementIsSpanOrElementEquivalent; // span, b, etc... without any attributes

    unsigned matchedAttributes = 0;
    const Vector<OwnPtr<HTMLAttributeEquivalent> >& HTMLAttributeEquivalents = htmlAttributeEquivalents();
    for (size_t i = 0; i < HTMLAttributeEquivalents.size(); ++i) {
        if (HTMLAttributeEquivalents[i]->matches(element) && HTMLAttributeEquivalents[i]->attributeName() != HTMLNames::dirAttr)
            matchedAttributes++;
    }

    if (!elementIsSpanOrElementEquivalent && !matchedAttributes)
        return false; // element is not a span, a html element equivalent, or font element.
    
    if (element->getAttribute(HTMLNames::classAttr) == AppleStyleSpanClass)
        matchedAttributes++;

    if (element->hasAttribute(HTMLNames::styleAttr)) {
        if (CSSMutableStyleDeclaration* style = element->inlineStyleDecl()) {
            unsigned propertyCount = style->propertyCount();
            for (unsigned i = 0; i < propertyCount; ++i) {
                if (!isEditingProperty(style->propertyAt(i).id()))
                    return false;
            }
        }
        matchedAttributes++;
    }

    // font with color attribute, span with style attribute, etc...
    ASSERT(matchedAttributes <= attributeMap->length());
    return matchedAttributes >= attributeMap->length();
}

void EditingStyle::prepareToApplyAt(const Position& position, ShouldPreserveWritingDirection shouldPreserveWritingDirection)
{
    if (!m_mutableStyle)
        return;

    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    RefPtr<EditingStyle> style = EditingStyle::create(position, EditingPropertiesInEffect);

    RefPtr<CSSValue> unicodeBidi;
    RefPtr<CSSValue> direction;
    if (shouldPreserveWritingDirection == PreserveWritingDirection) {
        unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
    }

    m_mutableStyle->removeEquivalentProperties(style->m_mutableStyle.get());

    if (getRGBAFontColor(m_mutableStyle.get()) == getRGBAFontColor(style->m_mutableStyle.get()))
        m_mutableStyle->removeProperty(CSSPropertyColor);

    if (hasTransparentBackgroundColor(m_mutableStyle.get())
        || cssValueToRGBA(m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor).get()) == rgbaBackgroundColorInEffect(position.containerNode()))
        m_mutableStyle->removeProperty(CSSPropertyBackgroundColor);

    if (unicodeBidi && unicodeBidi->isPrimitiveValue()) {
        m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent());
        if (direction && direction->isPrimitiveValue())
            m_mutableStyle->setProperty(CSSPropertyDirection, static_cast<CSSPrimitiveValue*>(direction.get())->getIdent());
    }
}

void EditingStyle::mergeTypingStyle(Document* document)
{
    ASSERT(document);

    RefPtr<EditingStyle> typingStyle = document->frame()->selection()->typingStyle();
    if (!typingStyle || typingStyle == this)
        return;

    mergeStyle(typingStyle->style(), OverrideValues);
}

void EditingStyle::mergeInlineStyleOfElement(StyledElement* element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude)
{
    ASSERT(element);
    if (!element->inlineStyleDecl())
        return;

    switch (propertiesToInclude) {
    case AllProperties:
        mergeStyle(element->inlineStyleDecl(), mode);
        return;
    case OnlyEditingInheritableProperties:
        mergeStyle(copyEditingProperties(element->inlineStyleDecl(), OnlyInheritableEditingProperties).get(), mode);
        return;
    case EditingPropertiesInEffect:
        mergeStyle(copyEditingProperties(element->inlineStyleDecl(), AllEditingProperties).get(), mode);
        return;
    }
}

static inline bool elementMatchesAndPropertyIsNotInInlineStyleDecl(const HTMLElementEquivalent* equivalent, const StyledElement* element,
    EditingStyle::CSSPropertyOverrideMode mode, CSSMutableStyleDeclaration* style)
{
    return equivalent->matches(element) && !equivalent->propertyExistsInStyle(element->inlineStyleDecl())
        && (mode == EditingStyle::OverrideValues || !equivalent->propertyExistsInStyle(style));
}

void EditingStyle::mergeInlineAndImplicitStyleOfElement(StyledElement* element, CSSPropertyOverrideMode mode, PropertiesToInclude propertiesToInclude)
{
    mergeInlineStyleOfElement(element, mode, propertiesToInclude);

    const Vector<OwnPtr<HTMLElementEquivalent> >& elementEquivalents = htmlElementEquivalents();
    for (size_t i = 0; i < elementEquivalents.size(); ++i) {
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(elementEquivalents[i].get(), element, mode, m_mutableStyle.get()))
            elementEquivalents[i]->addToStyle(element, this);
    }

    const Vector<OwnPtr<HTMLAttributeEquivalent> >& attributeEquivalents = htmlAttributeEquivalents();
    for (size_t i = 0; i < attributeEquivalents.size(); ++i) {
        if (attributeEquivalents[i]->attributeName() == HTMLNames::dirAttr)
            continue; // We don't want to include directionality
        if (elementMatchesAndPropertyIsNotInInlineStyleDecl(attributeEquivalents[i].get(), element, mode, m_mutableStyle.get()))
            attributeEquivalents[i]->addToStyle(element, this);
    }
}

PassRefPtr<EditingStyle> EditingStyle::wrappingStyleForSerialization(Node* context, bool shouldAnnotate)
{
    RefPtr<EditingStyle> wrappingStyle;
    if (shouldAnnotate) {
        wrappingStyle = EditingStyle::create(context, EditingStyle::EditingPropertiesInEffect);

        // Styles that Mail blockquotes contribute should only be placed on the Mail blockquote,
        // to help us differentiate those styles from ones that the user has applied.
        // This helps us get the color of content pasted into blockquotes right.
        wrappingStyle->removeStyleAddedByNode(enclosingNodeOfType(firstPositionInOrBeforeNode(context), isMailBlockquote, CanCrossEditingBoundary));

        // Call collapseTextDecorationProperties first or otherwise it'll copy the value over from in-effect to text-decorations.
        wrappingStyle->collapseTextDecorationProperties();
        
        return wrappingStyle.release();
    }

    wrappingStyle = EditingStyle::create();

    // When not annotating for interchange, we only preserve inline style declarations.
    for (Node* node = context; node && !node->isDocumentNode(); node = node->parentNode()) {
        if (node->isStyledElement()) {
            wrappingStyle->mergeInlineAndImplicitStyleOfElement(static_cast<StyledElement*>(node), EditingStyle::DoNotOverrideValues,
                EditingStyle::EditingPropertiesInEffect);
        }
    }

    return wrappingStyle.release();
}


static void mergeTextDecorationValues(CSSValueList* mergedValue, const CSSValueList* valueToMerge)
{
    DEFINE_STATIC_LOCAL(const RefPtr<CSSPrimitiveValue>, underline, (CSSPrimitiveValue::createIdentifier(CSSValueUnderline)));
    DEFINE_STATIC_LOCAL(const RefPtr<CSSPrimitiveValue>, lineThrough, (CSSPrimitiveValue::createIdentifier(CSSValueLineThrough)));

    if (valueToMerge->hasValue(underline.get()) && !mergedValue->hasValue(underline.get()))
        mergedValue->append(underline.get());

    if (valueToMerge->hasValue(lineThrough.get()) && !mergedValue->hasValue(lineThrough.get()))
        mergedValue->append(lineThrough.get());
}

void EditingStyle::mergeStyle(CSSMutableStyleDeclaration* style, CSSPropertyOverrideMode mode)
{
    if (!style)
        return;

    if (!m_mutableStyle) {
        m_mutableStyle = style->copy();
        return;
    }

    unsigned propertyCount = style->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        const CSSProperty& property = style->propertyAt(i);
        RefPtr<CSSValue> value = m_mutableStyle->getPropertyCSSValue(property.id());

        // text decorations never override values
        if ((property.id() == CSSPropertyTextDecoration || property.id() == CSSPropertyWebkitTextDecorationsInEffect) && property.value()->isValueList() && value) {
            if (value->isValueList()) {
                mergeTextDecorationValues(static_cast<CSSValueList*>(value.get()), static_cast<CSSValueList*>(property.value()));
                continue;
            }
            value = 0; // text-decoration: none is equivalent to not having the property
        }

        if (mode == OverrideValues || (mode == DoNotOverrideValues && !value))
            m_mutableStyle->setProperty(property.id(), property.value()->cssText(), property.isImportant());
    }
}

static PassRefPtr<CSSMutableStyleDeclaration> styleFromMatchedRulesForElement(Element* element, unsigned rulesToInclude)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    RefPtr<CSSRuleList> matchedRules = element->document()->styleSelector()->styleRulesForElement(element, rulesToInclude);
    if (matchedRules) {
        for (unsigned i = 0; i < matchedRules->length(); i++) {
            if (matchedRules->item(i)->type() == CSSRule::STYLE_RULE) {
                RefPtr<CSSMutableStyleDeclaration> s = static_cast<CSSStyleRule*>(matchedRules->item(i))->declaration();
                style->merge(s.get(), true);
            }
        }
    }
    
    return style.release();
}

void EditingStyle::mergeStyleFromRules(StyledElement* element)
{
    RefPtr<CSSMutableStyleDeclaration> styleFromMatchedRules = styleFromMatchedRulesForElement(element,
        CSSStyleSelector::AuthorCSSRules | CSSStyleSelector::CrossOriginCSSRules);
    // Styles from the inline style declaration, held in the variable "style", take precedence 
    // over those from matched rules.
    if (m_mutableStyle)
        styleFromMatchedRules->merge(m_mutableStyle.get());

    clear();
    m_mutableStyle = styleFromMatchedRules;
}

void EditingStyle::mergeStyleFromRulesForSerialization(StyledElement* element)
{
    mergeStyleFromRules(element);

    // The property value, if it's a percentage, may not reflect the actual computed value.  
    // For example: style="height: 1%; overflow: visible;" in quirksmode
    // FIXME: There are others like this, see <rdar://problem/5195123> Slashdot copy/paste fidelity problem
    RefPtr<CSSComputedStyleDeclaration> computedStyleForElement = computedStyle(element);
    RefPtr<CSSMutableStyleDeclaration> fromComputedStyle = CSSMutableStyleDeclaration::create();
    {
        unsigned propertyCount = m_mutableStyle->propertyCount();
        for (unsigned i = 0; i < propertyCount; ++i) {
            const CSSProperty& property = m_mutableStyle->propertyAt(i);
            CSSValue* value = property.value();
            if (!value->isPrimitiveValue())
                continue;
            if (static_cast<CSSPrimitiveValue*>(value)->isPercentage()) {
                if (RefPtr<CSSValue> computedPropertyValue = computedStyleForElement->getPropertyCSSValue(property.id()))
                    fromComputedStyle->addParsedProperty(CSSProperty(property.id(), computedPropertyValue));
            }
        }
    }
    m_mutableStyle->merge(fromComputedStyle.get());
}

static void removePropertiesInStyle(CSSMutableStyleDeclaration* styleToRemovePropertiesFrom, CSSMutableStyleDeclaration* style)
{
    unsigned propertyCount = style->propertyCount();
    Vector<int> propertiesToRemove(propertyCount);
    for (unsigned i = 0; i < propertyCount; ++i)
        propertiesToRemove[i] = style->propertyAt(i).id();

    styleToRemovePropertiesFrom->removePropertiesInSet(propertiesToRemove.data(), propertiesToRemove.size());
}

void EditingStyle::removeStyleFromRulesAndContext(StyledElement* element, Node* context)
{
    ASSERT(element);
    if (!m_mutableStyle)
        return;

    // 1. Remove style from matched rules because style remain without repeating it in inline style declaration
    RefPtr<CSSMutableStyleDeclaration> styleFromMatchedRules = styleFromMatchedRulesForElement(element, CSSStyleSelector::AllButEmptyCSSRules);
    if (styleFromMatchedRules && !styleFromMatchedRules->isEmpty())
        m_mutableStyle = getPropertiesNotIn(m_mutableStyle.get(), styleFromMatchedRules.get());

    // 2. Remove style present in context and not overriden by matched rules.
    RefPtr<EditingStyle> computedStyle = EditingStyle::create(context, EditingPropertiesInEffect);
    if (computedStyle->m_mutableStyle) {
        if (!computedStyle->m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor))
            computedStyle->m_mutableStyle->setProperty(CSSPropertyBackgroundColor, CSSValueTransparent);

        removePropertiesInStyle(computedStyle->m_mutableStyle.get(), styleFromMatchedRules.get());
        m_mutableStyle = getPropertiesNotIn(m_mutableStyle.get(), computedStyle->m_mutableStyle.get());
    }

    // 3. If this element is a span and has display: inline or float: none, remove them unless they are overriden by rules.
    // These rules are added by serialization code to wrap text nodes.
    if (isStyleSpanOrSpanWithOnlyStyleAttribute(element)) {
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyDisplay) && getIdentifierValue(m_mutableStyle.get(), CSSPropertyDisplay) == CSSValueInline)
            m_mutableStyle->removeProperty(CSSPropertyDisplay);
        if (!styleFromMatchedRules->getPropertyCSSValue(CSSPropertyFloat) && getIdentifierValue(m_mutableStyle.get(), CSSPropertyFloat) == CSSValueNone)
            m_mutableStyle->removeProperty(CSSPropertyFloat);
    }
}

void EditingStyle::removePropertiesInElementDefaultStyle(Element* element)
{
    if (!m_mutableStyle || m_mutableStyle->isEmpty())
        return;

    RefPtr<CSSMutableStyleDeclaration> defaultStyle = styleFromMatchedRulesForElement(element, CSSStyleSelector::UAAndUserCSSRules);

    removePropertiesInStyle(m_mutableStyle.get(), defaultStyle.get());
}

void EditingStyle::forceInline()
{
    if (!m_mutableStyle)
        m_mutableStyle = CSSMutableStyleDeclaration::create();
    const bool propertyIsImportant = true;
    m_mutableStyle->setProperty(CSSPropertyDisplay, CSSValueInline, propertyIsImportant);
}

int EditingStyle::legacyFontSize(Document* document) const
{
    RefPtr<CSSValue> cssValue = m_mutableStyle->getPropertyCSSValue(CSSPropertyFontSize);
    if (!cssValue || !cssValue->isPrimitiveValue())
        return 0;
    return legacyFontSizeFromCSSValue(document, static_cast<CSSPrimitiveValue*>(cssValue.get()),
        m_shouldUseFixedDefaultFontSize, AlwaysUseLegacyFontSize);
}

PassRefPtr<EditingStyle> EditingStyle::styleAtSelectionStart(const VisibleSelection& selection, bool shouldUseBackgroundColorInEffect)
{
    if (selection.isNone())
        return 0;

    Position position = adjustedSelectionStartForStyleComputation(selection);

    // If the pos is at the end of a text node, then this node is not fully selected. 
    // Move it to the next deep equivalent position to avoid removing the style from this node. 
    // e.g. if pos was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead. 
    // We only do this for range because caret at Position("hello", 5) in <b>hello</b>world should give you font-weight: bold. 
    Node* positionNode = position.containerNode(); 
    if (selection.isRange() && positionNode && positionNode->isTextNode() && position.computeOffsetInContainerNode() == positionNode->maxCharacterOffset()) 
        position = nextVisuallyDistinctCandidate(position); 

    Element* element = position.element();
    if (!element)
        return 0;

    RefPtr<EditingStyle> style = EditingStyle::create(element, EditingStyle::AllProperties);
    style->mergeTypingStyle(element->document());

    // If background color is transparent, traverse parent nodes until we hit a different value or document root
    // Also, if the selection is a range, ignore the background color at the start of selection,
    // and find the background color of the common ancestor.
    if (shouldUseBackgroundColorInEffect && (selection.isRange() || hasTransparentBackgroundColor(style->m_mutableStyle.get()))) {
        RefPtr<Range> range(selection.toNormalizedRange());
        ExceptionCode ec = 0;
        if (PassRefPtr<CSSValue> value = backgroundColorInEffect(range->commonAncestorContainer(ec)))
            style->setProperty(CSSPropertyBackgroundColor, value->cssText());
    }

    return style;
}

static void reconcileTextDecorationProperties(CSSMutableStyleDeclaration* style)
{    
    RefPtr<CSSValue> textDecorationsInEffect = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    RefPtr<CSSValue> textDecoration = style->getPropertyCSSValue(CSSPropertyTextDecoration);
    // We shouldn't have both text-decoration and -webkit-text-decorations-in-effect because that wouldn't make sense.
    ASSERT(!textDecorationsInEffect || !textDecoration);
    if (textDecorationsInEffect) {
        style->setProperty(CSSPropertyTextDecoration, textDecorationsInEffect->cssText());
        style->removeProperty(CSSPropertyWebkitTextDecorationsInEffect);
        textDecoration = textDecorationsInEffect;
    }

    // If text-decoration is set to "none", remove the property because we don't want to add redundant "text-decoration: none".
    if (textDecoration && !textDecoration->isValueList())
        style->removeProperty(CSSPropertyTextDecoration);
}

StyleChange::StyleChange(EditingStyle* style, const Position& position)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applyUnderline(false)
    , m_applyLineThrough(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
{
    Document* document = position.anchorNode() ? position.anchorNode()->document() : 0;
    if (!style || !style->style() || !document || !document->frame())
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyle = position.computedStyle();
    // FIXME: take care of background-color in effect
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = getPropertiesNotIn(style->style(), computedStyle.get());

    reconcileTextDecorationProperties(mutableStyle.get());
    if (!document->frame()->editor()->shouldStyleWithCSS())
        extractTextStyles(document, mutableStyle.get(), computedStyle->useFixedFontDefaultSize());

    // Changing the whitespace style in a tab span would collapse the tab into a space.
    if (isTabSpanTextNode(position.deprecatedNode()) || isTabSpanNode((position.deprecatedNode())))
        mutableStyle->removeProperty(CSSPropertyWhiteSpace);

    // If unicode-bidi is present in mutableStyle and direction is not, then add direction to mutableStyle.
    // FIXME: Shouldn't this be done in getPropertiesNotIn?
    if (mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi) && !style->style()->getPropertyCSSValue(CSSPropertyDirection))
        mutableStyle->setProperty(CSSPropertyDirection, style->style()->getPropertyValue(CSSPropertyDirection));

    // Save the result for later
    m_cssStyle = mutableStyle->asText().stripWhiteSpace();
}

static void setTextDecorationProperty(CSSMutableStyleDeclaration* style, const CSSValueList* newTextDecoration, int propertyID)
{
    if (newTextDecoration->length())
        style->setProperty(propertyID, newTextDecoration->cssText(), style->propertyIsImportant(propertyID));
    else {
        // text-decoration: none is redundant since it does not remove any text decorations.
        ASSERT(!style->propertyIsImportant(propertyID));
        style->removeProperty(propertyID);
    }
}

void StyleChange::extractTextStyles(Document* document, CSSMutableStyleDeclaration* style, bool shouldUseFixedFontDefaultSize)
{
    ASSERT(style);

    if (getIdentifierValue(style, CSSPropertyFontWeight) == CSSValueBold) {
        style->removeProperty(CSSPropertyFontWeight);
        m_applyBold = true;
    }

    int fontStyle = getIdentifierValue(style, CSSPropertyFontStyle);
    if (fontStyle == CSSValueItalic || fontStyle == CSSValueOblique) {
        style->removeProperty(CSSPropertyFontStyle);
        m_applyItalic = true;
    }

    // Assuming reconcileTextDecorationProperties has been called, there should not be -webkit-text-decorations-in-effect
    // Furthermore, text-decoration: none has been trimmed so that text-decoration property is always a CSSValueList.
    RefPtr<CSSValue> textDecoration = style->getPropertyCSSValue(CSSPropertyTextDecoration);
    if (textDecoration && textDecoration->isValueList()) {
        DEFINE_STATIC_LOCAL(RefPtr<CSSPrimitiveValue>, underline, (CSSPrimitiveValue::createIdentifier(CSSValueUnderline)));
        DEFINE_STATIC_LOCAL(RefPtr<CSSPrimitiveValue>, lineThrough, (CSSPrimitiveValue::createIdentifier(CSSValueLineThrough)));

        RefPtr<CSSValueList> newTextDecoration = static_cast<CSSValueList*>(textDecoration.get())->copy();
        if (newTextDecoration->removeAll(underline.get()))
            m_applyUnderline = true;
        if (newTextDecoration->removeAll(lineThrough.get()))
            m_applyLineThrough = true;

        // If trimTextDecorations, delete underline and line-through
        setTextDecorationProperty(style, newTextDecoration.get(), CSSPropertyTextDecoration);
    }

    int verticalAlign = getIdentifierValue(style, CSSPropertyVerticalAlign);
    switch (verticalAlign) {
    case CSSValueSub:
        style->removeProperty(CSSPropertyVerticalAlign);
        m_applySubscript = true;
        break;
    case CSSValueSuper:
        style->removeProperty(CSSPropertyVerticalAlign);
        m_applySuperscript = true;
        break;
    }

    if (style->getPropertyCSSValue(CSSPropertyColor)) {
        m_applyFontColor = Color(getRGBAFontColor(style)).serialized();
        style->removeProperty(CSSPropertyColor);
    }

    m_applyFontFace = style->getPropertyValue(CSSPropertyFontFamily);
    style->removeProperty(CSSPropertyFontFamily);

    if (RefPtr<CSSValue> fontSize = style->getPropertyCSSValue(CSSPropertyFontSize)) {
        if (!fontSize->isPrimitiveValue())
            style->removeProperty(CSSPropertyFontSize); // Can't make sense of the number. Put no font size.
        else if (int legacyFontSize = legacyFontSizeFromCSSValue(document, static_cast<CSSPrimitiveValue*>(fontSize.get()),
                shouldUseFixedFontDefaultSize, UseLegacyFontSizeOnlyIfPixelValuesMatch)) {
            m_applyFontSize = String::number(legacyFontSize);
            style->removeProperty(CSSPropertyFontSize);
        }
    }
}

static void diffTextDecorations(CSSMutableStyleDeclaration* style, int propertID, CSSValue* refTextDecoration)
{
    RefPtr<CSSValue> textDecoration = style->getPropertyCSSValue(propertID);
    if (!textDecoration || !textDecoration->isValueList() || !refTextDecoration || !refTextDecoration->isValueList())
        return;

    RefPtr<CSSValueList> newTextDecoration = static_cast<CSSValueList*>(textDecoration.get())->copy();
    CSSValueList* valuesInRefTextDecoration = static_cast<CSSValueList*>(refTextDecoration);

    for (size_t i = 0; i < valuesInRefTextDecoration->length(); i++)
        newTextDecoration->removeAll(valuesInRefTextDecoration->item(i));

    setTextDecorationProperty(style, newTextDecoration.get(), propertID);
}

static bool fontWeightIsBold(CSSStyleDeclaration* style)
{
    ASSERT(style);
    RefPtr<CSSValue> fontWeight = style->getPropertyCSSValueInternal(CSSPropertyFontWeight);

    if (!fontWeight)
        return false;
    if (!fontWeight->isPrimitiveValue())
        return false;

    // Because b tag can only bold text, there are only two states in plain html: bold and not bold.
    // Collapse all other values to either one of these two states for editing purposes.
    switch (static_cast<CSSPrimitiveValue*>(fontWeight.get())->getIdent()) {
        case CSSValue100:
        case CSSValue200:
        case CSSValue300:
        case CSSValue400:
        case CSSValue500:
        case CSSValueNormal:
            return false;
        case CSSValueBold:
        case CSSValue600:
        case CSSValue700:
        case CSSValue800:
        case CSSValue900:
            return true;
    }

    ASSERT_NOT_REACHED(); // For CSSValueBolder and CSSValueLighter
    return false; // Make compiler happy
}

static int getTextAlignment(CSSStyleDeclaration* style)
{
    int textAlign = getIdentifierValue(style, CSSPropertyTextAlign);
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
    }
    return CSSValueInvalid;
}

RefPtr<CSSMutableStyleDeclaration> getPropertiesNotIn(CSSStyleDeclaration* styleWithRedundantProperties, CSSStyleDeclaration* baseStyle)
{
    ASSERT(styleWithRedundantProperties);
    ASSERT(baseStyle);
    RefPtr<CSSMutableStyleDeclaration> result = styleWithRedundantProperties->copy();

    result->removeEquivalentProperties(baseStyle);

    RefPtr<CSSValue> baseTextDecorationsInEffect = baseStyle->getPropertyCSSValueInternal(CSSPropertyWebkitTextDecorationsInEffect);
    diffTextDecorations(result.get(), CSSPropertyTextDecoration, baseTextDecorationsInEffect.get());
    diffTextDecorations(result.get(), CSSPropertyWebkitTextDecorationsInEffect, baseTextDecorationsInEffect.get());

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyFontSize) && fontWeightIsBold(result.get()) == fontWeightIsBold(baseStyle))
        result->removeProperty(CSSPropertyFontWeight);

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyColor) && getRGBAFontColor(result.get()) == getRGBAFontColor(baseStyle))
        result->removeProperty(CSSPropertyColor);

    if (baseStyle->getPropertyCSSValueInternal(CSSPropertyTextAlign) && getTextAlignment(result.get()) == getTextAlignment(baseStyle))
        result->removeProperty(CSSPropertyTextAlign);

    return result;
}

int getIdentifierValue(CSSStyleDeclaration* style, CSSPropertyID propertyID)
{
    if (!style)
        return 0;

    RefPtr<CSSValue> value = style->getPropertyCSSValueInternal(propertyID);
    if (!value || !value->isPrimitiveValue())
        return 0;

    return static_cast<CSSPrimitiveValue*>(value.get())->getIdent();
}

static bool isCSSValueLength(CSSPrimitiveValue* value)
{
    return value->isFontIndependentLength();
}

int legacyFontSizeFromCSSValue(Document* document, CSSPrimitiveValue* value, bool shouldUseFixedFontDefaultSize, LegacyFontSizeMode mode)
{
    if (isCSSValueLength(value)) {
        int pixelFontSize = value->getIntValue(CSSPrimitiveValue::CSS_PX);
        int legacyFontSize = CSSStyleSelector::legacyFontSize(document, pixelFontSize, shouldUseFixedFontDefaultSize);
        // Use legacy font size only if pixel value matches exactly to that of legacy font size.
        int cssPrimitiveEquivalent = legacyFontSize - 1 + CSSValueXSmall;
        if (mode == AlwaysUseLegacyFontSize || CSSStyleSelector::fontSizeForKeyword(document, cssPrimitiveEquivalent, shouldUseFixedFontDefaultSize) == pixelFontSize)
            return legacyFontSize;

        return 0;
    }

    if (CSSValueXSmall <= value->getIdent() && value->getIdent() <= CSSValueWebkitXxxLarge)
        return value->getIdent() - CSSValueXSmall + 1;

    return 0;
}

bool hasTransparentBackgroundColor(CSSStyleDeclaration* style)
{
    RefPtr<CSSValue> cssValue = style->getPropertyCSSValueInternal(CSSPropertyBackgroundColor);
    if (!cssValue)
        return true;
    
    if (!cssValue->isPrimitiveValue())
        return false;
    CSSPrimitiveValue* value = static_cast<CSSPrimitiveValue*>(cssValue.get());
    
    if (value->isRGBColor())
        return !alphaChannel(value->getRGBA32Value());
    
    return value->getIdent() == CSSValueTransparent;
}

PassRefPtr<CSSValue> backgroundColorInEffect(Node* node)
{
    for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        RefPtr<CSSComputedStyleDeclaration> ancestorStyle = computedStyle(ancestor);
        if (!hasTransparentBackgroundColor(ancestorStyle.get()))
            return ancestorStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
    }
    return 0;
}

}

/*
 * Copyright (C) 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplyStyleCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLFontElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include "Range.h"
#include "RenderObject.h"
#include "Text.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static RGBA32 getRGBAFontColor(CSSStyleDeclaration* style)
{
    RefPtr<CSSValue> colorValue = style->getPropertyCSSValue(CSSPropertyColor);
    if (!colorValue)
        return Color::transparent;

    ASSERT(colorValue->isPrimitiveValue());

    CSSPrimitiveValue* primitiveColor = static_cast<CSSPrimitiveValue*>(colorValue.get());
    RGBA32 rgba = 0;
    if (primitiveColor->primitiveType() != CSSPrimitiveValue::CSS_RGBCOLOR) {
        CSSParser::parseColor(rgba, colorValue->cssText());
        // Need to take care of named color such as green and black
        // This code should be removed after https://bugs.webkit.org/show_bug.cgi?id=28282 is fixed.
    } else
        rgba = primitiveColor->getRGBA32Value();

    return rgba;
}

class StyleChange {
public:
    explicit StyleChange(CSSStyleDeclaration*, const Position&);

    String cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }
    bool applyUnderline() const { return m_applyUnderline; }
    bool applyLineThrough() const { return m_applyLineThrough; }
    bool applySubscript() const { return m_applySubscript; }
    bool applySuperscript() const { return m_applySuperscript; }
    bool applyFontColor() const { return m_applyFontColor.length() > 0; }
    bool applyFontFace() const { return m_applyFontFace.length() > 0; }
    bool applyFontSize() const { return m_applyFontSize.length() > 0; }

    String fontColor() { return m_applyFontColor; }
    String fontFace() { return m_applyFontFace; }
    String fontSize() { return m_applyFontSize; }

    bool operator==(const StyleChange& other)
    {
        return m_cssStyle == other.m_cssStyle
            && m_applyBold == other.m_applyBold
            && m_applyItalic == other.m_applyItalic
            && m_applyUnderline == other.m_applyUnderline
            && m_applyLineThrough == other.m_applyLineThrough
            && m_applySubscript == other.m_applySubscript
            && m_applySuperscript == other.m_applySuperscript
            && m_applyFontColor == other.m_applyFontColor
            && m_applyFontFace == other.m_applyFontFace
            && m_applyFontSize == other.m_applyFontSize;
    }
    bool operator!=(const StyleChange& other)
    {
        return !(*this == other);
    }
private:
    void init(PassRefPtr<CSSStyleDeclaration>, const Position&);
    void reconcileTextDecorationProperties(CSSMutableStyleDeclaration*);
    void extractTextStyles(Document*, CSSMutableStyleDeclaration*, bool shouldUseFixedFontDefautlSize);

    String m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
    bool m_applyUnderline;
    bool m_applyLineThrough;
    bool m_applySubscript;
    bool m_applySuperscript;
    String m_applyFontColor;
    String m_applyFontFace;
    String m_applyFontSize;
};


StyleChange::StyleChange(CSSStyleDeclaration* style, const Position& position)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applyUnderline(false)
    , m_applyLineThrough(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
{
    init(style, position);
}

void StyleChange::init(PassRefPtr<CSSStyleDeclaration> style, const Position& position)
{
    Document* document = position.node() ? position.node()->document() : 0;
    if (!document || !document->frame())
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyle = position.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = getPropertiesNotIn(style.get(), computedStyle.get());

    reconcileTextDecorationProperties(mutableStyle.get());
    if (!document->frame()->editor()->shouldStyleWithCSS())
        extractTextStyles(document, mutableStyle.get(), computedStyle->useFixedFontDefaultSize());

    // Changing the whitespace style in a tab span would collapse the tab into a space.
    if (isTabSpanTextNode(position.node()) || isTabSpanNode((position.node())))
        mutableStyle->removeProperty(CSSPropertyWhiteSpace);

    // If unicode-bidi is present in mutableStyle and direction is not, then add direction to mutableStyle.
    // FIXME: Shouldn't this be done in getPropertiesNotIn?
    if (mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi) && !style->getPropertyCSSValue(CSSPropertyDirection))
        mutableStyle->setProperty(CSSPropertyDirection, style->getPropertyValue(CSSPropertyDirection));

    // Save the result for later
    m_cssStyle = mutableStyle->cssText().stripWhiteSpace();
}

void StyleChange::reconcileTextDecorationProperties(CSSMutableStyleDeclaration* style)
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

static int getIdentifierValue(CSSStyleDeclaration* style, int propertyID)
{
    if (!style)
        return 0;

    RefPtr<CSSValue> value = style->getPropertyCSSValue(propertyID);
    if (!value || !value->isPrimitiveValue())
        return 0;

    return static_cast<CSSPrimitiveValue*>(value.get())->getIdent();
}

static void setTextDecorationProperty(CSSMutableStyleDeclaration* style, const CSSValueList* newTextDecoration, int propertyID)
{
    if (newTextDecoration->length())
        style->setProperty(propertyID, newTextDecoration->cssText(), style->getPropertyPriority(propertyID));
    else {
        // text-decoration: none is redundant since it does not remove any text decorations.
        ASSERT(!style->getPropertyPriority(propertyID));
        style->removeProperty(propertyID);
    }
}

void StyleChange::extractTextStyles(Document* document, CSSMutableStyleDeclaration* style, bool shouldUseFixedFontDefautlSize)
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
        m_applyFontColor = Color(getRGBAFontColor(style)).name();
        style->removeProperty(CSSPropertyColor);
    }

    m_applyFontFace = style->getPropertyValue(CSSPropertyFontFamily);
    style->removeProperty(CSSPropertyFontFamily);

    if (RefPtr<CSSValue> fontSize = style->getPropertyCSSValue(CSSPropertyFontSize)) {
        if (!fontSize->isPrimitiveValue())
            style->removeProperty(CSSPropertyFontSize); // Can't make sense of the number. Put no font size.
        else {
            CSSPrimitiveValue* value = static_cast<CSSPrimitiveValue*>(fontSize.get());
            if (value->primitiveType() >= CSSPrimitiveValue::CSS_PX && value->primitiveType() <= CSSPrimitiveValue::CSS_PC) {
                int pixelFontSize = value->getFloatValue(CSSPrimitiveValue::CSS_PX);
                int legacyFontSize = CSSStyleSelector::legacyFontSize(document, pixelFontSize, shouldUseFixedFontDefautlSize);
                // Use legacy font size only if pixel value matches exactly to that of legacy font size.
                if (CSSStyleSelector::fontSizeForKeyword(document, legacyFontSize - 1 + CSSValueXSmall, shouldUseFixedFontDefautlSize) == pixelFontSize) {
                    m_applyFontSize = String::number(legacyFontSize);
                    style->removeProperty(CSSPropertyFontSize);
                }
            } else if (CSSValueXSmall <= value->getIdent() && value->getIdent() <= CSSValueWebkitXxxLarge) {
                m_applyFontSize = String::number(value->getIdent() - CSSValueXSmall + 1);
                style->removeProperty(CSSPropertyFontSize);
            }
        }
    }
}

static String& styleSpanClassString()
{
    DEFINE_STATIC_LOCAL(String, styleSpanClassString, ((AppleStyleSpanClass)));
    return styleSpanClassString;
}

bool isStyleSpan(const Node *node)
{
    if (!node || !node->isHTMLElement())
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    return elem->hasLocalName(spanAttr) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isUnstyledStyleSpan(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    CSSMutableStyleDeclaration* inlineStyleDecl = elem->inlineStyleDecl();
    return (!inlineStyleDecl || inlineStyleDecl->isEmpty()) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isSpanWithoutAttributesOrUnstyleStyleSpan(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    NamedNodeMap* attributes = elem->attributes(true); // readonly
    if (attributes->isEmpty())
        return true;

    return isUnstyledStyleSpan(node);
}

static bool isEmptyFontTag(const Node *node)
{
    if (!node || !node->hasTagName(fontTag))
        return false;

    const Element *elem = static_cast<const Element *>(node);
    NamedNodeMap *map = elem->attributes(true); // true for read-only
    if (!map)
        return true;
    return map->isEmpty() || (map->length() == 1 && elem->getAttribute(classAttr) == styleSpanClassString());
}

static PassRefPtr<Element> createFontElement(Document* document)
{
    RefPtr<Element> fontNode = createHTMLElement(document, fontTag);
    fontNode->setAttribute(classAttr, styleSpanClassString());
    return fontNode.release();
}

PassRefPtr<HTMLElement> createStyleSpanElement(Document* document)
{
    RefPtr<HTMLElement> styleElement = createHTMLElement(document, spanTag);
    styleElement->setAttribute(classAttr, styleSpanClassString());
    return styleElement.release();
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
    RefPtr<CSSValue> fontWeight = style->getPropertyCSSValue(CSSPropertyFontWeight);

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

RefPtr<CSSMutableStyleDeclaration> getPropertiesNotIn(CSSStyleDeclaration* styleWithRedundantProperties, CSSStyleDeclaration* baseStyle)
{
    ASSERT(styleWithRedundantProperties);
    ASSERT(baseStyle);
    RefPtr<CSSMutableStyleDeclaration> result = styleWithRedundantProperties->copy();
    baseStyle->diff(result.get());

    RefPtr<CSSValue> baseTextDecorationsInEffect = baseStyle->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    diffTextDecorations(result.get(), CSSPropertyTextDecoration, baseTextDecorationsInEffect.get());
    diffTextDecorations(result.get(), CSSPropertyWebkitTextDecorationsInEffect, baseTextDecorationsInEffect.get());

    if (fontWeightIsBold(result.get()) == fontWeightIsBold(baseStyle))
        result->removeProperty(CSSPropertyFontWeight);

    if (getRGBAFontColor(result.get()) == getRGBAFontColor(baseStyle))
        result->removeProperty(CSSPropertyColor);

    return result;
}

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be copied to the new paragraph.
// FIXME: The current editingStyleProperties contains all inheritableProperties but we may not need to preserve all inheritable properties
static const int editingStyleProperties[] = {
    // CSS inheritable properties
    CSSPropertyBorderCollapse,
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
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSizeAdjust,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};
size_t numEditingStyleProperties = sizeof(editingStyleProperties)/sizeof(editingStyleProperties[0]);

RefPtr<CSSMutableStyleDeclaration> ApplyStyleCommand::removeNonEditingProperties(CSSStyleDeclaration* style)
{
    return style->copyPropertiesInSet(editingStyleProperties, numEditingStyleProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> ApplyStyleCommand::editingStyleAtPosition(Position pos, ShouldIncludeTypingStyle shouldIncludeTypingStyle)
{
    RefPtr<CSSComputedStyleDeclaration> computedStyleAtPosition = pos.computedStyle();
    RefPtr<CSSMutableStyleDeclaration> style;
    if (!computedStyleAtPosition)
        style = CSSMutableStyleDeclaration::create();
    else
        style = removeNonEditingProperties(computedStyleAtPosition.get());

    if (style && pos.node() && pos.node()->computedStyle()) {
        RenderStyle* renderStyle = pos.node()->computedStyle();
        // If a node's text fill color is invalid, then its children use 
        // their font-color as their text fill color (they don't
        // inherit it).  Likewise for stroke color.
        ExceptionCode ec = 0;
        if (!renderStyle->textFillColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextFillColor, ec);
        if (!renderStyle->textStrokeColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextStrokeColor, ec);
        ASSERT(ec == 0);
        if (renderStyle->fontDescription().keywordSize())
            style->setProperty(CSSPropertyFontSize, computedStyleAtPosition->getFontSizeCSSValuePreferringKeyword()->cssText());
    }

    if (shouldIncludeTypingStyle == IncludeTypingStyle) {
        CSSMutableStyleDeclaration* typingStyle = pos.node()->document()->frame()->selection()->typingStyle();
        if (typingStyle)
            style->merge(typingStyle);
    }

    return style.release();
}

void prepareEditingStyleToApplyAt(CSSMutableStyleDeclaration* editingStyle, Position pos)
{
    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    RefPtr<CSSMutableStyleDeclaration> style = ApplyStyleCommand::editingStyleAtPosition(pos);
    style->diff(editingStyle);

    // if alpha value is zero, we don't add the background color.
    RefPtr<CSSValue> backgroundColor = editingStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
    if (backgroundColor && backgroundColor->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(backgroundColor.get());
        Color color = Color(primitiveValue->getRGBA32Value());
        ExceptionCode ec;
        if (color.alpha() == 0)
            editingStyle->removeProperty(CSSPropertyBackgroundColor, ec);
    }
}

void removeStylesAddedByNode(CSSMutableStyleDeclaration* editingStyle, Node* node)
{
    ASSERT(node);
    ASSERT(node->parentNode());
    RefPtr<CSSMutableStyleDeclaration> parentStyle = ApplyStyleCommand::editingStyleAtPosition(Position(node->parentNode(), 0));
    RefPtr<CSSMutableStyleDeclaration> style = ApplyStyleCommand::editingStyleAtPosition(Position(node, 0));
    parentStyle->diff(style.get());
    style->diff(editingStyle);
}

ApplyStyleCommand::ApplyStyleCommand(Document* document, CSSStyleDeclaration* style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document)
    , m_style(style->makeMutable())
    , m_editingAction(editingAction)
    , m_propertyLevel(propertyLevel)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(0)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document* document, CSSStyleDeclaration* style, const Position& start, const Position& end, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document)
    , m_style(style->makeMutable())
    , m_editingAction(editingAction)
    , m_propertyLevel(propertyLevel)
    , m_start(start)
    , m_end(end)
    , m_useEndingSelection(false)
    , m_styledInlineElement(0)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(PassRefPtr<Element> element, bool removeOnly, EditAction editingAction)
    : CompositeEditCommand(element->document())
    , m_style(CSSMutableStyleDeclaration::create())
    , m_editingAction(editingAction)
    , m_propertyLevel(PropertyDefault)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(element)
    , m_removeOnly(removeOnly)
{
}

void ApplyStyleCommand::updateStartEnd(const Position& newStart, const Position& newEnd)
{
    ASSERT(comparePositions(newEnd, newStart) >= 0);

    if (!m_useEndingSelection && (newStart != m_start || newEnd != m_end))
        m_useEndingSelection = true;

    setEndingSelection(VisibleSelection(newStart, newEnd, VP_DEFAULT_AFFINITY));
    m_start = newStart;
    m_end = newEnd;
}

Position ApplyStyleCommand::startPosition()
{
    if (m_useEndingSelection)
        return endingSelection().start();
    
    return m_start;
}

Position ApplyStyleCommand::endPosition()
{
    if (m_useEndingSelection)
        return endingSelection().end();
    
    return m_end;
}

void ApplyStyleCommand::doApply()
{
    switch (m_propertyLevel) {
        case PropertyDefault: {
            // apply the block-centric properties of the style
            RefPtr<CSSMutableStyleDeclaration> blockStyle = m_style->copyBlockProperties();
            if (blockStyle->length())
                applyBlockStyle(blockStyle.get());
            // apply any remaining styles to the inline elements
            // NOTE: hopefully, this string comparison is the same as checking for a non-null diff
            if (blockStyle->length() < m_style->length() || m_styledInlineElement) {
                RefPtr<CSSMutableStyleDeclaration> inlineStyle = m_style->copy();
                applyRelativeFontStyleChange(inlineStyle.get());
                blockStyle->diff(inlineStyle.get());
                applyInlineStyle(inlineStyle.get());
            }
            break;
        }
        case ForceBlockProperties:
            // Force all properties to be applied as block styles.
            applyBlockStyle(m_style.get());
            break;
    }
}

EditAction ApplyStyleCommand::editingAction() const
{
    return m_editingAction;
}

void ApplyStyleCommand::applyBlockStyle(CSSMutableStyleDeclaration *style)
{
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    // get positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();
    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }
        
    VisiblePosition visibleStart(start);
    VisiblePosition visibleEnd(end);
    // Save and restore the selection endpoints using their indices in the document, since
    // addBlockStyleIfNeeded may moveParagraphs, which can remove these endpoints.
    // Calculate start and end indices from the start of the tree that they're in.
    Node* scope = highestAncestor(visibleStart.deepEquivalent().node());
    Position rangeStart(scope, 0);
    RefPtr<Range> startRange = Range::create(document(), rangeStart, rangeCompliantEquivalent(visibleStart.deepEquivalent()));
    RefPtr<Range> endRange = Range::create(document(), rangeStart, rangeCompliantEquivalent(visibleEnd.deepEquivalent()));
    int startIndex = TextIterator::rangeLength(startRange.get(), true);
    int endIndex = TextIterator::rangeLength(endRange.get(), true);
    
    VisiblePosition paragraphStart(startOfParagraph(visibleStart));
    VisiblePosition nextParagraphStart(endOfParagraph(paragraphStart).next());
    VisiblePosition beyondEnd(endOfParagraph(visibleEnd).next());
    while (paragraphStart.isNotNull() && paragraphStart != beyondEnd) {
        StyleChange styleChange(style, paragraphStart.deepEquivalent());
        if (styleChange.cssStyle().length() || m_removeOnly) {
            RefPtr<Node> block = enclosingBlock(paragraphStart.deepEquivalent().node());
            RefPtr<Node> newBlock = moveParagraphContentsToNewBlockIfNecessary(paragraphStart.deepEquivalent());
            if (newBlock)
                block = newBlock;
            ASSERT(block->isHTMLElement());
            if (block->isHTMLElement()) {
                removeCSSStyle(style, static_cast<HTMLElement*>(block.get()));
                if (!m_removeOnly)
                    addBlockStyle(styleChange, static_cast<HTMLElement*>(block.get()));
            }

            if (nextParagraphStart.isOrphan())
                nextParagraphStart = endOfParagraph(paragraphStart).next();
        }

        paragraphStart = nextParagraphStart;
        nextParagraphStart = endOfParagraph(paragraphStart).next();
    }
    
    startRange = TextIterator::rangeFromLocationAndLength(static_cast<Element*>(scope), startIndex, 0, true);
    endRange = TextIterator::rangeFromLocationAndLength(static_cast<Element*>(scope), endIndex, 0, true);
    if (startRange && endRange)
        updateStartEnd(startRange->startPosition(), endRange->startPosition());
}

#define NoFontDelta (0.0f)
#define MinimumFontSize (0.1f)

void ApplyStyleCommand::applyRelativeFontStyleChange(CSSMutableStyleDeclaration *style)
{
    RefPtr<CSSValue> value = style->getPropertyCSSValue(CSSPropertyFontSize);
    if (value) {
        // Explicit font size overrides any delta.
        style->removeProperty(CSSPropertyWebkitFontSizeDelta);
        return;
    }

    // Get the adjustment amount out of the style.
    value = style->getPropertyCSSValue(CSSPropertyWebkitFontSizeDelta);
    if (!value)
        return;
    float adjustment = NoFontDelta;
    if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
        CSSPrimitiveValue *primitiveValue = static_cast<CSSPrimitiveValue *>(value.get());
        if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PX) {
            // Only PX handled now. If we handle more types in the future, perhaps
            // a switch statement here would be more appropriate.
            adjustment = primitiveValue->getFloatValue();
        }
    }
    style->removeProperty(CSSPropertyWebkitFontSizeDelta);
    if (adjustment == NoFontDelta)
        return;
    
    Position start = startPosition();
    Position end = endPosition();
    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }
    
    // Join up any adjacent text nodes.
    if (start.node()->isTextNode()) {
        joinChildTextNodes(start.node()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }
    if (end.node()->isTextNode() && start.node()->parentNode() != end.node()->parentNode()) {
        joinChildTextNodes(end.node()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    // Split the start text nodes if needed to apply style.
    if (isValidCaretPositionInTextNode(start)) {
        splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (isValidCaretPositionInTextNode(end)) {
        splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
    }

    // Calculate loop end point.
    // If the end node is before the start node (can only happen if the end node is
    // an ancestor of the start node), we gather nodes up to the next sibling of the end node
    Node *beyondEnd;
    if (start.node()->isDescendantOf(end.node()))
        beyondEnd = end.node()->traverseNextSibling();
    else
        beyondEnd = end.node()->traverseNextNode();
    
    start = start.upstream(); // Move upstream to ensure we do not add redundant spans.
    Node *startNode = start.node();
    if (startNode->isTextNode() && start.deprecatedEditingOffset() >= caretMaxOffset(startNode)) // Move out of text node if range does not include its characters.
        startNode = startNode->traverseNextNode();

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    HashMap<Node*, float> startingFontSizes;
    for (Node *node = startNode; node != beyondEnd; node = node->traverseNextNode())
        startingFontSizes.set(node, computedFontSize(node));

    // These spans were added by us. If empty after font size changes, they can be removed.
    Vector<RefPtr<HTMLElement> > unstyledSpans;
    
    Node* lastStyledNode = 0;
    for (Node* node = startNode; node != beyondEnd; node = node->traverseNextNode()) {
        RefPtr<HTMLElement> element;
        if (node->isHTMLElement()) {
            // Only work on fully selected nodes.
            if (!nodeFullySelected(node, start, end))
                continue;
            element = static_cast<HTMLElement*>(node);
        } else if (node->isTextNode() && node->renderer() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            RefPtr<HTMLElement> span = createStyleSpanElement(document());
            surroundNodeRangeWithElement(node, node, span.get());
            element = span.release();
        }  else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;

        CSSMutableStyleDeclaration* inlineStyleDecl = element->getInlineStyleDecl();
        float currentFontSize = computedFontSize(node);
        float desiredFontSize = max(MinimumFontSize, startingFontSizes.get(node) + adjustment);
        RefPtr<CSSValue> value = inlineStyleDecl->getPropertyCSSValue(CSSPropertyFontSize);
        if (value) {
            inlineStyleDecl->removeProperty(CSSPropertyFontSize, true);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            inlineStyleDecl->setProperty(CSSPropertyFontSize, String::number(desiredFontSize) + "px", false, false);
            setNodeAttribute(element.get(), styleAttr, inlineStyleDecl->cssText());
        }
        if (inlineStyleDecl->isEmpty()) {
            removeNodeAttribute(element.get(), styleAttr);
            // FIXME: should this be isSpanWithoutAttributesOrUnstyleStyleSpan?  Need a test.
            if (isUnstyledStyleSpan(element.get()))
                unstyledSpans.append(element.release());
        }
    }

    size_t size = unstyledSpans.size();
    for (size_t i = 0; i < size; ++i)
        removeNodePreservingChildren(unstyledSpans[i].get());
}

#undef NoFontDelta
#undef MinimumFontSize

static Node* dummySpanAncestorForNode(const Node* node)
{
    while (node && !isStyleSpan(node))
        node = node->parent();
    
    return node ? node->parent() : 0;
}

void ApplyStyleCommand::cleanupUnstyledAppleStyleSpans(Node* dummySpanAncestor)
{
    if (!dummySpanAncestor)
        return;

    // Dummy spans are created when text node is split, so that style information
    // can be propagated, which can result in more splitting. If a dummy span gets
    // cloned/split, the new node is always a sibling of it. Therefore, we scan
    // all the children of the dummy's parent
    Node* next;
    for (Node* node = dummySpanAncestor->firstChild(); node; node = next) {
        next = node->nextSibling();
        if (isUnstyledStyleSpan(node))
            removeNodePreservingChildren(node);
        node = next;
    }
}

HTMLElement* ApplyStyleCommand::splitAncestorsWithUnicodeBidi(Node* node, bool before, int allowedDirection)
{
    // We are allowed to leave the highest ancestor with unicode-bidi unsplit if it is unicode-bidi: embed and direction: allowedDirection.
    // In that case, we return the unsplit ancestor. Otherwise, we return 0.
    Node* block = enclosingBlock(node);
    if (!block)
        return 0;

    Node* highestAncestorWithUnicodeBidi = 0;
    Node* nextHighestAncestorWithUnicodeBidi = 0;
    int highestAncestorUnicodeBidi = 0;
    for (Node* n = node->parent(); n != block; n = n->parent()) {
        int unicodeBidi = getIdentifierValue(computedStyle(n).get(), CSSPropertyUnicodeBidi);
        if (unicodeBidi && unicodeBidi != CSSValueNormal) {
            highestAncestorUnicodeBidi = unicodeBidi;
            nextHighestAncestorWithUnicodeBidi = highestAncestorWithUnicodeBidi;
            highestAncestorWithUnicodeBidi = n;
        }
    }

    if (!highestAncestorWithUnicodeBidi)
        return 0;

    HTMLElement* unsplitAncestor = 0;

    if (allowedDirection && highestAncestorUnicodeBidi != CSSValueBidiOverride
        && getIdentifierValue(computedStyle(highestAncestorWithUnicodeBidi).get(), CSSPropertyDirection) == allowedDirection
        && highestAncestorWithUnicodeBidi->isHTMLElement()) {
        if (!nextHighestAncestorWithUnicodeBidi)
            return static_cast<HTMLElement*>(highestAncestorWithUnicodeBidi);

        unsplitAncestor = static_cast<HTMLElement*>(highestAncestorWithUnicodeBidi);
        highestAncestorWithUnicodeBidi = nextHighestAncestorWithUnicodeBidi;
    }

    // Split every ancestor through highest ancestor with embedding.
    Node* n = node;
    while (true) {
        Element* parent = static_cast<Element*>(n->parent());
        if (before ? n->previousSibling() : n->nextSibling())
            splitElement(parent, before ? n : n->nextSibling());
        if (parent == highestAncestorWithUnicodeBidi)
            break;
        n = n->parent();
    }
    return unsplitAncestor;
}

void ApplyStyleCommand::removeEmbeddingUpToEnclosingBlock(Node* node, Node* unsplitAncestor)
{
    Node* block = enclosingBlock(node);
    if (!block)
        return;

    Node* parent = 0;
    for (Node* n = node->parent(); n != block && n != unsplitAncestor; n = parent) {
        parent = n->parent();
        if (!n->isStyledElement())
            continue;

        StyledElement* element = static_cast<StyledElement*>(n);
        int unicodeBidi = getIdentifierValue(computedStyle(element).get(), CSSPropertyUnicodeBidi);
        if (!unicodeBidi || unicodeBidi == CSSValueNormal)
            continue;

        // FIXME: This code should really consider the mapped attribute 'dir', the inline style declaration,
        // and all matching style rules in order to determine how to best set the unicode-bidi property to 'normal'.
        // For now, it assumes that if the 'dir' attribute is present, then removing it will suffice, and
        // otherwise it sets the property in the inline style declaration.
        if (element->hasAttribute(dirAttr)) {
            // FIXME: If this is a BDO element, we should probably just remove it if it has no
            // other attributes, like we (should) do with B and I elements.
            removeNodeAttribute(element, dirAttr);
        } else {
            RefPtr<CSSMutableStyleDeclaration> inlineStyle = element->getInlineStyleDecl()->copy();
            inlineStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
            inlineStyle->removeProperty(CSSPropertyDirection);
            setNodeAttribute(element, styleAttr, inlineStyle->cssText());
            // FIXME: should this be isSpanWithoutAttributesOrUnstyleStyleSpan?  Need a test.
            if (isUnstyledStyleSpan(element))
                removeNodePreservingChildren(element);
        }
    }
}

static Node* highestEmbeddingAncestor(Node* startNode, Node* enclosingNode)
{
    for (Node* n = startNode; n && n != enclosingNode; n = n->parent()) {
        if (n->isHTMLElement() && getIdentifierValue(computedStyle(n).get(), CSSPropertyUnicodeBidi) == CSSValueEmbed)
            return n;
    }

    return 0;
}

void ApplyStyleCommand::applyInlineStyle(CSSMutableStyleDeclaration *style)
{
    Node* startDummySpanAncestor = 0;
    Node* endDummySpanAncestor = 0;
    
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    // adjust to the positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();
    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = isValidCaretPositionInTextNode(start);
    if (splitStart) {
        if (shouldSplitTextElement(start.node()->parentElement(), style))
            splitTextElementAtStart(start, end);
        else
            splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
        startDummySpanAncestor = dummySpanAncestorForNode(start.node());
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = isValidCaretPositionInTextNode(end);
    if (splitEnd) {
        if (shouldSplitTextElement(end.node()->parentElement(), style))
            splitTextElementAtEnd(start, end);
        else
            splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
        endDummySpanAncestor = dummySpanAncestorForNode(end.node());
    }

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    Position removeStart = start.upstream();
    int unicodeBidi = getIdentifierValue(style, CSSPropertyUnicodeBidi);
    int direction = 0;
    RefPtr<CSSMutableStyleDeclaration> styleWithoutEmbedding;
    if (unicodeBidi) {
        // Leave alone an ancestor that provides the desired single level embedding, if there is one.
        if (unicodeBidi == CSSValueEmbed)
            direction = getIdentifierValue(style, CSSPropertyDirection);
        HTMLElement* startUnsplitAncestor = splitAncestorsWithUnicodeBidi(start.node(), true, direction);
        HTMLElement* endUnsplitAncestor = splitAncestorsWithUnicodeBidi(end.node(), false, direction);
        removeEmbeddingUpToEnclosingBlock(start.node(), startUnsplitAncestor);
        removeEmbeddingUpToEnclosingBlock(end.node(), endUnsplitAncestor);

        // Avoid removing the dir attribute and the unicode-bidi and direction properties from the unsplit ancestors.
        Position embeddingRemoveStart = removeStart;
        if (startUnsplitAncestor && nodeFullySelected(startUnsplitAncestor, removeStart, end))
            embeddingRemoveStart = positionInParentAfterNode(startUnsplitAncestor);

        Position embeddingRemoveEnd = end;
        if (endUnsplitAncestor && nodeFullySelected(endUnsplitAncestor, removeStart, end))
            embeddingRemoveEnd = positionInParentBeforeNode(endUnsplitAncestor).downstream();

        if (embeddingRemoveEnd != removeStart || embeddingRemoveEnd != end) {
            RefPtr<CSSMutableStyleDeclaration> embeddingStyle = CSSMutableStyleDeclaration::create();
            embeddingStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
            embeddingStyle->setProperty(CSSPropertyDirection, direction);
            if (comparePositions(embeddingRemoveStart, embeddingRemoveEnd) <= 0)
                removeInlineStyle(embeddingStyle, embeddingRemoveStart, embeddingRemoveEnd);
            styleWithoutEmbedding = style->copy();
            styleWithoutEmbedding->removeProperty(CSSPropertyUnicodeBidi);
            styleWithoutEmbedding->removeProperty(CSSPropertyDirection);
        }
    }

    removeInlineStyle(styleWithoutEmbedding ? styleWithoutEmbedding.get() : style, removeStart, end);
    start = startPosition();
    end = endPosition();

    if (splitStart) {
        if (mergeStartWithPreviousIfIdentical(start, end)) {
            start = startPosition();
            end = endPosition();
        }
    }

    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end);
        start = startPosition();
        end = endPosition();
    }

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    RefPtr<CSSMutableStyleDeclaration> styleToApply = style;
    if (unicodeBidi) {
        // Avoid applying the unicode-bidi and direction properties beneath ancestors that already have them.
        Node* embeddingStartNode = highestEmbeddingAncestor(start.node(), enclosingBlock(start.node()));
        Node* embeddingEndNode = highestEmbeddingAncestor(end.node(), enclosingBlock(end.node()));

        if (embeddingStartNode || embeddingEndNode) {
            Position embeddingApplyStart = embeddingStartNode ? positionInParentAfterNode(embeddingStartNode) : start;
            Position embeddingApplyEnd = embeddingEndNode ? positionInParentBeforeNode(embeddingEndNode) : end;
            ASSERT(embeddingApplyStart.isNotNull() && embeddingApplyEnd.isNotNull());

            RefPtr<CSSMutableStyleDeclaration> embeddingStyle = CSSMutableStyleDeclaration::create();
            embeddingStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
            embeddingStyle->setProperty(CSSPropertyDirection, direction);
            fixRangeAndApplyInlineStyle(embeddingStyle.get(), embeddingApplyStart, embeddingApplyEnd);

            if (styleWithoutEmbedding)
                styleToApply = styleWithoutEmbedding;
            else {
                styleToApply = style->copy();
                styleToApply->removeProperty(CSSPropertyUnicodeBidi);
                styleToApply->removeProperty(CSSPropertyDirection);
            }
        }
    }

    fixRangeAndApplyInlineStyle(styleToApply.get(), start, end);

    // Remove dummy style spans created by splitting text elements.
    cleanupUnstyledAppleStyleSpans(startDummySpanAncestor);
    if (endDummySpanAncestor != startDummySpanAncestor)
        cleanupUnstyledAppleStyleSpans(endDummySpanAncestor);
}

void ApplyStyleCommand::fixRangeAndApplyInlineStyle(CSSMutableStyleDeclaration* style, const Position& start, const Position& end)
{
    Node* startNode = start.node();

    if (start.deprecatedEditingOffset() >= caretMaxOffset(start.node())) {
        startNode = startNode->traverseNextNode();
        if (!startNode || comparePositions(end, Position(startNode, 0)) < 0)
            return;
    }

    Node* pastEndNode = end.node();
    if (end.deprecatedEditingOffset() >= caretMaxOffset(end.node()))
        pastEndNode = end.node()->traverseNextSibling();

    // FIXME: Callers should perform this operation on a Range that includes the br
    // if they want style applied to the empty line.
    if (start == end && start.node()->hasTagName(brTag))
        pastEndNode = start.node()->traverseNextNode();

    applyInlineStyleToNodeRange(style, startNode, pastEndNode);
}

static bool containsNonEditableRegion(Node* node)
{
    if (!node->isContentEditable())
        return true;

    Node* sibling = node->traverseNextSibling();
    for (Node* descendent = node->firstChild(); descendent && descendent != sibling; descendent = descendent->traverseNextNode()) {
        if (!descendent->isContentEditable())
            return true;
    }

    return false;
}

void ApplyStyleCommand::applyInlineStyleToNodeRange(CSSMutableStyleDeclaration* style, Node* node, Node* pastEndNode)
{
    for (Node* next; node && node != pastEndNode; node = next) {
        next = node->traverseNextNode();
        
        if (!node->renderer() || !node->isContentEditable())
            continue;
        
        if (!node->isContentRichlyEditable() && node->isHTMLElement()) {
            // This is a plaintext-only region. Only proceed if it's fully selected.
            // pastEndNode is the node after the last fully selected node, so if it's inside node then
            // node isn't fully selected.
            if (pastEndNode && pastEndNode->isDescendantOf(node))
                break;
            // Add to this element's inline style and skip over its contents.
            HTMLElement* element = static_cast<HTMLElement*>(node);
            RefPtr<CSSMutableStyleDeclaration> inlineStyle = element->getInlineStyleDecl()->copy();
            inlineStyle->merge(style);
            setNodeAttribute(element, styleAttr, inlineStyle->cssText());
            next = node->traverseNextSibling();
            continue;
        }
        
        if (isBlock(node))
            continue;
        
        if (node->childNodeCount()) {
            if (node->contains(pastEndNode) || containsNonEditableRegion(node) || !node->parentNode()->isContentEditable())
                continue;
            if (editingIgnoresContent(node)) {
                next = node->traverseNextSibling();
                continue;
            }
        }

        Node* runEnd = node;
        Node* sibling = node->nextSibling();
        while (sibling && sibling != pastEndNode && !sibling->contains(pastEndNode)
               && (!isBlock(sibling) || sibling->hasTagName(brTag))
               && !containsNonEditableRegion(sibling)) {
            runEnd = sibling;
            sibling = runEnd->nextSibling();
        }
        next = runEnd->traverseNextSibling();

        if (!removeStyleFromRunBeforeApplyingStyle(style, node, runEnd))
            continue;
        addInlineStyleIfNeeded(style, node, runEnd, m_removeOnly ? DoNotAddStyledElement : AddStyledElement);
    }
}

bool ApplyStyleCommand::removeStyleFromRunBeforeApplyingStyle(CSSMutableStyleDeclaration* style, Node*& runStart, Node*& runEnd)
{
    Node* pastEndNode = runEnd->traverseNextSibling();
    Node* next;
    for (Node* node = runStart; node && node != pastEndNode; node = next) {
        next = node->traverseNextNode();
        if (!node->isHTMLElement())
            continue;
        
        Node* previousSibling = node->previousSibling();
        Node* nextSibling = node->nextSibling();
        Node* parent = node->parentNode();
        removeInlineStyleFromElement(style, static_cast<HTMLElement*>(node), RemoveAlways);
        if (!node->inDocument()) {
            // FIXME: We might need to update the start and the end of current selection here but need a test.
            if (runStart == node)
                runStart = previousSibling ? previousSibling->nextSibling() : parent->firstChild();
            if (runEnd == node)
                runEnd = nextSibling ? nextSibling->previousSibling() : parent->lastChild();
        }
    }

    return true;
}

bool ApplyStyleCommand::removeInlineStyleFromElement(CSSMutableStyleDeclaration* style, HTMLElement* element, InlineStyleRemovalMode mode, CSSMutableStyleDeclaration* extractedStyle)
{
    ASSERT(style);
    ASSERT(element);

    if (m_styledInlineElement && element->hasTagName(m_styledInlineElement->tagQName())) {
        if (mode != RemoveNone) {
            if (extractedStyle && element->inlineStyleDecl())
                extractedStyle->merge(element->inlineStyleDecl());
            removeNodePreservingChildren(element);
        }
        return true;
    }

    bool removed = false;
    if (removeImplicitlyStyledElement(style, element, mode, extractedStyle))
        removed = true;

    if (!element->inDocument())
        return removed;

    // If the node was converted to a span, the span may still contain relevant
    // styles which must be removed (e.g. <b style='font-weight: bold'>)
    if (removeCSSStyle(style, element, mode, extractedStyle))
        removed = true;

    return removed;
}
    
enum EPushDownType { ShouldBePushedDown, ShouldNotBePushedDown };
struct HTMLEquivalent {
    int propertyID;
    bool isValueList;
    int primitiveId;
    const QualifiedName* element;
    const QualifiedName* attribute;
    PassRefPtr<CSSValue> (*attributeToCSSValue)(int propertyID, const String&);
    EPushDownType pushDownType;
};

static PassRefPtr<CSSValue> stringToCSSValue(int propertyID, const String& value)
{
    RefPtr<CSSMutableStyleDeclaration> dummyStyle;
    dummyStyle = CSSMutableStyleDeclaration::create();
    dummyStyle->setProperty(propertyID, value);
    return dummyStyle->getPropertyCSSValue(propertyID);
}

static PassRefPtr<CSSValue> fontSizeToCSSValue(int propertyID, const String& value)
{
    UNUSED_PARAM(propertyID);
    ASSERT(propertyID == CSSPropertyFontSize);
    int size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return 0;
    return CSSPrimitiveValue::createIdentifier(size);
}

static const HTMLEquivalent HTMLEquivalents[] = {
    { CSSPropertyFontWeight, false, CSSValueBold, &bTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyFontWeight, false, CSSValueBold, &strongTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyVerticalAlign, false, CSSValueSub, &subTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyVerticalAlign, false, CSSValueSuper, &supTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyFontStyle, false, CSSValueItalic, &iTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyFontStyle, false, CSSValueItalic, &emTag, 0, 0, ShouldBePushedDown },

    // text-decorations should be CSSValueList
    { CSSPropertyTextDecoration, true, CSSValueUnderline, &uTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyTextDecoration, true, CSSValueLineThrough, &sTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyTextDecoration, true, CSSValueLineThrough, &strikeTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyWebkitTextDecorationsInEffect, true, CSSValueUnderline, &uTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyWebkitTextDecorationsInEffect, true, CSSValueLineThrough, &sTag, 0, 0, ShouldBePushedDown },
    { CSSPropertyWebkitTextDecorationsInEffect, true, CSSValueLineThrough, &strikeTag, 0, 0, ShouldBePushedDown },

    // FIXME: font attributes should only be removed if values were different
    { CSSPropertyColor, false, CSSValueInvalid, &fontTag, &colorAttr, stringToCSSValue, ShouldBePushedDown },
    { CSSPropertyFontFamily, false, CSSValueInvalid, &fontTag, &faceAttr, stringToCSSValue, ShouldBePushedDown },
    { CSSPropertyFontSize, false, CSSValueInvalid, &fontTag, &sizeAttr, fontSizeToCSSValue, ShouldBePushedDown },

    // unicode-bidi and direction are pushed down separately so don't push down with other styles.
    { CSSPropertyDirection, false, CSSValueInvalid, 0, &dirAttr, stringToCSSValue, ShouldNotBePushedDown },
    { CSSPropertyUnicodeBidi, false, CSSValueInvalid, 0, &dirAttr, stringToCSSValue, ShouldNotBePushedDown },
};

bool ApplyStyleCommand::removeImplicitlyStyledElement(CSSMutableStyleDeclaration* style, HTMLElement* element, InlineStyleRemovalMode mode, CSSMutableStyleDeclaration* extractedStyle)
{
    // Current implementation does not support stylePushedDown when mode == RemoveNone because of early exit.
    ASSERT(!extractedStyle || mode != RemoveNone);
    bool removed = false;
    for (size_t i = 0; i < sizeof(HTMLEquivalents) / sizeof(HTMLEquivalent); i++) {
        const HTMLEquivalent& equivalent = HTMLEquivalents[i];
        ASSERT(equivalent.element || equivalent.attribute);
        if ((extractedStyle && equivalent.pushDownType == ShouldNotBePushedDown)
            || (equivalent.element && !element->hasTagName(*equivalent.element))
            || (equivalent.attribute && !element->hasAttribute(*equivalent.attribute)))
            continue;

        RefPtr<CSSValue> styleValue = style->getPropertyCSSValue(equivalent.propertyID);
        if (!styleValue)
            continue;
        RefPtr<CSSValue> mapValue;
        if (equivalent.attribute)
            mapValue = equivalent.attributeToCSSValue(equivalent.propertyID, element->getAttribute(*equivalent.attribute));
        else
            mapValue = CSSPrimitiveValue::createIdentifier(equivalent.primitiveId).get();

        if (mode != RemoveAlways) {
            if (equivalent.isValueList && styleValue->isValueList() && static_cast<CSSValueList*>(styleValue.get())->hasValue(mapValue.get()))
                continue; // If CSS value assumes CSSValueList, then only skip if the value was present in style to apply.
            else if (mapValue && styleValue->cssText() == mapValue->cssText())
                continue; // If CSS value is primitive, then skip if they are equal.
        }

        if (extractedStyle)
            extractedStyle->setProperty(equivalent.propertyID, mapValue->cssText());

        if (mode == RemoveNone)
            return true;

        removed = true;
        if (!equivalent.attribute) {
            replaceWithSpanOrRemoveIfWithoutAttributes(element);
            break;
        }
        removeNodeAttribute(element, *equivalent.attribute);
        if (isEmptyFontTag(element) || isSpanWithoutAttributesOrUnstyleStyleSpan(element))
            removeNodePreservingChildren(element);
    }
    return removed;
}
    
void ApplyStyleCommand::replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement*& elem)
{
    bool removeNode = false;

    // Similar to isSpanWithoutAttributesOrUnstyleStyleSpan, but does not look for Apple-style-span.
    NamedNodeMap* attributes = elem->attributes(true); // readonly
    if (!attributes || attributes->isEmpty())
        removeNode = true;
    else if (attributes->length() == 1 && elem->hasAttribute(styleAttr)) {
        // Remove the element even if it has just style='' (this might be redundantly checked later too)
        CSSMutableStyleDeclaration* inlineStyleDecl = elem->inlineStyleDecl();
        if (!inlineStyleDecl || inlineStyleDecl->isEmpty())
            removeNode = true;
    }

    if (removeNode)
        removeNodePreservingChildren(elem);
    else {
        HTMLElement* newSpanElement = replaceNodeWithSpanPreservingChildrenAndAttributes(elem);
        ASSERT(newSpanElement && newSpanElement->inDocument());
        elem = newSpanElement;
    }
}

bool ApplyStyleCommand::removeCSSStyle(CSSMutableStyleDeclaration* style, HTMLElement* element, InlineStyleRemovalMode mode, CSSMutableStyleDeclaration* extractedStyle)
{
    ASSERT(style);
    ASSERT(element);

    CSSMutableStyleDeclaration* decl = element->inlineStyleDecl();
    if (!decl)
        return false;

    bool removed = false;
    CSSMutableStyleDeclaration::const_iterator end = style->end();
    for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(it->id());
        RefPtr<CSSValue> value = decl->getPropertyCSSValue(propertyID);
        if (value && (propertyID != CSSPropertyWhiteSpace || !isTabSpanNode(element))) {
            removed = true;
            if (mode == RemoveNone)
                return true;

            ExceptionCode ec = 0;
            if (extractedStyle)
                extractedStyle->setProperty(propertyID, value->cssText(), decl->getPropertyPriority(propertyID), ec);
            removeCSSProperty(element, propertyID);

            if (propertyID == CSSPropertyUnicodeBidi && !decl->getPropertyValue(CSSPropertyDirection).isEmpty()) {
                if (extractedStyle)
                    extractedStyle->setProperty(CSSPropertyDirection, decl->getPropertyValue(CSSPropertyDirection), decl->getPropertyPriority(CSSPropertyDirection), ec);
                removeCSSProperty(element, CSSPropertyDirection);
            }
        }
    }

    if (mode == RemoveNone)
        return removed;

    // No need to serialize <foo style=""> if we just removed the last css property
    if (decl->isEmpty())
        removeNodeAttribute(element, styleAttr);

    if (isSpanWithoutAttributesOrUnstyleStyleSpan(element))
        removeNodePreservingChildren(element);

    return removed;
}

HTMLElement* ApplyStyleCommand::highestAncestorWithConflictingInlineStyle(CSSMutableStyleDeclaration* style, Node* node)
{
    if (!node)
        return 0;

    HTMLElement* result = 0;
    Node* unsplittableElement = unsplittableElementForPosition(Position(node, 0));

    for (Node *n = node; n; n = n->parentNode()) {
        if (n->isHTMLElement() && shouldRemoveInlineStyleFromElement(style, static_cast<HTMLElement*>(n)))
            result = static_cast<HTMLElement*>(n);
        // Should stop at the editable root (cannot cross editing boundary) and
        // also stop at the unsplittable element to be consistent with other UAs
        if (n == unsplittableElement)
            break;
    }

    return result;
}

void ApplyStyleCommand::applyInlineStyleToPushDown(Node* node, CSSMutableStyleDeclaration* style)
{
    ASSERT(node);

    if (!style || !style->length() || !node->renderer())
        return;

    RefPtr<CSSMutableStyleDeclaration> newInlineStyle = style;
    if (node->isHTMLElement()) {
        HTMLElement* element = static_cast<HTMLElement*>(node);
        CSSMutableStyleDeclaration* existingInlineStyle = element->inlineStyleDecl();

        // Avoid overriding existing styles of node
        if (existingInlineStyle) {
            newInlineStyle = existingInlineStyle->copy();
            CSSMutableStyleDeclaration::const_iterator end = style->end();
            for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
                ExceptionCode ec;
                if (!existingInlineStyle->getPropertyCSSValue(it->id()))
                    newInlineStyle->setProperty(it->id(), it->value()->cssText(), it->isImportant(), ec);

                // text-decorations adds up
                if (it->id() == CSSPropertyTextDecoration && it->value()->isValueList()) {
                    RefPtr<CSSValue> textDecoration = newInlineStyle->getPropertyCSSValue(CSSPropertyTextDecoration);
                    if (textDecoration && textDecoration->isValueList()) {
                        CSSValueList* textDecorationOfInlineStyle = static_cast<CSSValueList*>(textDecoration.get());
                        CSSValueList* textDecorationOfStyleApplied = static_cast<CSSValueList*>(it->value());

                        DEFINE_STATIC_LOCAL(RefPtr<CSSPrimitiveValue>, underline, (CSSPrimitiveValue::createIdentifier(CSSValueUnderline)));
                        DEFINE_STATIC_LOCAL(RefPtr<CSSPrimitiveValue>, lineThrough, (CSSPrimitiveValue::createIdentifier(CSSValueLineThrough)));
                        
                        if (textDecorationOfStyleApplied->hasValue(underline.get()) && !textDecorationOfInlineStyle->hasValue(underline.get()))
                            textDecorationOfInlineStyle->append(underline.get());

                        if (textDecorationOfStyleApplied->hasValue(lineThrough.get()) && !textDecorationOfInlineStyle->hasValue(lineThrough.get()))
                            textDecorationOfInlineStyle->append(lineThrough.get());
                    }
                }
            }
        }
    }

    // Since addInlineStyleIfNeeded can't add styles to block-flow render objects, add style attribute instead.
    // FIXME: applyInlineStyleToRange should be used here instead.
    if ((node->renderer()->isBlockFlow() || node->childNodeCount()) && node->isHTMLElement()) {
        setNodeAttribute(static_cast<HTMLElement*>(node), styleAttr, newInlineStyle->cssText());
        return;
    }

    if (node->renderer()->isText() && static_cast<RenderText*>(node->renderer())->isAllCollapsibleWhitespace())
        return;

    // We can't wrap node with the styled element here because new styled element will never be removed if we did.
    // If we modified the child pointer in pushDownInlineStyleAroundNode to point to new style element
    // then we fall into an infinite loop where we keep removing and adding styled element wrapping node.
    addInlineStyleIfNeeded(newInlineStyle.get(), node, node, DoNotAddStyledElement);
}

void ApplyStyleCommand::pushDownInlineStyleAroundNode(CSSMutableStyleDeclaration* style, Node* targetNode)
{
    HTMLElement* highestAncestor = highestAncestorWithConflictingInlineStyle(style, targetNode);
    if (!highestAncestor)
        return;

    // The outer loop is traversing the tree vertically from highestAncestor to targetNode
    Node* current = highestAncestor;
    while (current != targetNode) {
        ASSERT(current);
        ASSERT(current->isHTMLElement());
        ASSERT(current->contains(targetNode));
        Node* child = current->firstChild();
        Node* lastChild = current->lastChild();
        RefPtr<StyledElement> styledElement;
        if (current->isStyledElement() && m_styledInlineElement && current->hasTagName(m_styledInlineElement->tagQName()))
            styledElement = static_cast<StyledElement*>(current);
        RefPtr<CSSMutableStyleDeclaration> styleToPushDown = CSSMutableStyleDeclaration::create();
        removeInlineStyleFromElement(style, static_cast<HTMLElement*>(current), RemoveIfNeeded, styleToPushDown.get());

        // The inner loop will go through children on each level
        // FIXME: we should aggregate inline child elements together so that we don't wrap each child separately.
        while (child) {
            Node* nextChild = child->nextSibling();

            if (child != targetNode && styledElement) {
                // If child has children, wrap children of child by a clone of the styled element to avoid infinite loop.
                // Otherwise, wrap the child by the styled element, and we won't fall into an infinite loop.
                RefPtr<Element> wrapper = styledElement->cloneElementWithoutChildren();
                ExceptionCode ec = 0;
                wrapper->removeAttribute(styleAttr, ec);
                ASSERT(!ec);
                if (child->firstChild())
                    surroundNodeRangeWithElement(child->firstChild(), child->lastChild(), wrapper);
                else
                    surroundNodeRangeWithElement(child, child, wrapper);
            }

            // Apply text decoration to all nodes containing targetNode and their siblings but NOT to targetNode
            // But if we've removed styledElement then go ahead and always apply the style.
            if (child != targetNode || styledElement)
                applyInlineStyleToPushDown(child, styleToPushDown.get());

            // We found the next node for the outer loop (contains targetNode)
            // When reached targetNode, stop the outer loop upon the completion of the current inner loop
            if (child == targetNode || child->contains(targetNode))
                current = child;

            if (child == lastChild || child->contains(lastChild))
                break;
            child = nextChild;
        }
    }
}

void ApplyStyleCommand::removeInlineStyle(PassRefPtr<CSSMutableStyleDeclaration> style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(comparePositions(start, end) <= 0);
    
    RefPtr<CSSValue> textDecorationSpecialProperty = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);
    if (textDecorationSpecialProperty) {
        style = style->copy();
        style->setProperty(CSSPropertyTextDecoration, textDecorationSpecialProperty->cssText(), style->getPropertyPriority(CSSPropertyWebkitTextDecorationsInEffect));
    }

    Position pushDownStart = start.downstream();
    // If the pushDownStart is at the end of a text node, then this node is not fully selected.
    // Move it to the next deep quivalent position to avoid removing the style from this node.
    // e.g. if pushDownStart was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead.
    Node* pushDownStartContainer = pushDownStart.containerNode();
    if (pushDownStartContainer && pushDownStartContainer->isTextNode()
        && pushDownStart.computeOffsetInContainerNode() == pushDownStartContainer->maxCharacterOffset())
        pushDownStart = nextVisuallyDistinctCandidate(pushDownStart);
    Position pushDownEnd = end.upstream();
    pushDownInlineStyleAroundNode(style.get(), pushDownStart.node());
    pushDownInlineStyleAroundNode(style.get(), pushDownEnd.node());

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    // If pushDownInlineStyleAroundNode has pruned start.node() or end.node(),
    // use pushDownStart or pushDownEnd instead, which pushDownInlineStyleAroundNode won't prune.
    Position s = start.isNull() || start.isOrphan() ? pushDownStart : start;
    Position e = end.isNull() || end.isOrphan() ? pushDownEnd : end;

    Node* node = start.node();
    while (node) {
        Node* next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(node, start, end)) {
            HTMLElement* elem = static_cast<HTMLElement*>(node);
            Node* prev = elem->traversePreviousNodePostOrder();
            Node* next = elem->traverseNextNode();
            removeInlineStyleFromElement(style.get(), elem);
            if (!elem->inDocument()) {
                if (s.node() == elem) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    ASSERT(s.deprecatedEditingOffset() <= caretMinOffset(s.node()));
                    s = Position(next, 0);
                }
                if (e.node() == elem) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    ASSERT(e.deprecatedEditingOffset() >= lastOffsetForEditing(e.node()));
                    e = Position(prev, lastOffsetForEditing(prev));
                }
            }
        }
        if (node == end.node())
            break;
        node = next;
    }
    
    ASSERT(s.node()->inDocument());
    ASSERT(e.node()->inDocument());
    updateStartEnd(s, e);
}

bool ApplyStyleCommand::nodeFullySelected(Node *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    return comparePositions(Position(node, 0), start) >= 0 && comparePositions(pos, end) <= 0;
}

bool ApplyStyleCommand::nodeFullyUnselected(Node *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    bool isFullyBeforeStart = comparePositions(pos, start) < 0;
    bool isFullyAfterEnd = comparePositions(Position(node, 0), end) > 0;

    return isFullyBeforeStart || isFullyAfterEnd;
}

void ApplyStyleCommand::splitTextAtStart(const Position& start, const Position& end)
{
    int endOffsetAdjustment = start.node() == end.node() ? start.deprecatedEditingOffset() : 0;
    Text* text = static_cast<Text*>(start.node());
    splitTextNode(text, start.deprecatedEditingOffset());
    updateStartEnd(Position(start.node(), 0), Position(end.node(), end.deprecatedEditingOffset() - endOffsetAdjustment));
}

void ApplyStyleCommand::splitTextAtEnd(const Position& start, const Position& end)
{
    Text* text = static_cast<Text *>(end.node());
    splitTextNode(text, end.deprecatedEditingOffset());

    Node* prevNode = text->previousSibling();
    ASSERT(prevNode);
    Node* startNode = start.node() == end.node() ? prevNode : start.node();
    ASSERT(startNode);
    updateStartEnd(Position(startNode, start.deprecatedEditingOffset()), Position(prevNode, caretMaxOffset(prevNode)));
}

void ApplyStyleCommand::splitTextElementAtStart(const Position& start, const Position& end)
{
    int endOffsetAdjustment = start.node() == end.node() ? start.deprecatedEditingOffset() : 0;
    Text* text = static_cast<Text*>(start.node());
    splitTextNodeContainingElement(text, start.deprecatedEditingOffset());
    updateStartEnd(Position(start.node()->parentNode(), start.node()->nodeIndex()), Position(end.node(), end.deprecatedEditingOffset() - endOffsetAdjustment));
}

void ApplyStyleCommand::splitTextElementAtEnd(const Position& start, const Position& end)
{
    Text* text = static_cast<Text*>(end.node());
    splitTextNodeContainingElement(text, end.deprecatedEditingOffset());

    Node* prevNode = text->parent()->previousSibling()->lastChild();
    ASSERT(prevNode);
    Node* startNode = start.node() == end.node() ? prevNode : start.node();
    ASSERT(startNode);
    updateStartEnd(Position(startNode, start.deprecatedEditingOffset()), Position(prevNode->parent(), prevNode->nodeIndex() + 1));
}

bool ApplyStyleCommand::shouldSplitTextElement(Element* element, CSSMutableStyleDeclaration* style)
{
    if (!element || !element->isHTMLElement() || !element->parentElement() || !element->parentElement()->isContentEditable())
        return false;

    return shouldRemoveInlineStyleFromElement(style, static_cast<HTMLElement*>(element));
}

bool ApplyStyleCommand::isValidCaretPositionInTextNode(const Position& position)
{
    Node* node = position.node();
    if (!node->isTextNode())
        return false;
    int offsetInText = position.deprecatedEditingOffset();
    return (offsetInText > caretMinOffset(node) && offsetInText < caretMaxOffset(node));
}

static bool areIdenticalElements(Node *first, Node *second)
{
    // check that tag name and all attribute names and values are identical

    if (!first->isElementNode())
        return false;
    
    if (!second->isElementNode())
        return false;

    Element *firstElement = static_cast<Element *>(first);
    Element *secondElement = static_cast<Element *>(second);
    
    if (!firstElement->tagQName().matches(secondElement->tagQName()))
        return false;

    NamedNodeMap *firstMap = firstElement->attributes();
    NamedNodeMap *secondMap = secondElement->attributes();

    unsigned firstLength = firstMap->length();

    if (firstLength != secondMap->length())
        return false;

    for (unsigned i = 0; i < firstLength; i++) {
        Attribute *attribute = firstMap->attributeItem(i);
        Attribute *secondAttribute = secondMap->getAttributeItem(attribute->name());

        if (!secondAttribute || attribute->value() != secondAttribute->value())
            return false;
    }
    
    return true;
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position &start, const Position &end)
{
    Node *startNode = start.node();
    int startOffset = start.deprecatedEditingOffset();

    if (isAtomicNode(start.node())) {
        if (start.deprecatedEditingOffset() != 0)
            return false;

        // note: prior siblings could be unrendered elements. it's silly to miss the
        // merge opportunity just for that.
        if (start.node()->previousSibling())
            return false;

        startNode = start.node()->parent();
        startOffset = 0;
    }

    if (!startNode->isElementNode())
        return false;

    if (startOffset != 0)
        return false;

    Node *previousSibling = startNode->previousSibling();

    if (previousSibling && areIdenticalElements(startNode, previousSibling)) {
        Element *previousElement = static_cast<Element *>(previousSibling);
        Element *element = static_cast<Element *>(startNode);
        Node *startChild = element->firstChild();
        ASSERT(startChild);
        mergeIdenticalElements(previousElement, element);

        int startOffsetAdjustment = startChild->nodeIndex();
        int endOffsetAdjustment = startNode == end.node() ? startOffsetAdjustment : 0;
        updateStartEnd(Position(startNode, startOffsetAdjustment), Position(end.node(), end.deprecatedEditingOffset() + endOffsetAdjustment)); 
        return true;
    }

    return false;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position &start, const Position &end)
{
    Node *endNode = end.node();
    int endOffset = end.deprecatedEditingOffset();

    if (isAtomicNode(endNode)) {
        if (endOffset < caretMaxOffset(endNode))
            return false;

        unsigned parentLastOffset = end.node()->parent()->childNodes()->length() - 1;
        if (end.node()->nextSibling())
            return false;

        endNode = end.node()->parent();
        endOffset = parentLastOffset;
    }

    if (!endNode->isElementNode() || endNode->hasTagName(brTag))
        return false;

    Node *nextSibling = endNode->nextSibling();

    if (nextSibling && areIdenticalElements(endNode, nextSibling)) {
        Element *nextElement = static_cast<Element *>(nextSibling);
        Element *element = static_cast<Element *>(endNode);
        Node *nextChild = nextElement->firstChild();

        mergeIdenticalElements(element, nextElement);

        Node *startNode = start.node() == endNode ? nextElement : start.node();
        ASSERT(startNode);

        int endOffset = nextChild ? nextChild->nodeIndex() : nextElement->childNodes()->length();
        updateStartEnd(Position(startNode, start.deprecatedEditingOffset()), Position(nextElement, endOffset));
        return true;
    }

    return false;
}

void ApplyStyleCommand::surroundNodeRangeWithElement(Node* startNode, Node* endNode, PassRefPtr<Element> elementToInsert)
{
    ASSERT(startNode);
    ASSERT(endNode);
    ASSERT(elementToInsert);
    RefPtr<Element> element = elementToInsert;

    insertNodeBefore(element, startNode);
    
    Node* node = startNode;
    while (1) {
        Node* next = node->nextSibling();
        removeNode(node);
        appendNode(node, element);
        if (node == endNode)
            break;
        node = next;
    }

    Node* nextSibling = element->nextSibling();
    Node* previousSibling = element->previousSibling();
    if (nextSibling && nextSibling->isElementNode() && nextSibling->isContentEditable()
        && areIdenticalElements(element.get(), static_cast<Element*>(nextSibling)))
        mergeIdenticalElements(element, static_cast<Element*>(nextSibling));

    if (previousSibling && previousSibling->isElementNode() && previousSibling->isContentEditable()) {
        Node* mergedElement = previousSibling->nextSibling();
        if (mergedElement->isElementNode() && mergedElement->isContentEditable()
            && areIdenticalElements(static_cast<Element*>(previousSibling), static_cast<Element*>(mergedElement)))
            mergeIdenticalElements(static_cast<Element*>(previousSibling), static_cast<Element*>(mergedElement));
    }

    // FIXME: We should probably call updateStartEnd if the start or end was in the node
    // range so that the endingSelection() is canonicalized.  See the comments at the end of
    // VisibleSelection::validate().
}

void ApplyStyleCommand::addBlockStyle(const StyleChange& styleChange, HTMLElement* block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for
    // inline content.
    if (!block)
        return;
        
    String cssText = styleChange.cssStyle();
    CSSMutableStyleDeclaration* decl = block->inlineStyleDecl();
    if (decl)
        cssText += decl->cssText();
    setNodeAttribute(block, styleAttr, cssText);
}

void ApplyStyleCommand::addInlineStyleIfNeeded(CSSMutableStyleDeclaration *style, Node *startNode, Node *endNode, EAddStyledElement addStyledElement)
{
    StyleChange styleChange(style, Position(startNode, 0));

    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        RefPtr<Element> fontElement = createFontElement(document());

        if (styleChange.applyFontColor())
            fontElement->setAttribute(colorAttr, styleChange.fontColor());
        if (styleChange.applyFontFace())
            fontElement->setAttribute(faceAttr, styleChange.fontFace());
        if (styleChange.applyFontSize())
            fontElement->setAttribute(sizeAttr, styleChange.fontSize());

        surroundNodeRangeWithElement(startNode, endNode, fontElement.get());
    }

    if (styleChange.cssStyle().length()) {
        RefPtr<Element> styleElement = createStyleSpanElement(document());
        styleElement->setAttribute(styleAttr, styleChange.cssStyle());
        surroundNodeRangeWithElement(startNode, endNode, styleElement.release());
    }

    if (styleChange.applyBold())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), bTag));

    if (styleChange.applyItalic())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), iTag));

    if (styleChange.applyUnderline())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), uTag));

    if (styleChange.applyLineThrough())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), sTag));

    if (styleChange.applySubscript())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), subTag));
    else if (styleChange.applySuperscript())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), supTag));

    if (m_styledInlineElement && addStyledElement == AddStyledElement)
        surroundNodeRangeWithElement(startNode, endNode, m_styledInlineElement->cloneElementWithoutChildren());
}

float ApplyStyleCommand::computedFontSize(const Node *node)
{
    if (!node)
        return 0;
    
    Position pos(const_cast<Node *>(node), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    if (!computedStyle)
        return 0;

    RefPtr<CSSPrimitiveValue> value = static_pointer_cast<CSSPrimitiveValue>(computedStyle->getPropertyCSSValue(CSSPropertyFontSize));
    if (!value)
        return 0;

    return value->getFloatValue(CSSPrimitiveValue::CSS_PX);
}

void ApplyStyleCommand::joinChildTextNodes(Node *node, const Position &start, const Position &end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;
    
    Node *child = node->firstChild();
    while (child) {
        Node *next = child->nextSibling();
        if (child->isTextNode() && next && next->isTextNode()) {
            Text *childText = static_cast<Text *>(child);
            Text *nextText = static_cast<Text *>(next);
            if (next == start.node())
                newStart = Position(childText, childText->length() + start.deprecatedEditingOffset());
            if (next == end.node())
                newEnd = Position(childText, childText->length() + end.deprecatedEditingOffset());
            String textToMove = nextText->data();
            insertTextIntoNode(childText, childText->length(), textToMove);
            removeNode(next);
            // don't move child node pointer. it may want to merge with more text nodes.
        }
        else {
            child = child->nextSibling();
        }
    }

    updateStartEnd(newStart, newEnd);
}

}

/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "AnimationList.h"
#include "ApplePayButtonSystemImage.h"
#include "BorderValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CounterDirectives.h"
#include "FilterOperations.h"
#include "FontCascadeDescription.h"
#include "GapLength.h"
#include "GraphicsTypes.h"
#include "Length.h"
#include "LengthBox.h"
#include "LengthFunctions.h"
#include "LengthPoint.h"
#include "LengthSize.h"
#include "LineClampValue.h"
#include "NinePieceImage.h"
#include "OffsetRotation.h"
#include "Pagination.h"
#include "RenderStyleConstants.h"
#include "RotateTransformOperation.h"
#include "RoundedRect.h"
#include "SVGRenderStyle.h"
#include "ScaleTransformOperation.h"
#include "ScrollTypes.h"
#include "ShadowData.h"
#include "ShapeValue.h"
#include "StyleBackgroundData.h"
#include "StyleBoxData.h"
#include "StyleColor.h"
#include "StyleDeprecatedFlexibleBoxData.h"
#include "StyleFilterData.h"
#include "StyleFlexibleBoxData.h"
#include "StyleMarqueeData.h"
#include "StyleMultiColData.h"
#include "StyleRareInheritedData.h"
#include "StyleRareNonInheritedData.h"
#include "StyleReflection.h"
#include "StyleSurroundData.h"
#include "StyleTransformData.h"
#include "StyleVisualData.h"
#include "TextFlags.h"
#include "ThemeTypes.h"
#include "TouchAction.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include "UnicodeBidi.h"
#include <memory>
#include <wtf/DataRef.h>
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#include "StyleGridData.h"
#include "StyleGridItemData.h"

#if ENABLE(TEXT_AUTOSIZING)
#include "TextSizeAdjustment.h"
#endif

#if ENABLE(DARK_MODE_CSS)
#include "StyleColorScheme.h"
#endif

#define SET_VAR(group, variable, value) do { \
        if (!compareEqual(group->variable, value)) \
            group.access().variable = value; \
    } while (0)

#define SET_NESTED_VAR(group, parentVariable, variable, value) do { \
        if (!compareEqual(group->parentVariable->variable, value)) \
            group.access().parentVariable.access().variable = value; \
    } while (0)

#define SET_BORDERVALUE_COLOR(group, variable, value) do { \
        if (!compareEqual(group->variable.color(), value)) \
            group.access().variable.setColor(value); \
    } while (0)

namespace WebCore {

class BorderData;
class ContentData;
class CounterContent;
class CursorList;
class FontCascade;
class FontMetrics;
class IntRect;
class Pair;
class ShadowData;
class StyleImage;
class StyleInheritedData;
class StyleScrollSnapArea;
class TransformationMatrix;

struct ScrollSnapAlign;
struct ScrollSnapType;

using PseudoStyleCache = Vector<std::unique_ptr<RenderStyle>, 4>;

template<typename T, typename U> inline bool compareEqual(const T& t, const U& u) { return t == static_cast<const T&>(u); }

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyle);
class RenderStyle {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(RenderStyle);
private:
    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

public:
    RenderStyle(RenderStyle&&);
    RenderStyle& operator=(RenderStyle&&);
    WEBCORE_EXPORT ~RenderStyle();

    RenderStyle replace(RenderStyle&&) WARN_UNUSED_RETURN;

    explicit RenderStyle(CreateDefaultStyleTag);
    RenderStyle(const RenderStyle&, CloneTag);

    static RenderStyle& defaultStyle();

    static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();

    static RenderStyle clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool operator==(const RenderStyle&) const;
    bool operator!=(const RenderStyle& other) const { return !(*this == other); }

    void inheritFrom(const RenderStyle&);
    void fastPathInheritFrom(const RenderStyle&);
    void copyNonInheritedFrom(const RenderStyle&);
    void copyContentFrom(const RenderStyle&);
    void copyPseudoElementsFrom(const RenderStyle&);

    ContentPosition resolvedJustifyContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistribution resolvedJustifyContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentPosition resolvedAlignContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistribution resolvedAlignContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    StyleSelfAlignmentData resolvedAlignItems(ItemPosition normalValueBehaviour) const;
    StyleSelfAlignmentData resolvedAlignSelf(const RenderStyle* parentStyle, ItemPosition normalValueBehaviour) const;
    StyleContentAlignmentData resolvedAlignContent(const StyleContentAlignmentData& normalValueBehaviour) const;
    StyleSelfAlignmentData resolvedJustifyItems(ItemPosition normalValueBehaviour) const;
    StyleSelfAlignmentData resolvedJustifySelf(const RenderStyle* parentStyle, ItemPosition normalValueBehaviour) const;
    StyleContentAlignmentData resolvedJustifyContent(const StyleContentAlignmentData& normalValueBehaviour) const;

    PseudoId styleType() const { return static_cast<PseudoId>(m_nonInheritedFlags.styleType); }
    void setStyleType(PseudoId styleType) { m_nonInheritedFlags.styleType = static_cast<unsigned>(styleType); }

    RenderStyle* getCachedPseudoStyle(PseudoId) const;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    void deduplicateInheritedCustomProperties(const RenderStyle&);
    const CustomPropertyValueMap& inheritedCustomProperties() const { return m_rareInheritedData->customProperties->values; }
    const CustomPropertyValueMap& nonInheritedCustomProperties() const { return m_rareNonInheritedData->customProperties->values; }
    const CSSCustomPropertyValue* getCustomProperty(const AtomString&) const;
    void setInheritedCustomPropertyValue(const AtomString& name, Ref<CSSCustomPropertyValue>&&);
    void setNonInheritedCustomPropertyValue(const AtomString& name, Ref<CSSCustomPropertyValue>&&);

    void setUsesViewportUnits() { m_nonInheritedFlags.usesViewportUnits = true; }
    bool usesViewportUnits() const { return m_nonInheritedFlags.usesViewportUnits; }
    void setUsesContainerUnits() { m_nonInheritedFlags.usesContainerUnits = true; }
    bool usesContainerUnits() const { return m_nonInheritedFlags.usesContainerUnits; }

    void setColumnStylesFromPaginationMode(const Pagination::Mode&);
    
    bool isFloating() const { return static_cast<Float>(m_nonInheritedFlags.floating) != Float::None; }
    bool hasMargin() const { return !m_surroundData->margin.isZero(); }
    bool hasBorder() const { return m_surroundData->border.hasBorder(); }
    bool hasBorderImage() const { return m_surroundData->border.hasBorderImage(); }
    bool hasVisibleBorderDecoration() const { return hasVisibleBorder() || hasBorderImage(); }
    bool hasVisibleBorder() const { return m_surroundData->border.hasVisibleBorder(); }
    bool hasPadding() const { return !m_surroundData->padding.isZero(); }
    bool hasOffset() const { return !m_surroundData->offset.isZero(); }
    bool hasMarginBeforeQuirk() const { return marginBefore().hasQuirk(); }
    bool hasMarginAfterQuirk() const { return marginAfter().hasQuirk(); }

    bool hasBackgroundImage() const { return backgroundLayers().hasImage(); }
    bool hasAnyFixedBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::FixedBackground); }

    bool hasEntirelyFixedBackground() const;
    bool hasAnyLocalBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::LocalBackground); }

    bool hasAppearance() const { return appearance() != NoControlPart; }
    bool hasEffectiveAppearance() const { return effectiveAppearance() != NoControlPart; }

    bool hasBackground() const;
    
    LayoutBoxExtent imageOutsets(const NinePieceImage&) const;
    bool hasBorderImageOutsets() const { return borderImage().hasImage() && !borderImage().outset().isZero(); }
    LayoutBoxExtent borderImageOutsets() const { return imageOutsets(borderImage()); }

    LayoutBoxExtent maskBoxImageOutsets() const { return imageOutsets(maskBoxImage()); }

    IntOutsets filterOutsets() const { return hasFilter() ? filter().outsets() : IntOutsets(); }

    Order rtlOrdering() const { return static_cast<Order>(m_inheritedFlags.rtlOrdering); }
    void setRTLOrdering(Order ordering) { m_inheritedFlags.rtlOrdering = static_cast<unsigned>(ordering); }

    bool isStyleAvailable() const;

    bool hasAnyPublicPseudoStyles() const;
    bool hasPseudoStyle(PseudoId) const;
    void setHasPseudoStyles(PseudoIdSet);
    bool hasUniquePseudoStyle() const;

    // attribute getter methods

    DisplayType display() const { return static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay); }

    const Length& left() const { return m_surroundData->offset.left(); }
    const Length& right() const { return m_surroundData->offset.right(); }
    const Length& top() const { return m_surroundData->offset.top(); }
    const Length& bottom() const { return m_surroundData->offset.bottom(); }

    // Accessors for positioned object edges that take into account writing mode.
    const Length& logicalLeft() const { return m_surroundData->offset.start(writingMode()); }
    const Length& logicalRight() const { return m_surroundData->offset.end(writingMode()); }
    const Length& logicalTop() const { return m_surroundData->offset.before(writingMode()); }
    const Length& logicalBottom() const { return m_surroundData->offset.after(writingMode()); }

    // Whether or not a positioned element requires normal flow x/y to be computed  to determine its position.
    bool hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
    bool hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }

    PositionType position() const { return static_cast<PositionType>(m_nonInheritedFlags.position); }
    bool hasOutOfFlowPosition() const { return position() == PositionType::Absolute || position() == PositionType::Fixed; }
    bool hasInFlowPosition() const { return position() == PositionType::Relative || position() == PositionType::Sticky; }
    bool hasViewportConstrainedPosition() const { return position() == PositionType::Fixed || position() == PositionType::Sticky; }
    Float floating() const { return static_cast<Float>(m_nonInheritedFlags.floating); }
    static UsedFloat usedFloat(const RenderObject&);

    const Length& width() const { return m_boxData->width(); }
    const Length& height() const { return m_boxData->height(); }
    const Length& minWidth() const { return m_boxData->minWidth(); }
    const Length& maxWidth() const { return m_boxData->maxWidth(); }
    const Length& minHeight() const { return m_boxData->minHeight(); }
    const Length& maxHeight() const { return m_boxData->maxHeight(); }
    
    const Length& logicalWidth() const { return isHorizontalWritingMode() ? width() : height(); }
    const Length& logicalHeight() const { return isHorizontalWritingMode() ? height() : width(); }
    const Length& logicalMinWidth() const { return isHorizontalWritingMode() ? minWidth() : minHeight(); }
    const Length& logicalMaxWidth() const { return isHorizontalWritingMode() ? maxWidth() : maxHeight(); }
    const Length& logicalMinHeight() const { return isHorizontalWritingMode() ? minHeight() : minWidth(); }
    const Length& logicalMaxHeight() const { return isHorizontalWritingMode() ? maxHeight() : maxWidth(); }

    const BorderData& border() const { return m_surroundData->border; }
    const BorderValue& borderLeft() const { return m_surroundData->border.left(); }
    const BorderValue& borderRight() const { return m_surroundData->border.right(); }
    const BorderValue& borderTop() const { return m_surroundData->border.top(); }
    const BorderValue& borderBottom() const { return m_surroundData->border.bottom(); }

    const BorderValue& borderBefore() const;
    const BorderValue& borderAfter() const;
    const BorderValue& borderStart() const;
    const BorderValue& borderEnd() const;

    const NinePieceImage& borderImage() const { return m_surroundData->border.image(); }
    StyleImage* borderImageSource() const { return m_surroundData->border.image().image(); }
    const LengthBox& borderImageSlices() const { return m_surroundData->border.image().imageSlices(); }
    const LengthBox& borderImageWidth() const { return m_surroundData->border.image().borderSlices(); }
    const LengthBox& borderImageOutset() const { return m_surroundData->border.image().outset(); }
    NinePieceImageRule borderImageHorizontalRule() const { return m_surroundData->border.image().horizontalRule(); }
    NinePieceImageRule borderImageVerticalRule() const { return m_surroundData->border.image().verticalRule(); }

    const LengthSize& borderTopLeftRadius() const { return m_surroundData->border.topLeftRadius(); }
    const LengthSize& borderTopRightRadius() const { return m_surroundData->border.topRightRadius(); }
    const LengthSize& borderBottomLeftRadius() const { return m_surroundData->border.bottomLeftRadius(); }
    const LengthSize& borderBottomRightRadius() const { return m_surroundData->border.bottomRightRadius(); }
    const BorderData::Radii& borderRadii() const { return m_surroundData->border.m_radii; }
    bool hasBorderRadius() const { return m_surroundData->border.hasBorderRadius(); }
    bool hasExplicitlySetBorderBottomLeftRadius() const { return m_nonInheritedFlags.hasExplicitlySetBorderBottomLeftRadius; }
    bool hasExplicitlySetBorderBottomRightRadius() const { return m_nonInheritedFlags.hasExplicitlySetBorderBottomRightRadius; }
    bool hasExplicitlySetBorderTopLeftRadius() const { return m_nonInheritedFlags.hasExplicitlySetBorderTopLeftRadius; }
    bool hasExplicitlySetBorderTopRightRadius() const { return m_nonInheritedFlags.hasExplicitlySetBorderTopRightRadius; }
    bool hasExplicitlySetBorderRadius() const { return hasExplicitlySetBorderBottomLeftRadius() || hasExplicitlySetBorderBottomRightRadius() || hasExplicitlySetBorderTopLeftRadius() || hasExplicitlySetBorderTopRightRadius(); }

    float borderLeftWidth() const { return m_surroundData->border.borderLeftWidth(); }
    BorderStyle borderLeftStyle() const { return m_surroundData->border.left().style(); }
    bool borderLeftIsTransparent() const { return m_surroundData->border.left().isTransparent(); }
    float borderRightWidth() const { return m_surroundData->border.borderRightWidth(); }
    BorderStyle borderRightStyle() const { return m_surroundData->border.right().style(); }
    bool borderRightIsTransparent() const { return m_surroundData->border.right().isTransparent(); }
    float borderTopWidth() const { return m_surroundData->border.borderTopWidth(); }
    BorderStyle borderTopStyle() const { return m_surroundData->border.top().style(); }
    bool borderTopIsTransparent() const { return m_surroundData->border.top().isTransparent(); }
    float borderBottomWidth() const { return m_surroundData->border.borderBottomWidth(); }
    BorderStyle borderBottomStyle() const { return m_surroundData->border.bottom().style(); }
    bool borderBottomIsTransparent() const { return m_surroundData->border.bottom().isTransparent(); }
    FloatBoxExtent borderWidth() const { return m_surroundData->border.borderWidth(); }

    float borderBeforeWidth() const;
    float borderAfterWidth() const;
    float borderStartWidth() const;
    float borderEndWidth() const;

    float outlineSize() const { return std::max<float>(0, outlineWidth() + outlineOffset()); }
    float outlineWidth() const;
    bool hasOutline() const { return outlineStyle() > BorderStyle::Hidden && outlineWidth() > 0; }
    BorderStyle outlineStyle() const { return m_backgroundData->outline.style(); }
    OutlineIsAuto outlineStyleIsAuto() const { return static_cast<OutlineIsAuto>(m_backgroundData->outline.isAuto()); }
    bool hasOutlineInVisualOverflow() const { return hasOutline() && outlineSize() > 0; }
    
    Overflow overflowX() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowX); }
    Overflow overflowY() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowY); }
    bool isOverflowVisible() const { return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible; }

    OverscrollBehavior overscrollBehaviorX() const { return static_cast<OverscrollBehavior>(m_rareNonInheritedData->overscrollBehaviorX); }
    OverscrollBehavior overscrollBehaviorY() const { return static_cast<OverscrollBehavior>(m_rareNonInheritedData->overscrollBehaviorY); }
    
    Visibility visibility() const { return static_cast<Visibility>(m_inheritedFlags.visibility); }
    VerticalAlign verticalAlign() const { return static_cast<VerticalAlign>(m_nonInheritedFlags.verticalAlign); }
    const Length& verticalAlignLength() const { return m_boxData->verticalAlign(); }

    const Length& clipLeft() const { return m_visualData->clip.left(); }
    const Length& clipRight() const { return m_visualData->clip.right(); }
    const Length& clipTop() const { return m_visualData->clip.top(); }
    const Length& clipBottom() const { return m_visualData->clip.bottom(); }
    const LengthBox& clip() const { return m_visualData->clip; }
    bool hasClip() const { return m_visualData->hasClip; }

    UnicodeBidi unicodeBidi() const { return static_cast<UnicodeBidi>(m_nonInheritedFlags.unicodeBidi); }

    Clear clear() const { return static_cast<Clear>(m_nonInheritedFlags.clear); }
    static UsedClear usedClear(const RenderObject&);
    TableLayoutType tableLayout() const { return static_cast<TableLayoutType>(m_nonInheritedFlags.tableLayout); }

    WEBCORE_EXPORT const FontCascade& fontCascade() const;
    WEBCORE_EXPORT const FontMetrics& metricsOfPrimaryFont() const;
    WEBCORE_EXPORT const FontCascadeDescription& fontDescription() const;
    float specifiedFontSize() const;
    float computedFontSize() const;
    unsigned computedFontPixelSize() const;
    std::optional<float> fontSizeAdjust() const { return fontDescription().fontSizeAdjust(); }
    std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();

    FontVariationSettings fontVariationSettings() const { return fontDescription().variationSettings(); }
    FontSelectionValue fontWeight() const { return fontDescription().weight(); }
    FontSelectionValue fontStretch() const { return fontDescription().stretch(); }
    std::optional<FontSelectionValue> fontItalic() const { return fontDescription().italic(); }
    FontPalette fontPalette() const { return fontDescription().fontPalette(); }

    const Length& textIndent() const { return m_rareInheritedData->indent; }
    TextAlignMode textAlign() const { return static_cast<TextAlignMode>(m_inheritedFlags.textAlign); }
    TextAlignLast textAlignLast() const { return static_cast<TextAlignLast>(m_rareInheritedData->textAlignLast); }
    TextTransform textTransform() const { return static_cast<TextTransform>(m_inheritedFlags.textTransform); }
    OptionSet<TextDecorationLine> textDecorationsInEffect() const { return OptionSet<TextDecorationLine>::fromRaw(m_inheritedFlags.textDecorationLines); }
    OptionSet<TextDecorationLine> textDecorationLine() const { return OptionSet<TextDecorationLine>::fromRaw(m_visualData->textDecorationLine); }
    TextDecorationStyle textDecorationStyle() const { return static_cast<TextDecorationStyle>(m_rareNonInheritedData->textDecorationStyle); }
    TextDecorationSkipInk textDecorationSkipInk() const { return static_cast<TextDecorationSkipInk>(m_rareInheritedData->textDecorationSkipInk); }
    TextUnderlinePosition textUnderlinePosition() const { return static_cast<TextUnderlinePosition>(m_rareInheritedData->textUnderlinePosition); }
    TextUnderlineOffset textUnderlineOffset() const { return m_rareInheritedData->textUnderlineOffset; }
    TextDecorationThickness textDecorationThickness() const { return m_rareNonInheritedData->textDecorationThickness; }

    TextIndentLine textIndentLine() const { return static_cast<TextIndentLine>(m_rareInheritedData->textIndentLine); }
    TextIndentType textIndentType() const { return static_cast<TextIndentType>(m_rareInheritedData->textIndentType); }
    TextJustify textJustify() const { return static_cast<TextJustify>(m_rareInheritedData->textJustify); }

    const Length& wordSpacing() const;
    float letterSpacing() const;

    float zoom() const { return m_visualData->zoom; }
    float effectiveZoom() const { return m_rareInheritedData->effectiveZoom; }
    
    TextZoom textZoom() const { return static_cast<TextZoom>(m_rareInheritedData->textZoom); }

    TextDirection direction() const { return static_cast<TextDirection>(m_inheritedFlags.direction); }
    bool isLeftToRightDirection() const { return direction() == TextDirection::LTR; }
    bool hasExplicitlySetDirection() const { return m_nonInheritedFlags.hasExplicitlySetDirection; }

    const Length& specifiedLineHeight() const;
    WEBCORE_EXPORT const Length& lineHeight() const;
    WEBCORE_EXPORT int computedLineHeight() const;
    int computeLineHeight(const Length&) const;

    WhiteSpace whiteSpace() const { return static_cast<WhiteSpace>(m_inheritedFlags.whiteSpace); }
    static bool autoWrap(WhiteSpace);
    bool autoWrap() const { return autoWrap(whiteSpace()); }
    static bool preserveNewline(WhiteSpace);
    bool preserveNewline() const { return preserveNewline(whiteSpace()); }
    static bool collapseWhiteSpace(WhiteSpace);
    bool collapseWhiteSpace() const { return collapseWhiteSpace(whiteSpace()); }
    bool isCollapsibleWhiteSpace(UChar) const;
    bool breakOnlyAfterWhiteSpace() const;
    bool breakWords() const;

    FillRepeatXY backgroundRepeat() const { return m_backgroundData->background->repeat(); }
    FillAttachment backgroundAttachment() const { return static_cast<FillAttachment>(m_backgroundData->background->attachment()); }
    FillBox backgroundClip() const { return static_cast<FillBox>(m_backgroundData->background->clip()); }
    FillBox backgroundOrigin() const { return static_cast<FillBox>(m_backgroundData->background->origin()); }
    const Length& backgroundXPosition() const { return m_backgroundData->background->xPosition(); }
    const Length& backgroundYPosition() const { return m_backgroundData->background->yPosition(); }
    FillSizeType backgroundSizeType() const { return m_backgroundData->background->sizeType(); }
    const LengthSize& backgroundSizeLength() const { return m_backgroundData->background->sizeLength(); }
    FillLayer& ensureBackgroundLayers() { return m_backgroundData.access().background.access(); }
    const FillLayer& backgroundLayers() const { return m_backgroundData->background; }
    BlendMode backgroundBlendMode() const { return static_cast<BlendMode>(m_backgroundData->background->blendMode()); }

    StyleImage* maskImage() const { return m_rareNonInheritedData->mask->image(); }
    FillRepeatXY maskRepeat() const { return m_rareNonInheritedData->mask->repeat(); }
    CompositeOperator maskComposite() const { return static_cast<CompositeOperator>(m_rareNonInheritedData->mask->composite()); }
    FillBox maskClip() const { return static_cast<FillBox>(m_rareNonInheritedData->mask->clip()); }
    FillBox maskOrigin() const { return static_cast<FillBox>(m_rareNonInheritedData->mask->origin()); }
    const Length& maskXPosition() const { return m_rareNonInheritedData->mask->xPosition(); }
    const Length& maskYPosition() const { return m_rareNonInheritedData->mask->yPosition(); }
    FillSizeType maskSizeType() const { return m_rareNonInheritedData->mask->sizeType(); }
    const LengthSize& maskSizeLength() const { return m_rareNonInheritedData->mask->sizeLength(); }
    FillLayer& ensureMaskLayers() { return m_rareNonInheritedData.access().mask.access(); }
    const FillLayer& maskLayers() const { return m_rareNonInheritedData->mask; }
    const NinePieceImage& maskBoxImage() const { return m_rareNonInheritedData->maskBoxImage; }
    StyleImage* maskBoxImageSource() const { return m_rareNonInheritedData->maskBoxImage.image(); }

    BorderCollapse borderCollapse() const { return static_cast<BorderCollapse>(m_inheritedFlags.borderCollapse); }
    float horizontalBorderSpacing() const;
    float verticalBorderSpacing() const;
    EmptyCell emptyCells() const { return static_cast<EmptyCell>(m_inheritedFlags.emptyCells); }
    CaptionSide captionSide() const { return static_cast<CaptionSide>(m_inheritedFlags.captionSide); }

    const AtomString& listStyleStringValue() const { return m_rareInheritedData->listStyleStringValue; }
    ListStyleType listStyleType() const { return static_cast<ListStyleType>(m_inheritedFlags.listStyleType); }
    StyleImage* listStyleImage() const;
    ListStylePosition listStylePosition() const { return static_cast<ListStylePosition>(m_inheritedFlags.listStylePosition); }

    const Length& marginTop() const { return m_surroundData->margin.top(); }
    const Length& marginBottom() const { return m_surroundData->margin.bottom(); }
    const Length& marginLeft() const { return m_surroundData->margin.left(); }
    const Length& marginRight() const { return m_surroundData->margin.right(); }
    const Length& marginBefore() const { return m_surroundData->margin.before(writingMode()); }
    const Length& marginAfter() const { return m_surroundData->margin.after(writingMode()); }
    const Length& marginStart() const { return m_surroundData->margin.start(writingMode(), direction()); }
    const Length& marginEnd() const { return m_surroundData->margin.end(writingMode(), direction()); }
    const Length& marginStartUsing(const RenderStyle* otherStyle) const { return m_surroundData->margin.start(otherStyle->writingMode(), otherStyle->direction()); }
    const Length& marginEndUsing(const RenderStyle* otherStyle) const { return m_surroundData->margin.end(otherStyle->writingMode(), otherStyle->direction()); }
    const Length& marginBeforeUsing(const RenderStyle* otherStyle) const { return m_surroundData->margin.before(otherStyle->writingMode()); }
    const Length& marginAfterUsing(const RenderStyle* otherStyle) const { return m_surroundData->margin.after(otherStyle->writingMode()); }

    const LengthBox& paddingBox() const { return m_surroundData->padding; }
    const Length& paddingTop() const { return m_surroundData->padding.top(); }
    const Length& paddingBottom() const { return m_surroundData->padding.bottom(); }
    const Length& paddingLeft() const { return m_surroundData->padding.left(); }
    const Length& paddingRight() const { return m_surroundData->padding.right(); }
    const Length& paddingBefore() const { return m_surroundData->padding.before(writingMode()); }
    const Length& paddingAfter() const { return m_surroundData->padding.after(writingMode()); }
    const Length& paddingStart() const { return m_surroundData->padding.start(writingMode(), direction()); }
    const Length& paddingEnd() const { return m_surroundData->padding.end(writingMode(), direction()); }

    CursorType cursor() const { return static_cast<CursorType>(m_inheritedFlags.cursor); }

#if ENABLE(CURSOR_VISIBILITY)
    CursorVisibility cursorVisibility() const { return static_cast<CursorVisibility>(m_inheritedFlags.cursorVisibility); }
#endif

    CursorList* cursors() const { return m_rareInheritedData->cursorData.get(); }

    InsideLink insideLink() const { return static_cast<InsideLink>(m_inheritedFlags.insideLink); }
    bool isLink() const { return m_nonInheritedFlags.isLink; }

    bool insideDefaultButton() const { return m_inheritedFlags.insideDefaultButton; }

    unsigned short widows() const { return m_rareInheritedData->widows; }
    unsigned short orphans() const { return m_rareInheritedData->orphans; }
    bool hasAutoWidows() const { return m_rareInheritedData->hasAutoWidows; }
    bool hasAutoOrphans() const { return m_rareInheritedData->hasAutoOrphans; }

    BreakInside breakInside() const { return static_cast<BreakInside>(m_rareNonInheritedData->breakInside); }
    BreakBetween breakBefore() const { return static_cast<BreakBetween>(m_rareNonInheritedData->breakBefore); }
    BreakBetween breakAfter() const { return static_cast<BreakBetween>(m_rareNonInheritedData->breakAfter); }

    OptionSet<HangingPunctuation> hangingPunctuation() const { return OptionSet<HangingPunctuation>::fromRaw(m_rareInheritedData->hangingPunctuation); }

    float outlineOffset() const;
    const ShadowData* textShadow() const { return m_rareInheritedData->textShadow.get(); }
    LayoutBoxExtent textShadowExtent() const { return shadowExtent(textShadow()); }
    void getTextShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const { getShadowInlineDirectionExtent(textShadow(), logicalLeft, logicalRight); }
    void getTextShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const { getShadowBlockDirectionExtent(textShadow(), logicalTop, logicalBottom); }

    float textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
    float opacity() const { return m_rareNonInheritedData->opacity; }
    bool hasOpacity() const { return m_rareNonInheritedData->opacity < 1; }
    ControlPart appearance() const { return static_cast<ControlPart>(m_rareNonInheritedData->appearance); }
    ControlPart effectiveAppearance() const { return static_cast<ControlPart>(m_rareNonInheritedData->effectiveAppearance); }
    AspectRatioType aspectRatioType() const { return static_cast<AspectRatioType>(m_rareNonInheritedData->aspectRatioType); }
    double aspectRatioWidth() const { return m_rareNonInheritedData->aspectRatioWidth; }
    double aspectRatioHeight() const { return m_rareNonInheritedData->aspectRatioHeight; }
    double logicalAspectRatio() const
    {
        ASSERT(aspectRatioType() != AspectRatioType::Auto);
        if (isHorizontalWritingMode())
            return aspectRatioWidth() / aspectRatioHeight();
        return aspectRatioHeight() / aspectRatioWidth();
    }
    BoxSizing boxSizingForAspectRatio() const
    {
        if (aspectRatioType() == AspectRatioType::AutoAndRatio)
            return BoxSizing::ContentBox;
        return boxSizing();
    }
    bool hasAspectRatio() const { return aspectRatioType() == AspectRatioType::Ratio || aspectRatioType() == AspectRatioType::AutoAndRatio; }
    OptionSet<Containment> contain() const { return m_rareNonInheritedData->contain; }
    OptionSet<Containment> effectiveContainment() const { return m_rareNonInheritedData->effectiveContainment(); }
    bool containsLayout() const { return effectiveContainment().contains(Containment::Layout); }
    bool containsSize() const { return effectiveContainment().contains(Containment::Size); }
    bool containsInlineSize() const { return effectiveContainment().contains(Containment::InlineSize); }
    bool containsSizeOrInlineSize() const { return effectiveContainment().containsAny({ Containment::Size, Containment::InlineSize }); }
    bool containsStyle() const { return effectiveContainment().contains(Containment::Style); }
    bool containsPaint() const { return effectiveContainment().contains(Containment::Paint); }
    bool containsLayoutOrPaint() const { return effectiveContainment().containsAny({ Containment::Layout, Containment::Paint }); }
    ContainerType containerType() const { return static_cast<ContainerType>(m_rareNonInheritedData->containerType); }
    const Vector<AtomString>& containerNames() const { return m_rareNonInheritedData->containerNames; }

    ContentVisibility contentVisibility() const { return static_cast<ContentVisibility>(m_rareNonInheritedData->contentVisibility); }

    ContainIntrinsicSizeType containIntrinsicWidthType() const { return static_cast<ContainIntrinsicSizeType>(m_rareNonInheritedData->containIntrinsicWidthType); }
    ContainIntrinsicSizeType containIntrinsicHeightType() const { return static_cast<ContainIntrinsicSizeType>(m_rareNonInheritedData->containIntrinsicHeightType); }
    std::optional<Length> containIntrinsicWidth() const { return m_rareNonInheritedData->containIntrinsicWidth; }
    std::optional<Length> containIntrinsicHeight() const { return m_rareNonInheritedData->containIntrinsicHeight; }

    BoxAlignment boxAlign() const { return static_cast<BoxAlignment>(m_rareNonInheritedData->deprecatedFlexibleBox->align); }
    BoxDirection boxDirection() const { return static_cast<BoxDirection>(m_inheritedFlags.boxDirection); }
    float boxFlex() const { return m_rareNonInheritedData->deprecatedFlexibleBox->flex; }
    unsigned boxFlexGroup() const { return m_rareNonInheritedData->deprecatedFlexibleBox->flexGroup; }
    BoxLines boxLines() const { return static_cast<BoxLines>(m_rareNonInheritedData->deprecatedFlexibleBox->lines); }
    unsigned boxOrdinalGroup() const { return m_rareNonInheritedData->deprecatedFlexibleBox->ordinalGroup; }
    BoxOrient boxOrient() const { return static_cast<BoxOrient>(m_rareNonInheritedData->deprecatedFlexibleBox->orient); }
    BoxPack boxPack() const { return static_cast<BoxPack>(m_rareNonInheritedData->deprecatedFlexibleBox->pack); }

    int order() const { return m_rareNonInheritedData->order; }
    float flexGrow() const { return m_rareNonInheritedData->flexibleBox->flexGrow; }
    float flexShrink() const { return m_rareNonInheritedData->flexibleBox->flexShrink; }
    const Length& flexBasis() const { return m_rareNonInheritedData->flexibleBox->flexBasis; }
    const StyleContentAlignmentData& alignContent() const { return m_rareNonInheritedData->alignContent; }
    const StyleSelfAlignmentData& alignItems() const { return m_rareNonInheritedData->alignItems; }
    const StyleSelfAlignmentData& alignSelf() const { return m_rareNonInheritedData->alignSelf; }
    FlexDirection flexDirection() const { return static_cast<FlexDirection>(m_rareNonInheritedData->flexibleBox->flexDirection); }
    bool isColumnFlexDirection() const { return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse; }
    bool isReverseFlexDirection() const { return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse; }
    FlexWrap flexWrap() const { return static_cast<FlexWrap>(m_rareNonInheritedData->flexibleBox->flexWrap); }
    const StyleContentAlignmentData& justifyContent() const { return m_rareNonInheritedData->justifyContent; }
    const StyleSelfAlignmentData& justifyItems() const { return m_rareNonInheritedData->justifyItems; }
    const StyleSelfAlignmentData& justifySelf() const { return m_rareNonInheritedData->justifySelf; }

    const Vector<GridTrackSize>& gridColumns() const { return m_rareNonInheritedData->grid->gridColumns(); }
    const Vector<GridTrackSize>& gridRows() const { return m_rareNonInheritedData->grid->gridRows(); }
    const GridTrackList& gridColumnList() const { return m_rareNonInheritedData->grid->columns(); }
    const GridTrackList& gridRowList() const { return m_rareNonInheritedData->grid->rows(); }
    const Vector<GridTrackSize>& gridAutoRepeatColumns() const { return m_rareNonInheritedData->grid->gridAutoRepeatColumns(); }
    const Vector<GridTrackSize>& gridAutoRepeatRows() const { return m_rareNonInheritedData->grid->gridAutoRepeatRows(); }
    unsigned gridAutoRepeatColumnsInsertionPoint() const { return m_rareNonInheritedData->grid->autoRepeatColumnsInsertionPoint(); }
    unsigned gridAutoRepeatRowsInsertionPoint() const { return m_rareNonInheritedData->grid->autoRepeatRowsInsertionPoint(); }
    AutoRepeatType gridAutoRepeatColumnsType() const  { return m_rareNonInheritedData->grid->autoRepeatColumnsType(); }
    AutoRepeatType gridAutoRepeatRowsType() const  { return m_rareNonInheritedData->grid->autoRepeatRowsType(); }
    const NamedGridLinesMap& namedGridColumnLines() const { return m_rareNonInheritedData->grid->namedGridColumnLines(); }
    const NamedGridLinesMap& namedGridRowLines() const { return m_rareNonInheritedData->grid->namedGridRowLines(); }
    const OrderedNamedGridLinesMap& orderedNamedGridColumnLines() const { return m_rareNonInheritedData->grid->orderedNamedGridColumnLines(); }
    const OrderedNamedGridLinesMap& orderedNamedGridRowLines() const { return m_rareNonInheritedData->grid->orderedNamedGridRowLines(); }
    const NamedGridLinesMap& autoRepeatNamedGridColumnLines() const { return m_rareNonInheritedData->grid->autoRepeatNamedGridColumnLines(); }
    const NamedGridLinesMap& autoRepeatNamedGridRowLines() const { return m_rareNonInheritedData->grid->autoRepeatNamedGridRowLines(); }
    const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridColumnLines() const { return m_rareNonInheritedData->grid->autoRepeatOrderedNamedGridColumnLines(); }
    const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridRowLines() const { return m_rareNonInheritedData->grid->autoRepeatOrderedNamedGridRowLines(); }
    const NamedGridLinesMap& implicitNamedGridColumnLines() const { return m_rareNonInheritedData->grid->implicitNamedGridColumnLines; }
    const NamedGridLinesMap& implicitNamedGridRowLines() const { return m_rareNonInheritedData->grid->implicitNamedGridRowLines; }
    const NamedGridAreaMap& namedGridArea() const { return m_rareNonInheritedData->grid->namedGridArea; }
    size_t namedGridAreaRowCount() const { return m_rareNonInheritedData->grid->namedGridAreaRowCount; }
    size_t namedGridAreaColumnCount() const { return m_rareNonInheritedData->grid->namedGridAreaColumnCount; }
    GridAutoFlow gridAutoFlow() const { return static_cast<GridAutoFlow>(m_rareNonInheritedData->grid->gridAutoFlow); }
    bool gridSubgridRows() const { return m_rareNonInheritedData->grid->subgridRows(); }
    bool gridSubgridColumns() const { return m_rareNonInheritedData->grid->subgridColumns(); }
    bool gridMasonryRows() const { return m_rareNonInheritedData->grid->masonryRows(); }
    bool gridMasonryColumns() const { return m_rareNonInheritedData->grid->masonryColumns(); }
    bool isGridAutoFlowDirectionRow() const { return (m_rareNonInheritedData->grid->gridAutoFlow & InternalAutoFlowDirectionRow); }
    bool isGridAutoFlowDirectionColumn() const { return (m_rareNonInheritedData->grid->gridAutoFlow & InternalAutoFlowDirectionColumn); }
    bool isGridAutoFlowAlgorithmSparse() const { return (m_rareNonInheritedData->grid->gridAutoFlow & InternalAutoFlowAlgorithmSparse); }
    bool isGridAutoFlowAlgorithmDense() const { return (m_rareNonInheritedData->grid->gridAutoFlow & InternalAutoFlowAlgorithmDense); }
    const Vector<GridTrackSize>& gridAutoColumns() const { return m_rareNonInheritedData->grid->gridAutoColumns; }
    const Vector<GridTrackSize>& gridAutoRows() const { return m_rareNonInheritedData->grid->gridAutoRows; }

    const GridPosition& gridItemColumnStart() const { return m_rareNonInheritedData->gridItem->gridColumnStart; }
    const GridPosition& gridItemColumnEnd() const { return m_rareNonInheritedData->gridItem->gridColumnEnd; }
    const GridPosition& gridItemRowStart() const { return m_rareNonInheritedData->gridItem->gridRowStart; }
    const GridPosition& gridItemRowEnd() const { return m_rareNonInheritedData->gridItem->gridRowEnd; }

    const ShadowData* boxShadow() const { return m_rareNonInheritedData->boxShadow.get(); }
    LayoutBoxExtent boxShadowExtent() const { return shadowExtent(boxShadow()); }
    LayoutBoxExtent boxShadowInsetExtent() const { return shadowInsetExtent(boxShadow()); }
    void getBoxShadowHorizontalExtent(LayoutUnit& left, LayoutUnit& right) const { getShadowHorizontalExtent(boxShadow(), left, right); }
    void getBoxShadowVerticalExtent(LayoutUnit& top, LayoutUnit& bottom) const { getShadowVerticalExtent(boxShadow(), top, bottom); }
    void getBoxShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const { getShadowInlineDirectionExtent(boxShadow(), logicalLeft, logicalRight); }
    void getBoxShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const { getShadowBlockDirectionExtent(boxShadow(), logicalTop, logicalBottom); }

#if ENABLE(CSS_BOX_DECORATION_BREAK)
    BoxDecorationBreak boxDecorationBreak() const { return m_boxData->boxDecorationBreak(); }
#endif

    StyleReflection* boxReflect() const { return m_rareNonInheritedData->boxReflect.get(); }
    BoxSizing boxSizing() const { return m_boxData->boxSizing(); }
    const Length& marqueeIncrement() const { return m_rareNonInheritedData->marquee->increment; }
    int marqueeSpeed() const { return m_rareNonInheritedData->marquee->speed; }
    int marqueeLoopCount() const { return m_rareNonInheritedData->marquee->loops; }
    MarqueeBehavior marqueeBehavior() const { return static_cast<MarqueeBehavior>(m_rareNonInheritedData->marquee->behavior); }
    MarqueeDirection marqueeDirection() const { return static_cast<MarqueeDirection>(m_rareNonInheritedData->marquee->direction); }
    UserModify effectiveUserModify() const { return effectiveInert() ? UserModify::ReadOnly : userModify(); }
    UserModify userModify() const { return static_cast<UserModify>(m_rareInheritedData->userModify); }
    UserDrag userDrag() const { return static_cast<UserDrag>(m_rareNonInheritedData->userDrag); }
    WEBCORE_EXPORT UserSelect effectiveUserSelect() const;
    UserSelect userSelect() const { return static_cast<UserSelect>(m_rareInheritedData->userSelect); }
    TextOverflow textOverflow() const { return static_cast<TextOverflow>(m_rareNonInheritedData->textOverflow); }
    WordBreak wordBreak() const { return static_cast<WordBreak>(m_rareInheritedData->wordBreak); }
    OverflowWrap overflowWrap() const { return static_cast<OverflowWrap>(m_rareInheritedData->overflowWrap); }
    NBSPMode nbspMode() const { return static_cast<NBSPMode>(m_rareInheritedData->nbspMode); }
    LineBreak lineBreak() const { return static_cast<LineBreak>(m_rareInheritedData->lineBreak); }
    Hyphens hyphens() const { return static_cast<Hyphens>(m_rareInheritedData->hyphens); }
    short hyphenationLimitBefore() const { return m_rareInheritedData->hyphenationLimitBefore; }
    short hyphenationLimitAfter() const { return m_rareInheritedData->hyphenationLimitAfter; }
    short hyphenationLimitLines() const { return m_rareInheritedData->hyphenationLimitLines; }
    const AtomString& hyphenationString() const { return m_rareInheritedData->hyphenationString; }
    const AtomString& computedLocale() const { return fontDescription().computedLocale(); }
    const AtomString& specifiedLocale() const { return fontDescription().specifiedLocale(); }
    Resize resize() const { return static_cast<Resize>(m_rareNonInheritedData->resize); }
    ColumnAxis columnAxis() const { return static_cast<ColumnAxis>(m_rareNonInheritedData->multiCol->axis); }
    bool hasInlineColumnAxis() const;
    ColumnProgression columnProgression() const { return static_cast<ColumnProgression>(m_rareNonInheritedData->multiCol->progression); }
    float columnWidth() const { return m_rareNonInheritedData->multiCol->width; }
    bool hasAutoColumnWidth() const { return m_rareNonInheritedData->multiCol->autoWidth; }
    unsigned short columnCount() const { return m_rareNonInheritedData->multiCol->count; }
    bool hasAutoColumnCount() const { return m_rareNonInheritedData->multiCol->autoCount; }
    bool specifiesColumns() const { return !hasAutoColumnCount() || !hasAutoColumnWidth() || !hasInlineColumnAxis(); }
    ColumnFill columnFill() const { return static_cast<ColumnFill>(m_rareNonInheritedData->multiCol->fill); }
    const GapLength& columnGap() const { return m_rareNonInheritedData->columnGap; }
    const GapLength& rowGap() const { return m_rareNonInheritedData->rowGap; }
    BorderStyle columnRuleStyle() const { return m_rareNonInheritedData->multiCol->rule.style(); }
    unsigned short columnRuleWidth() const { return m_rareNonInheritedData->multiCol->ruleWidth(); }
    bool columnRuleIsTransparent() const { return m_rareNonInheritedData->multiCol->rule.isTransparent(); }
    ColumnSpan columnSpan() const { return static_cast<ColumnSpan>(m_rareNonInheritedData->multiCol->columnSpan); }

    const TransformOperations& transform() const { return m_rareNonInheritedData->transform->operations; }
    bool hasTransform() const { return !m_rareNonInheritedData->transform->operations.operations().isEmpty() || offsetPath(); }
    const Length& transformOriginX() const { return m_rareNonInheritedData->transform->x; }
    const Length& transformOriginY() const { return m_rareNonInheritedData->transform->y; }
    float transformOriginZ() const { return m_rareNonInheritedData->transform->z; }
    LengthPoint transformOriginXY() const { return m_rareNonInheritedData->transform->originXY(); }

    TransformBox transformBox() const { return m_rareNonInheritedData->transform->transformBox; }

    RotateTransformOperation* rotate() const { return m_rareNonInheritedData->rotate.get(); }
    ScaleTransformOperation* scale() const { return m_rareNonInheritedData->scale.get(); }
    TranslateTransformOperation* translate() const { return m_rareNonInheritedData->translate.get(); }

    TextEmphasisFill textEmphasisFill() const { return static_cast<TextEmphasisFill>(m_rareInheritedData->textEmphasisFill); }
    TextEmphasisMark textEmphasisMark() const;
    const AtomString& textEmphasisCustomMark() const { return m_rareInheritedData->textEmphasisCustomMark; }
    OptionSet<TextEmphasisPosition> textEmphasisPosition() const { return OptionSet<TextEmphasisPosition>::fromRaw(m_rareInheritedData->textEmphasisPosition); }
    const AtomString& textEmphasisMarkString() const;

    RubyPosition rubyPosition() const { return static_cast<RubyPosition>(m_rareInheritedData->rubyPosition); }

#if ENABLE(DARK_MODE_CSS)
    StyleColorScheme colorScheme() const { return m_rareInheritedData->colorScheme; }
    void setHasExplicitlySetColorScheme(bool v) { m_nonInheritedFlags.hasExplicitlySetColorScheme = v; }
    bool hasExplicitlySetColorScheme() const { return m_nonInheritedFlags.hasExplicitlySetColorScheme; };
#endif

    TextOrientation textOrientation() const { return static_cast<TextOrientation>(m_rareInheritedData->textOrientation); }

    ObjectFit objectFit() const { return static_cast<ObjectFit>(m_rareNonInheritedData->objectFit); }
    LengthPoint objectPosition() const { return m_rareNonInheritedData->objectPosition; }

    // Return true if any transform related property (currently transform, translate, scale, rotate, transformStyle3D or perspective)
    // indicates that we are transforming. The usedTransformStyle3D is not used here because in many cases (such as for deciding
    // whether or not to establish a containing block), the computed value is what matters.
    bool hasTransformRelatedProperty() const { return hasTransform() || translate() || scale() || rotate() || transformStyle3D() == TransformStyle3D::Preserve3D || hasPerspective(); }

    enum class TransformOperationOption : uint8_t {
        TransformOrigin = 1 << 0,
        Translate       = 1 << 1,
        Rotate          = 1 << 2,
        Scale           = 1 << 3,
        Offset          = 1 << 4
    };

    static constexpr OptionSet<TransformOperationOption> allTransformOperations = { TransformOperationOption::TransformOrigin, TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale , TransformOperationOption::Offset };
    static constexpr OptionSet<TransformOperationOption> individualTransformOperations = { TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset };

    bool affectedByTransformOrigin() const;

    FloatPoint computePerspectiveOrigin(const FloatRect& boundingBox) const;
    void applyPerspective(TransformationMatrix&, const FloatPoint& originTranslate) const;

    FloatPoint3D computeTransformOrigin(const FloatRect& boundingBox) const;
    void applyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;
    void unapplyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;

    // applyTransform calls applyTransformOrigin(), then applyCSSTransform(), followed by unapplyTransformOrigin().
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, OptionSet<TransformOperationOption> = allTransformOperations) const;
    void applyCSSTransform(TransformationMatrix&, const FloatRect& boundingBox, OptionSet<TransformOperationOption> = allTransformOperations) const;
    void applyMotionPathTransform(TransformationMatrix&, const FloatRect& boundingBox) const;
    void setPageScaleTransform(float);

    bool hasPositionedMask() const { return m_rareNonInheritedData->mask->hasImage(); }
    bool hasMask() const { return m_rareNonInheritedData->mask->hasImage() || m_rareNonInheritedData->maskBoxImage.hasImage(); }

    TextCombine textCombine() const { return static_cast<TextCombine>(m_rareInheritedData->textCombine); }
    bool hasTextCombine() const { return textCombine() != TextCombine::None; }

    const TabSize& tabSize() const { return m_rareInheritedData->tabSize; }

    // End CSS3 Getters

    const AtomString& lineGrid() const { return m_rareInheritedData->lineGrid; }
    LineSnap lineSnap() const { return static_cast<LineSnap>(m_rareInheritedData->lineSnap); }
    LineAlign lineAlign() const { return static_cast<LineAlign>(m_rareInheritedData->lineAlign); }

    PointerEvents pointerEvents() const { return static_cast<PointerEvents>(m_inheritedFlags.pointerEvents); }
    PointerEvents effectivePointerEvents() const { return effectiveInert() ? PointerEvents::None : pointerEvents(); }
    const AnimationList* animations() const { return m_rareNonInheritedData->animations.get(); }
    const AnimationList* transitions() const { return m_rareNonInheritedData->transitions.get(); }

    AnimationList* animations() { return m_rareNonInheritedData->animations.get(); }
    AnimationList* transitions() { return m_rareNonInheritedData->transitions.get(); }
    
    bool hasAnimationsOrTransitions() const { return hasAnimations() || hasTransitions(); }

    AnimationList& ensureAnimations();
    AnimationList& ensureTransitions();

    bool hasAnimations() const { return m_rareNonInheritedData->animations && m_rareNonInheritedData->animations->size() > 0; }
    bool hasTransitions() const { return m_rareNonInheritedData->transitions && m_rareNonInheritedData->transitions->size() > 0; }

    TransformStyle3D transformStyle3D() const { return static_cast<TransformStyle3D>(m_rareNonInheritedData->transformStyle3D); }
    TransformStyle3D usedTransformStyle3D() const { return static_cast<bool>(m_rareNonInheritedData->transformStyleForcedToFlat) ? TransformStyle3D::Flat : transformStyle3D(); }
    bool preserves3D() const { return usedTransformStyle3D() == TransformStyle3D::Preserve3D; }

    BackfaceVisibility backfaceVisibility() const { return static_cast<BackfaceVisibility>(m_rareNonInheritedData->backfaceVisibility); }
    float perspective() const { return m_rareNonInheritedData->perspective; }
    float usedPerspective() const { return std::max(1.0f, perspective()); }
    bool hasPerspective() const { return m_rareNonInheritedData->perspective != initialPerspective(); }
    const Length& perspectiveOriginX() const { return m_rareNonInheritedData->perspectiveOriginX; }
    const Length& perspectiveOriginY() const { return m_rareNonInheritedData->perspectiveOriginY; }
    LengthPoint perspectiveOrigin() const { return m_rareNonInheritedData->perspectiveOrigin(); }

    const LengthSize& pageSize() const { return m_rareNonInheritedData->pageSize; }
    PageSizeType pageSizeType() const { return static_cast<PageSizeType>(m_rareNonInheritedData->pageSizeType); }

    OptionSet<LineBoxContain> lineBoxContain() const { return OptionSet<LineBoxContain>::fromRaw(m_rareInheritedData->lineBoxContain); }
    const LineClampValue& lineClamp() const { return m_rareNonInheritedData->lineClamp; }
    const IntSize& initialLetter() const { return m_rareNonInheritedData->initialLetter; }
    int initialLetterDrop() const { return initialLetter().width(); }
    int initialLetterHeight() const { return initialLetter().height(); }

    OptionSet<TouchAction> touchActions() const { return m_rareNonInheritedData->touchActions; }
    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    OptionSet<TouchAction> effectiveTouchActions() const { return m_rareInheritedData->effectiveTouchActions; }
    OptionSet<EventListenerRegionType> eventListenerRegionTypes() const { return m_rareInheritedData->eventListenerRegionTypes; }

    bool effectiveInert() const { return m_rareInheritedData->effectiveInert; }

    const LengthBox& scrollMargin() const;
    const Length& scrollMarginTop() const;
    const Length& scrollMarginBottom() const;
    const Length& scrollMarginLeft() const;
    const Length& scrollMarginRight() const;

    const LengthBox& scrollPadding() const;
    const Length& scrollPaddingTop() const;
    const Length& scrollPaddingBottom() const;
    const Length& scrollPaddingLeft() const;
    const Length& scrollPaddingRight() const;

    bool hasSnapPosition() const;
    const ScrollSnapType scrollSnapType() const;
    const ScrollSnapAlign& scrollSnapAlign() const;
    ScrollSnapStop scrollSnapStop() const;

#if ENABLE(TOUCH_EVENTS)
    StyleColor tapHighlightColor() const { return m_rareInheritedData->tapHighlightColor; }
#endif

#if PLATFORM(IOS_FAMILY)
    bool touchCalloutEnabled() const { return m_rareInheritedData->touchCalloutEnabled; }
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    bool useTouchOverflowScrolling() const { return m_rareInheritedData->useTouchOverflowScrolling; }
#endif

    bool useSmoothScrolling() const { return m_rareNonInheritedData->useSmoothScrolling; }

#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjustment textSizeAdjust() const { return m_rareInheritedData->textSizeAdjust; }
    AutosizeStatus autosizeStatus() const;
    bool isIdempotentTextAutosizingCandidate(std::optional<AutosizeStatus> overrideStatus = std::nullopt) const;
#endif

    TextSecurity textSecurity() const { return static_cast<TextSecurity>(m_rareInheritedData->textSecurity); }
    InputSecurity inputSecurity() const { return static_cast<InputSecurity>(m_rareNonInheritedData->inputSecurity); }

    WritingMode writingMode() const { return static_cast<WritingMode>(m_inheritedFlags.writingMode); }
    bool isHorizontalWritingMode() const { return WebCore::isHorizontalWritingMode(writingMode()); }
    bool isVerticalWritingMode() const { return WebCore::isVerticalWritingMode(writingMode()); }
    bool isFlippedLinesWritingMode() const { return WebCore::isFlippedLinesWritingMode(writingMode()); }
    bool isFlippedBlocksWritingMode() const { return WebCore::isFlippedWritingMode(writingMode()); }

    ImageOrientation imageOrientation() const;

    ImageRendering imageRendering() const { return static_cast<ImageRendering>(m_rareInheritedData->imageRendering); }

#if ENABLE(CSS_IMAGE_RESOLUTION)
    ImageResolutionSource imageResolutionSource() const { return static_cast<ImageResolutionSource>(m_rareInheritedData->imageResolutionSource); }
    ImageResolutionSnap imageResolutionSnap() const { return static_cast<ImageResolutionSnap>(m_rareInheritedData->imageResolutionSnap); }
    float imageResolution() const { return m_rareInheritedData->imageResolution; }
#endif
    
    OptionSet<SpeakAs> speakAs() const { return OptionSet<SpeakAs>::fromRaw(m_rareInheritedData->speakAs); }

    FilterOperations& mutableFilter() { return m_rareNonInheritedData.access().filter.access().operations; }
    const FilterOperations& filter() const { return m_rareNonInheritedData->filter->operations; }
    bool hasFilter() const { return !m_rareNonInheritedData->filter->operations.operations().isEmpty(); }
    bool hasReferenceFilterOnly() const;

    FilterOperations& mutableAppleColorFilter() { return m_rareInheritedData.access().appleColorFilter.access().operations; }
    const FilterOperations& appleColorFilter() const { return m_rareInheritedData->appleColorFilter->operations; }
    bool hasAppleColorFilter() const { return !m_rareInheritedData->appleColorFilter->operations.operations().isEmpty(); }

#if ENABLE(FILTERS_LEVEL_2)
    FilterOperations& mutableBackdropFilter() { return m_rareNonInheritedData.access().backdropFilter.access().operations; }
    const FilterOperations& backdropFilter() const { return m_rareNonInheritedData->backdropFilter->operations; }
    bool hasBackdropFilter() const { return !m_rareNonInheritedData->backdropFilter->operations.operations().isEmpty(); }
#else
    bool hasBackdropFilter() const { return false; }
#endif

#if ENABLE(CSS_COMPOSITING)
    BlendMode blendMode() const { return static_cast<BlendMode>(m_rareNonInheritedData->effectiveBlendMode); }
    void setBlendMode(BlendMode);
    bool hasBlendMode() const { return static_cast<BlendMode>(m_rareNonInheritedData->effectiveBlendMode) != BlendMode::Normal; }
    bool isInSubtreeWithBlendMode() const { return m_rareInheritedData->isInSubtreeWithBlendMode; }

    Isolation isolation() const { return static_cast<Isolation>(m_rareNonInheritedData->isolation); }
    void setIsolation(Isolation isolation) { SET_VAR(m_rareNonInheritedData, isolation, static_cast<unsigned>(isolation)); }
    bool hasIsolation() const { return isolation() != Isolation::Auto; }
#else
    BlendMode blendMode() const { return BlendMode::Normal; }
    bool hasBlendMode() const { return false; }

    Isolation isolation() const { return Isolation::Auto; }
    bool hasIsolation() const { return false; }
#endif

    bool shouldPlaceVerticalScrollbarOnLeft() const;

#if ENABLE(CSS_TRAILING_WORD)
    TrailingWord trailingWord() const { return TrailingWord::Auto; }
#endif

#if ENABLE(APPLE_PAY)
    ApplePayButtonStyle applePayButtonStyle() const { return static_cast<ApplePayButtonStyle>(m_rareNonInheritedData->applePayButtonStyle); }
    ApplePayButtonType applePayButtonType() const { return static_cast<ApplePayButtonType>(m_rareNonInheritedData->applePayButtonType); }
#endif

    MathStyle mathStyle() const { return static_cast<MathStyle>(m_rareInheritedData->mathStyle); }

// attribute setter methods

    void setDisplay(DisplayType value)
    {
        m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
        m_nonInheritedFlags.effectiveDisplay = m_nonInheritedFlags.originalDisplay;
    }
    void setEffectiveDisplay(DisplayType v) { m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(v); }
    void setPosition(PositionType v) { m_nonInheritedFlags.position = static_cast<unsigned>(v); }
    void setFloating(Float v) { m_nonInheritedFlags.floating = static_cast<unsigned>(v); }

    void setLeft(Length&& length) { SET_VAR(m_surroundData, offset.left(), WTFMove(length)); }
    void setRight(Length&& length) { SET_VAR(m_surroundData, offset.right(), WTFMove(length)); }
    void setTop(Length&& length) { SET_VAR(m_surroundData, offset.top(), WTFMove(length)); }
    void setBottom(Length&& length) { SET_VAR(m_surroundData, offset.bottom(), WTFMove(length)); }

    void setWidth(Length&& length) { SET_VAR(m_boxData, m_width, WTFMove(length)); }
    void setHeight(Length&& length) { SET_VAR(m_boxData, m_height, WTFMove(length)); }

    void setLogicalWidth(Length&&);
    void setLogicalHeight(Length&&);

    void setMinWidth(Length&& length) { SET_VAR(m_boxData, m_minWidth, WTFMove(length)); }
    void setMaxWidth(Length&& length) { SET_VAR(m_boxData, m_maxWidth, WTFMove(length)); }
    void setMinHeight(Length&& length) { SET_VAR(m_boxData, m_minHeight, WTFMove(length)); }
    void setMaxHeight(Length&& length) { SET_VAR(m_boxData, m_maxHeight, WTFMove(length)); }

    void resetBorder() { resetBorderExceptRadius(); resetBorderRadius(); }
    void resetBorderExceptRadius() { resetBorderImage(); resetBorderTop(); resetBorderRight(); resetBorderBottom(); resetBorderLeft(); }
    void resetBorderTop() { SET_VAR(m_surroundData, border.m_top, BorderValue()); }
    void resetBorderRight() { SET_VAR(m_surroundData, border.m_right, BorderValue()); }
    void resetBorderBottom() { SET_VAR(m_surroundData, border.m_bottom, BorderValue()); }
    void resetBorderLeft() { SET_VAR(m_surroundData, border.m_left, BorderValue()); }
    void resetBorderImage() { SET_VAR(m_surroundData, border.m_image, NinePieceImage()); }
    void resetBorderRadius() { resetBorderTopLeftRadius(); resetBorderTopRightRadius(); resetBorderBottomLeftRadius(); resetBorderBottomRightRadius(); }
    void resetBorderTopLeftRadius() { SET_VAR(m_surroundData, border.m_radii.topLeft, initialBorderRadius()); }
    void resetBorderTopRightRadius() { SET_VAR(m_surroundData, border.m_radii.topRight, initialBorderRadius()); }
    void resetBorderBottomLeftRadius() { SET_VAR(m_surroundData, border.m_radii.bottomLeft, initialBorderRadius()); }
    void resetBorderBottomRightRadius() { SET_VAR(m_surroundData, border.m_radii.bottomRight, initialBorderRadius()); }

    void setBackgroundColor(const StyleColor& v) { SET_VAR(m_backgroundData, color, v); }

    void setBackgroundXPosition(Length&& length) { SET_NESTED_VAR(m_backgroundData, background, m_xPosition, WTFMove(length)); }
    void setBackgroundYPosition(Length&& length) { SET_NESTED_VAR(m_backgroundData, background, m_yPosition, WTFMove(length)); }
    void setBackgroundSize(FillSizeType b) { SET_NESTED_VAR(m_backgroundData, background, m_sizeType, static_cast<unsigned>(b)); }
    void setBackgroundSizeLength(LengthSize&& size) { SET_NESTED_VAR(m_backgroundData, background, m_sizeLength, WTFMove(size)); }
    void setBackgroundAttachment(FillAttachment attachment) { SET_NESTED_VAR(m_backgroundData, background, m_attachment, static_cast<unsigned>(attachment)); SET_NESTED_VAR(m_backgroundData, background, m_attachmentSet, true); }
    void setBackgroundClip(FillBox fillBox) { SET_NESTED_VAR(m_backgroundData, background, m_clip, static_cast<unsigned>(fillBox)); SET_NESTED_VAR(m_backgroundData, background, m_clipSet, true); }
    void setBackgroundOrigin(FillBox fillBox) { SET_NESTED_VAR(m_backgroundData, background, m_origin, static_cast<unsigned>(fillBox)); SET_NESTED_VAR(m_backgroundData, background, m_originSet, true); }
    void setBackgroundRepeat(FillRepeatXY fillRepeat) { SET_NESTED_VAR(m_backgroundData, background, m_repeat, fillRepeat); SET_NESTED_VAR(m_backgroundData, background, m_repeatSet, true); }
    void setBackgroundBlendMode(BlendMode blendMode) { SET_NESTED_VAR(m_backgroundData, background, m_blendMode, static_cast<unsigned>(blendMode)); SET_NESTED_VAR(m_backgroundData, background, m_blendModeSet, true); }

    void setBorderImage(const NinePieceImage& b) { SET_VAR(m_surroundData, border.m_image, b); }
    void setBorderImageSource(RefPtr<StyleImage>&&);
    void setBorderImageSliceFill(bool);
    void setBorderImageSlices(LengthBox&&);
    void setBorderImageWidth(LengthBox&&);
    void setBorderImageWidthOverridesBorderWidths(bool);
    void setBorderImageOutset(LengthBox&&);
    void setBorderImageHorizontalRule(NinePieceImageRule);
    void setBorderImageVerticalRule(NinePieceImageRule);

    void setBorderTopLeftRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_radii.topLeft, WTFMove(size)); }
    void setBorderTopRightRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_radii.topRight, WTFMove(size)); }
    void setBorderBottomLeftRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_radii.bottomLeft, WTFMove(size)); }
    void setBorderBottomRightRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_radii.bottomRight, WTFMove(size)); }

    void setBorderRadius(LengthSize&&);
    void setBorderRadius(const IntSize&);
    void setHasExplicitlySetBorderBottomLeftRadius(bool v) { m_nonInheritedFlags.hasExplicitlySetBorderBottomLeftRadius = v; }
    void setHasExplicitlySetBorderBottomRightRadius(bool v) { m_nonInheritedFlags.hasExplicitlySetBorderBottomRightRadius = v; }
    void setHasExplicitlySetBorderTopLeftRadius(bool v) { m_nonInheritedFlags.hasExplicitlySetBorderTopLeftRadius = v; }
    void setHasExplicitlySetBorderTopRightRadius(bool v) { m_nonInheritedFlags.hasExplicitlySetBorderTopRightRadius = v; }

    RoundedRect getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth,
        LayoutUnit leftWidth, LayoutUnit rightWidth, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    static RoundedRect getRoundedInnerBorderFor(const LayoutRect&, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth, std::optional<BorderData::Radii>, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);

    void setBorderLeftWidth(float v) { SET_VAR(m_surroundData, border.m_left.m_width, v); }
    void setBorderLeftStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_left.m_style, static_cast<unsigned>(v)); }
    void setBorderLeftColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_left, v); }
    void setBorderRightWidth(float v) { SET_VAR(m_surroundData, border.m_right.m_width, v); }
    void setBorderRightStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_right.m_style, static_cast<unsigned>(v)); }
    void setBorderRightColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_right, v); }
    void setBorderTopWidth(float v) { SET_VAR(m_surroundData, border.m_top.m_width, v); }
    void setBorderTopStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_top.m_style, static_cast<unsigned>(v)); }
    void setBorderTopColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_top, v); }
    void setBorderBottomWidth(float v) { SET_VAR(m_surroundData, border.m_bottom.m_width, v); }
    void setBorderBottomStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_bottom.m_style, static_cast<unsigned>(v)); }
    void setBorderBottomColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_bottom, v); }

    void setOutlineWidth(float v) { SET_VAR(m_backgroundData, outline.m_width, v); }
    void setOutlineStyleIsAuto(OutlineIsAuto isAuto) { SET_VAR(m_backgroundData, outline.m_isAuto, static_cast<unsigned>(isAuto)); }
    void setOutlineStyle(BorderStyle v) { SET_VAR(m_backgroundData, outline.m_style, static_cast<unsigned>(v)); }
    void setOutlineColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_backgroundData, outline, v); }

    void setOverflowX(Overflow v) { m_nonInheritedFlags.overflowX =  static_cast<unsigned>(v); }
    void setOverflowY(Overflow v) { m_nonInheritedFlags.overflowY = static_cast<unsigned>(v); }
    void setOverscrollBehaviorX(OverscrollBehavior v) { SET_VAR(m_rareNonInheritedData, overscrollBehaviorX, static_cast<unsigned>(v)); }
    void setOverscrollBehaviorY(OverscrollBehavior v) { SET_VAR(m_rareNonInheritedData, overscrollBehaviorY, static_cast<unsigned>(v)); }
    void setVisibility(Visibility v) { m_inheritedFlags.visibility = static_cast<unsigned>(v); }
    void setVerticalAlign(VerticalAlign v) { m_nonInheritedFlags.verticalAlign = static_cast<unsigned>(v); }
    void setVerticalAlignLength(Length&& length) { setVerticalAlign(VerticalAlign::Length); SET_VAR(m_boxData, m_verticalAlign, WTFMove(length)); }

    void setHasClip(bool b = true) { SET_VAR(m_visualData, hasClip, b); }
    void setClipLeft(Length&& length) { SET_VAR(m_visualData, clip.left(), WTFMove(length)); }
    void setClipRight(Length&& length) { SET_VAR(m_visualData, clip.right(), WTFMove(length)); }
    void setClipTop(Length&& length) { SET_VAR(m_visualData, clip.top(), WTFMove(length)); }
    void setClipBottom(Length&& length) { SET_VAR(m_visualData, clip.bottom(), WTFMove(length)); }
    void setClip(Length&& top, Length&& right, Length&& bottom, Length&& left);
    void setClip(LengthBox&& box) { SET_VAR(m_visualData, clip, WTFMove(box)); }

    void setUnicodeBidi(UnicodeBidi v) { m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(v); }

    void setClear(Clear v) { m_nonInheritedFlags.clear = static_cast<unsigned>(v); }
    void setTableLayout(TableLayoutType v) { m_nonInheritedFlags.tableLayout = static_cast<unsigned>(v); }

    bool setFontDescription(FontCascadeDescription&&);

    // Only used for blending font sizes when animating, for MathML anonymous blocks, and for text autosizing.
    void setFontSize(float);
    void setFontSizeAdjust(std::optional<float>);

    void setFontVariationSettings(FontVariationSettings);
    void setFontWeight(FontSelectionValue);
    void setFontStretch(FontSelectionValue);
    void setFontItalic(std::optional<FontSelectionValue>);
    void setFontPalette(FontPalette);

    void setColor(const Color&);
    void setTextIndent(Length&& length) { SET_VAR(m_rareInheritedData, indent, WTFMove(length)); }
    void setTextAlign(TextAlignMode v) { m_inheritedFlags.textAlign = static_cast<unsigned>(v); }
    void setTextAlignLast(TextAlignLast v) { SET_VAR(m_rareInheritedData, textAlignLast, static_cast<unsigned>(v)); }
    void setTextTransform(TextTransform v) { m_inheritedFlags.textTransform = static_cast<unsigned>(v); }
    void addToTextDecorationsInEffect(OptionSet<TextDecorationLine> v) { m_inheritedFlags.textDecorationLines |= static_cast<unsigned>(v.toRaw()); }
    void setTextDecorationsInEffect(OptionSet<TextDecorationLine> v) { m_inheritedFlags.textDecorationLines = v.toRaw(); }
    void setTextDecorationLine(OptionSet<TextDecorationLine> v) { SET_VAR(m_visualData, textDecorationLine, v.toRaw()); }
    void setTextDecorationStyle(TextDecorationStyle v) { SET_VAR(m_rareNonInheritedData, textDecorationStyle, static_cast<unsigned>(v)); }
    void setTextDecorationSkipInk(TextDecorationSkipInk skipInk) { SET_VAR(m_rareInheritedData, textDecorationSkipInk, static_cast<unsigned>(skipInk)); }
    void setTextUnderlinePosition(TextUnderlinePosition position) { SET_VAR(m_rareInheritedData, textUnderlinePosition, static_cast<unsigned>(position)); }
    void setTextUnderlineOffset(TextUnderlineOffset textUnderlineOffset) { SET_VAR(m_rareInheritedData, textUnderlineOffset, textUnderlineOffset); }
    void setTextDecorationThickness(TextDecorationThickness textDecorationThickness) { SET_VAR(m_rareNonInheritedData, textDecorationThickness, textDecorationThickness); }
    void setDirection(TextDirection v) { m_inheritedFlags.direction = static_cast<unsigned>(v); }
    void setHasExplicitlySetDirection(bool v) { m_nonInheritedFlags.hasExplicitlySetDirection = v; }
    void setLineHeight(Length&&);
    bool setZoom(float);
    void setZoomWithoutReturnValue(float f) { setZoom(f); }
    bool setEffectiveZoom(float);
    void setTextZoom(TextZoom v) { SET_VAR(m_rareInheritedData, textZoom, static_cast<unsigned>(v)); }

    void setTextIndentLine(TextIndentLine v) { SET_VAR(m_rareInheritedData, textIndentLine, static_cast<unsigned>(v)); }
    void setTextIndentType(TextIndentType v) { SET_VAR(m_rareInheritedData, textIndentType, static_cast<unsigned>(v)); }
    void setTextJustify(TextJustify v) { SET_VAR(m_rareInheritedData, textJustify, static_cast<unsigned>(v)); }

#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(Length&&);
#endif

    void setImageOrientation(ImageOrientation v) { SET_VAR(m_rareInheritedData, imageOrientation, static_cast<int>(v)); }
    void setImageRendering(ImageRendering v) { SET_VAR(m_rareInheritedData, imageRendering, static_cast<unsigned>(v)); }

#if ENABLE(CSS_IMAGE_RESOLUTION)
    void setImageResolutionSource(ImageResolutionSource v) { SET_VAR(m_rareInheritedData, imageResolutionSource, v); }
    void setImageResolutionSnap(ImageResolutionSnap v) { SET_VAR(m_rareInheritedData, imageResolutionSnap, v); }
    void setImageResolution(float f) { SET_VAR(m_rareInheritedData, imageResolution, f); }
#endif

    void setWhiteSpace(WhiteSpace v) { m_inheritedFlags.whiteSpace = static_cast<unsigned>(v); }

    void setWordSpacing(Length&&);

    // If letter-spacing is nonzero, we disable ligatures, which means this property affects font preparation.
    void setLetterSpacing(float);
    void setLetterSpacingWithoutUpdatingFontDescription(float);

    void clearBackgroundLayers() { m_backgroundData.access().background = FillLayer::create(FillLayerType::Background); }
    void inheritBackgroundLayers(const FillLayer& parent) { m_backgroundData.access().background = FillLayer::create(parent); }

    void adjustBackgroundLayers();

    void clearMaskLayers() { m_rareNonInheritedData.access().mask = FillLayer::create(FillLayerType::Mask); }
    void inheritMaskLayers(const FillLayer& parent) { m_rareNonInheritedData.access().mask = FillLayer::create(parent); }

    void adjustMaskLayers();

    void setMaskImage(RefPtr<StyleImage>&& v) { m_rareNonInheritedData.access().mask.access().setImage(WTFMove(v)); }

    void setMaskBoxImage(const NinePieceImage& b) { SET_VAR(m_rareNonInheritedData, maskBoxImage, b); }
    void setMaskBoxImageSource(RefPtr<StyleImage>&& v) { m_rareNonInheritedData.access().maskBoxImage.setImage(WTFMove(v)); }
    void setMaskXPosition(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, mask, m_xPosition, WTFMove(length)); }
    void setMaskYPosition(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, mask, m_yPosition, WTFMove(length)); }
    void setMaskRepeat(FillRepeatXY fillRepeat) { SET_NESTED_VAR(m_rareNonInheritedData, mask, m_repeat, fillRepeat); SET_NESTED_VAR(m_rareNonInheritedData, mask, m_repeatSet, true); }

    void setMaskSize(LengthSize size) { SET_NESTED_VAR(m_rareNonInheritedData, mask, m_sizeLength, WTFMove(size)); }

    void setBorderCollapse(BorderCollapse collapse) { m_inheritedFlags.borderCollapse = static_cast<unsigned>(collapse); }
    void setHorizontalBorderSpacing(float);
    void setVerticalBorderSpacing(float);
    void setEmptyCells(EmptyCell v) { m_inheritedFlags.emptyCells = static_cast<unsigned>(v); }
    void setCaptionSide(CaptionSide v) { m_inheritedFlags.captionSide = static_cast<unsigned>(v); }

    void setAspectRatioType(AspectRatioType aspectRatioType) { SET_VAR(m_rareNonInheritedData, aspectRatioType, static_cast<unsigned>(aspectRatioType)); }
    void setAspectRatio(double width, double height) { SET_VAR(m_rareNonInheritedData, aspectRatioWidth, width); SET_VAR(m_rareNonInheritedData, aspectRatioHeight, height); }

    void setContain(OptionSet<Containment> containment) { SET_VAR(m_rareNonInheritedData, contain, containment); }
    void setContainerType(ContainerType containerType) { SET_VAR(m_rareNonInheritedData, containerType, static_cast<unsigned>(containerType)); }
    void setContainerNames(const Vector<AtomString>& names) { SET_VAR(m_rareNonInheritedData, containerNames, names); }

    void setContainIntrinsicWidthType(ContainIntrinsicSizeType containIntrinsicWidthType) { SET_VAR(m_rareNonInheritedData, containIntrinsicWidthType, static_cast<unsigned>(containIntrinsicWidthType)); }
    void setContainIntrinsicHeightType(ContainIntrinsicSizeType containIntrinsicHeightType) { SET_VAR(m_rareNonInheritedData, containIntrinsicHeightType, static_cast<unsigned>(containIntrinsicHeightType)); }
    void setContainIntrinsicWidth(std::optional<Length> width) { SET_VAR(m_rareNonInheritedData, containIntrinsicWidth, width); }
    void setContainIntrinsicHeight(std::optional<Length> height) { SET_VAR(m_rareNonInheritedData, containIntrinsicHeight, height); }

    void setContentVisibility(ContentVisibility value) { SET_VAR(m_rareNonInheritedData, contentVisibility, static_cast<unsigned>(value)); }

    void setListStyleStringValue(const AtomString& value) { SET_VAR(m_rareInheritedData, listStyleStringValue, value); }
    void setListStyleType(ListStyleType v) { m_inheritedFlags.listStyleType = static_cast<unsigned>(v); }
    void setListStyleImage(RefPtr<StyleImage>&&);
    void setListStylePosition(ListStylePosition v) { m_inheritedFlags.listStylePosition = static_cast<unsigned>(v); }

    void resetMargin() { SET_VAR(m_surroundData, margin, LengthBox(LengthType::Fixed)); }
    void setMarginTop(Length&& length) { SET_VAR(m_surroundData, margin.top(), WTFMove(length)); }
    void setMarginBottom(Length&& length) { SET_VAR(m_surroundData, margin.bottom(), WTFMove(length)); }
    void setMarginLeft(Length&& length) { SET_VAR(m_surroundData, margin.left(), WTFMove(length)); }
    void setMarginRight(Length&& length) { SET_VAR(m_surroundData, margin.right(), WTFMove(length)); }
    void setMarginStart(Length&&);
    void setMarginEnd(Length&&);

    void resetPadding() { SET_VAR(m_surroundData, padding, LengthBox(initialPadding().intValue())); }
    void setPaddingBox(LengthBox&& box) { SET_VAR(m_surroundData, padding, WTFMove(box)); }
    void setPaddingTop(Length&& length) { SET_VAR(m_surroundData, padding.top(), WTFMove(length)); }
    void setPaddingBottom(Length&& length) { SET_VAR(m_surroundData, padding.bottom(), WTFMove(length)); }
    void setPaddingLeft(Length&& length) { SET_VAR(m_surroundData, padding.left(), WTFMove(length)); }
    void setPaddingRight(Length&& length) { SET_VAR(m_surroundData, padding.right(), WTFMove(length)); }

    void setCursor(CursorType c) { m_inheritedFlags.cursor = static_cast<unsigned>(c); }
    void addCursor(RefPtr<StyleImage>&&, const std::optional<IntPoint>& hotSpot);
    void setCursorList(RefPtr<CursorList>&&);
    void clearCursorList();

#if ENABLE(CURSOR_VISIBILITY)
    void setCursorVisibility(CursorVisibility c) { m_inheritedFlags.cursorVisibility = static_cast<unsigned>(c); }
#endif

    void setInsideLink(InsideLink insideLink) { m_inheritedFlags.insideLink = static_cast<unsigned>(insideLink); }
    void setIsLink(bool v) { m_nonInheritedFlags.isLink = v; }

    void setInsideDefaultButton(bool insideDefaultButton) { m_inheritedFlags.insideDefaultButton = insideDefaultButton; }

    PrintColorAdjust printColorAdjust() const { return static_cast<PrintColorAdjust>(m_inheritedFlags.printColorAdjust); }
    void setPrintColorAdjust(PrintColorAdjust value) { m_inheritedFlags.printColorAdjust = static_cast<unsigned>(value); }

    int specifiedZIndex() const { return m_boxData->specifiedZIndex(); }
    bool hasAutoSpecifiedZIndex() const { return m_boxData->hasAutoSpecifiedZIndex(); }
    void setSpecifiedZIndex(int v)
    {
        SET_VAR(m_boxData, m_hasAutoSpecifiedZIndex, false);
        SET_VAR(m_boxData, m_specifiedZIndex, v);
    }
    void setHasAutoSpecifiedZIndex()
    {
        SET_VAR(m_boxData, m_hasAutoSpecifiedZIndex, true);
        SET_VAR(m_boxData, m_specifiedZIndex, 0);
    }

    int usedZIndex() const { return m_boxData->usedZIndex(); }
    bool hasAutoUsedZIndex() const { return m_boxData->hasAutoUsedZIndex(); }
    void setUsedZIndex(int v)
    {
        SET_VAR(m_boxData, m_hasAutoUsedZIndex, false);
        SET_VAR(m_boxData, m_usedZIndex, v);
    }
    void setHasAutoUsedZIndex()
    {
        SET_VAR(m_boxData, m_hasAutoUsedZIndex, true);
        SET_VAR(m_boxData, m_usedZIndex, 0);
    }

    void setHasAutoWidows() { SET_VAR(m_rareInheritedData, hasAutoWidows, true); SET_VAR(m_rareInheritedData, widows, initialWidows()); }
    void setWidows(unsigned short w) { SET_VAR(m_rareInheritedData, hasAutoWidows, false); SET_VAR(m_rareInheritedData, widows, w); }

    void setHasAutoOrphans() { SET_VAR(m_rareInheritedData, hasAutoOrphans, true); SET_VAR(m_rareInheritedData, orphans, initialOrphans()); }
    void setOrphans(unsigned short o) { SET_VAR(m_rareInheritedData, hasAutoOrphans, false); SET_VAR(m_rareInheritedData, orphans, o); }

    // CSS3 Setters
    void setOutlineOffset(float v) { SET_VAR(m_backgroundData, outline.m_offset, v); }
    void setTextShadow(std::unique_ptr<ShadowData>, bool add = false);
    void setTextStrokeColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, textStrokeColor, c); }
    void setTextStrokeWidth(float w) { SET_VAR(m_rareInheritedData, textStrokeWidth, w); }
    void setTextFillColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, textFillColor, c); }
    void setCaretColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, caretColor, c); SET_VAR(m_rareInheritedData, hasAutoCaretColor, false);  }
    void setHasAutoCaretColor() { SET_VAR(m_rareInheritedData, hasAutoCaretColor, true); SET_VAR(m_rareInheritedData, caretColor, currentColor()); }
    void setAccentColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, accentColor, c); SET_VAR(m_rareInheritedData, hasAutoAccentColor, false);  }
    void setHasAutoAccentColor() { SET_VAR(m_rareInheritedData, hasAutoAccentColor, true); SET_VAR(m_rareInheritedData, accentColor, currentColor()); }
    void setOpacity(float f) { float v = clampTo<float>(f, 0.f, 1.f); SET_VAR(m_rareNonInheritedData, opacity, v); }
    static_assert(largestControlPart < 1 << appearanceBitWidth, "Control part must fit in storage bits");
    void setAppearance(ControlPart a) { SET_VAR(m_rareNonInheritedData, appearance, a); SET_VAR(m_rareNonInheritedData, effectiveAppearance, a); }
    void setEffectiveAppearance(ControlPart a) { SET_VAR(m_rareNonInheritedData, effectiveAppearance, a); }
    // For valid values of box-align see http://www.w3.org/TR/2009/WD-css3-flexbox-20090723/#alignment
    void setBoxAlign(BoxAlignment a) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, align, static_cast<unsigned>(a)); }
    void setBoxDirection(BoxDirection d) { m_inheritedFlags.boxDirection = static_cast<unsigned>(d); }
    void setBoxFlex(float f) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, flex, f); }
    void setBoxFlexGroup(unsigned group) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, flexGroup, group); }
    void setBoxLines(BoxLines lines) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, lines, static_cast<unsigned>(lines)); }
    void setBoxOrdinalGroup(unsigned group) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, ordinalGroup, group); }
    void setBoxOrient(BoxOrient o) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, orient, static_cast<unsigned>(o)); }
    void setBoxPack(BoxPack p) { SET_NESTED_VAR(m_rareNonInheritedData, deprecatedFlexibleBox, pack, static_cast<unsigned>(p)); }
    void setBoxShadow(std::unique_ptr<ShadowData>, bool add = false);
    void setBoxReflect(RefPtr<StyleReflection>&&);
    void setBoxSizing(BoxSizing s) { SET_VAR(m_boxData, m_boxSizing, static_cast<unsigned>(s)); }
    void setFlexGrow(float f) { float clampedGrow = std::max<float>(f, 0.f); SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexGrow, clampedGrow); }
    void setFlexShrink(float f) { float clampledShrink = std::max<float>(f, 0.f); SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexShrink, clampledShrink); }
    void setFlexBasis(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexBasis, WTFMove(length)); }
    void setOrder(int o) { SET_VAR(m_rareNonInheritedData, order, o); }
    void setAlignContent(const StyleContentAlignmentData& data) { SET_VAR(m_rareNonInheritedData, alignContent, data); }
    void setAlignItems(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, alignItems, data); }
    void setAlignItemsPosition(ItemPosition position) { m_rareNonInheritedData.access().alignItems.setPosition(position); }
    void setAlignSelf(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, alignSelf, data); }
    void setAlignSelfPosition(ItemPosition position) { m_rareNonInheritedData.access().alignSelf.setPosition(position); }
    void setFlexDirection(FlexDirection direction) { SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexDirection, static_cast<unsigned>(direction)); }
    void setFlexWrap(FlexWrap w) { SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexWrap, static_cast<unsigned>(w)); }
    void setJustifyContent(const StyleContentAlignmentData& data) { SET_VAR(m_rareNonInheritedData, justifyContent, data); }
    void setJustifyContentPosition(ContentPosition position) { m_rareNonInheritedData.access().justifyContent.setPosition(position); }
    void setJustifyItems(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, justifyItems, data); }
    void setJustifySelf(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, justifySelf, data); }
    void setJustifySelfPosition(ItemPosition position) { m_rareNonInheritedData.access().justifySelf.setPosition(position); }

#if ENABLE(CSS_BOX_DECORATION_BREAK)
    void setBoxDecorationBreak(BoxDecorationBreak b) { SET_VAR(m_boxData, m_boxDecorationBreak, static_cast<unsigned>(b)); }
#endif

    void setGridColumnList(const GridTrackList& list)
    {
        if (!compareEqual(m_rareNonInheritedData->grid->columns(), list))
            m_rareNonInheritedData.access().grid.access().setColumns(list);
    }
    void setGridRowList(const GridTrackList& list)
    {
        if (!compareEqual(m_rareNonInheritedData->grid->rows(), list))
            m_rareNonInheritedData.access().grid.access().setRows(list);
    }
    void setGridAutoColumns(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoColumns, trackSizeList); }
    void setGridAutoRows(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoRows, trackSizeList); }
    void setImplicitNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, implicitNamedGridColumnLines, namedGridColumnLines); }
    void setImplicitNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, implicitNamedGridRowLines, namedGridRowLines); }
    void setNamedGridArea(const NamedGridAreaMap& namedGridArea) { SET_NESTED_VAR(m_rareNonInheritedData, grid, namedGridArea, namedGridArea); }
    void setNamedGridAreaRowCount(size_t rowCount) { SET_NESTED_VAR(m_rareNonInheritedData, grid, namedGridAreaRowCount, rowCount); }
    void setNamedGridAreaColumnCount(size_t columnCount) { SET_NESTED_VAR(m_rareNonInheritedData, grid, namedGridAreaColumnCount, columnCount); }
    void setGridAutoFlow(GridAutoFlow flow) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoFlow, flow); }
    void setGridItemColumnStart(const GridPosition& columnStartPosition) { SET_NESTED_VAR(m_rareNonInheritedData, gridItem, gridColumnStart, columnStartPosition); }
    void setGridItemColumnEnd(const GridPosition& columnEndPosition) { SET_NESTED_VAR(m_rareNonInheritedData, gridItem, gridColumnEnd, columnEndPosition); }
    void setGridItemRowStart(const GridPosition& rowStartPosition) { SET_NESTED_VAR(m_rareNonInheritedData, gridItem, gridRowStart, rowStartPosition); }
    void setGridItemRowEnd(const GridPosition& rowEndPosition) { SET_NESTED_VAR(m_rareNonInheritedData, gridItem, gridRowEnd, rowEndPosition); }

    void setMarqueeIncrement(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, marquee, increment, WTFMove(length)); }
    void setMarqueeSpeed(int f) { SET_NESTED_VAR(m_rareNonInheritedData, marquee, speed, f); }
    void setMarqueeDirection(MarqueeDirection d) { SET_NESTED_VAR(m_rareNonInheritedData, marquee, direction, static_cast<unsigned>(d)); }
    void setMarqueeBehavior(MarqueeBehavior b) { SET_NESTED_VAR(m_rareNonInheritedData, marquee, behavior, static_cast<unsigned>(b)); }
    void setMarqueeLoopCount(int i) { SET_NESTED_VAR(m_rareNonInheritedData, marquee, loops, i); }
    void setUserModify(UserModify u) { SET_VAR(m_rareInheritedData, userModify, static_cast<unsigned>(u)); }
    void setUserDrag(UserDrag d) { SET_VAR(m_rareNonInheritedData, userDrag, static_cast<unsigned>(d)); }
    void setUserSelect(UserSelect s) { SET_VAR(m_rareInheritedData, userSelect, static_cast<unsigned>(s)); }
    void setTextOverflow(TextOverflow overflow) { SET_VAR(m_rareNonInheritedData, textOverflow, static_cast<unsigned>(overflow)); }
    void setWordBreak(WordBreak b) { SET_VAR(m_rareInheritedData, wordBreak, static_cast<unsigned>(b)); }
    void setOverflowWrap(OverflowWrap b) { SET_VAR(m_rareInheritedData, overflowWrap, static_cast<unsigned>(b)); }
    void setNBSPMode(NBSPMode b) { SET_VAR(m_rareInheritedData, nbspMode, static_cast<unsigned>(b)); }
    void setLineBreak(LineBreak b) { SET_VAR(m_rareInheritedData, lineBreak, static_cast<unsigned>(b)); }
    void setHyphens(Hyphens h) { SET_VAR(m_rareInheritedData, hyphens, static_cast<unsigned>(h)); }
    void setHyphenationLimitBefore(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitBefore, limit); }
    void setHyphenationLimitAfter(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitAfter, limit); }
    void setHyphenationLimitLines(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitLines, limit); }
    void setHyphenationString(const AtomString& h) { SET_VAR(m_rareInheritedData, hyphenationString, h); }
    void setResize(Resize r) { SET_VAR(m_rareNonInheritedData, resize, static_cast<unsigned>(r)); }
    void setColumnAxis(ColumnAxis axis) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, axis, static_cast<unsigned>(axis)); }
    void setColumnProgression(ColumnProgression progression) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, progression, static_cast<unsigned>(progression)); }
    void setColumnWidth(float f) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, autoWidth, false); SET_NESTED_VAR(m_rareNonInheritedData, multiCol, width, f); }
    void setHasAutoColumnWidth() { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, autoWidth, true); SET_NESTED_VAR(m_rareNonInheritedData, multiCol, width, 0); }
    void setColumnCount(unsigned short c) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, autoCount, false); SET_NESTED_VAR(m_rareNonInheritedData, multiCol, count, c); }
    void setHasAutoColumnCount() { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, autoCount, true); SET_NESTED_VAR(m_rareNonInheritedData, multiCol, count, 0); }
    void setColumnFill(ColumnFill columnFill) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, fill, static_cast<unsigned>(columnFill)); }
    void setColumnGap(GapLength&& gapLength) { SET_VAR(m_rareNonInheritedData, columnGap, WTFMove(gapLength)); }
    void setRowGap(GapLength&& gapLength) { SET_VAR(m_rareNonInheritedData, rowGap, WTFMove(gapLength)); }
    void setColumnRuleColor(const StyleColor& c) { SET_BORDERVALUE_COLOR(m_rareNonInheritedData.access().multiCol, rule, c); }
    void setColumnRuleStyle(BorderStyle b) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, rule.m_style, static_cast<unsigned>(b)); }
    void setColumnRuleWidth(unsigned short w) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, rule.m_width, w); }
    void resetColumnRule() { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, rule, BorderValue()); }
    void setColumnSpan(ColumnSpan columnSpan) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, columnSpan, static_cast<unsigned>(columnSpan)); }
    void inheritColumnPropertiesFrom(const RenderStyle& parent) { m_rareNonInheritedData.access().multiCol = parent.m_rareNonInheritedData->multiCol; }

    void setTransform(const TransformOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, transform, operations, ops); }
    void setTransformOriginX(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, transform, x, WTFMove(length)); }
    void setTransformOriginY(Length&& length) { SET_NESTED_VAR(m_rareNonInheritedData, transform, y, WTFMove(length)); }
    void setTransformOriginZ(float f) { SET_NESTED_VAR(m_rareNonInheritedData, transform, z, f); }
    void setTransformBox(TransformBox box) { SET_NESTED_VAR(m_rareNonInheritedData, transform, transformBox, box); }

    void setRotate(RefPtr<RotateTransformOperation>&&);
    void setScale(RefPtr<ScaleTransformOperation>&&);
    void setTranslate(RefPtr<TranslateTransformOperation>&&);

    void setSpeakAs(OptionSet<SpeakAs> s) { SET_VAR(m_rareInheritedData, speakAs, s.toRaw()); }
    void setTextCombine(TextCombine v) { SET_VAR(m_rareInheritedData, textCombine, static_cast<unsigned>(v)); }
    void setTextDecorationColor(const StyleColor& c) { SET_VAR(m_rareNonInheritedData, textDecorationColor, c); }
    void setTextEmphasisColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, textEmphasisColor, c); }
    void setTextEmphasisFill(TextEmphasisFill fill) { SET_VAR(m_rareInheritedData, textEmphasisFill, static_cast<unsigned>(fill)); }
    void setTextEmphasisMark(TextEmphasisMark mark) { SET_VAR(m_rareInheritedData, textEmphasisMark, static_cast<unsigned>(mark)); }
    void setTextEmphasisCustomMark(const AtomString& mark) { SET_VAR(m_rareInheritedData, textEmphasisCustomMark, mark); }
    void setTextEmphasisPosition(OptionSet<TextEmphasisPosition> position) { SET_VAR(m_rareInheritedData, textEmphasisPosition, static_cast<unsigned>(position.toRaw())); }
    bool setTextOrientation(TextOrientation);

    void setObjectFit(ObjectFit fit) { SET_VAR(m_rareNonInheritedData, objectFit, static_cast<unsigned>(fit)); }
    void setObjectPosition(LengthPoint&& position) { SET_VAR(m_rareNonInheritedData, objectPosition, WTFMove(position)); }

    void setRubyPosition(RubyPosition position) { SET_VAR(m_rareInheritedData, rubyPosition, static_cast<unsigned>(position)); }

#if ENABLE(DARK_MODE_CSS)
    void setColorScheme(StyleColorScheme supported) { SET_VAR(m_rareInheritedData, colorScheme, supported); }
#endif

    void setFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, filter, operations, ops); }
    void setAppleColorFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareInheritedData, appleColorFilter, operations, ops); }

#if ENABLE(FILTERS_LEVEL_2)
    void setBackdropFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, backdropFilter, operations, ops); }
#endif

    void setTabSize(const TabSize& size) { SET_VAR(m_rareInheritedData, tabSize, size); }

    void setBreakBefore(BreakBetween breakBehavior) { SET_VAR(m_rareNonInheritedData, breakBefore, static_cast<unsigned>(breakBehavior)); }
    void setBreakAfter(BreakBetween breakBehavior) { SET_VAR(m_rareNonInheritedData, breakAfter, static_cast<unsigned>(breakBehavior)); }
    void setBreakInside(BreakInside breakBehavior) { SET_VAR(m_rareNonInheritedData, breakInside, static_cast<unsigned>(breakBehavior)); }
    
    void setHangingPunctuation(OptionSet<HangingPunctuation> punctuation) { SET_VAR(m_rareInheritedData, hangingPunctuation, punctuation.toRaw()); }

    // End CSS3 Setters

    void setLineGrid(const AtomString& lineGrid) { SET_VAR(m_rareInheritedData, lineGrid, lineGrid); }
    void setLineSnap(LineSnap lineSnap) { SET_VAR(m_rareInheritedData, lineSnap, static_cast<unsigned>(lineSnap)); }
    void setLineAlign(LineAlign lineAlign) { SET_VAR(m_rareInheritedData, lineAlign, static_cast<unsigned>(lineAlign)); }

    void setPointerEvents(PointerEvents p) { m_inheritedFlags.pointerEvents = static_cast<unsigned>(p); }

    void clearAnimations();
    void clearTransitions();

    void adjustAnimations();
    void adjustTransitions();

    void setTransformStyle3D(TransformStyle3D b) { SET_VAR(m_rareNonInheritedData, transformStyle3D, static_cast<unsigned>(b)); }
    void setTransformStyleForcedToFlat(bool b) { SET_VAR(m_rareNonInheritedData, transformStyleForcedToFlat, static_cast<unsigned>(b)); }
    void setBackfaceVisibility(BackfaceVisibility b) { SET_VAR(m_rareNonInheritedData, backfaceVisibility, static_cast<unsigned>(b)); }
    void setPerspective(float p) { SET_VAR(m_rareNonInheritedData, perspective, p); }
    void setPerspectiveOriginX(Length&& length) { SET_VAR(m_rareNonInheritedData, perspectiveOriginX, WTFMove(length)); }
    void setPerspectiveOriginY(Length&& length) { SET_VAR(m_rareNonInheritedData, perspectiveOriginY, WTFMove(length)); }
    void setPageSize(LengthSize size) { SET_VAR(m_rareNonInheritedData, pageSize, WTFMove(size)); }
    void setPageSizeType(PageSizeType t) { SET_VAR(m_rareNonInheritedData, pageSizeType, t); }
    void resetPageSizeType() { SET_VAR(m_rareNonInheritedData, pageSizeType, PAGE_SIZE_AUTO); }

    void setLineBoxContain(OptionSet<LineBoxContain> c) { SET_VAR(m_rareInheritedData, lineBoxContain, c.toRaw()); }
    void setLineClamp(LineClampValue c) { SET_VAR(m_rareNonInheritedData, lineClamp, c); }
    
    void setInitialLetter(const IntSize& size) { SET_VAR(m_rareNonInheritedData, initialLetter, size); }
    
    void setTouchActions(OptionSet<TouchAction> touchActions) { SET_VAR(m_rareNonInheritedData, touchActions, touchActions); }
    void setEffectiveTouchActions(OptionSet<TouchAction> touchActions) { SET_VAR(m_rareInheritedData, effectiveTouchActions, touchActions); }
    void setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes) { SET_VAR(m_rareInheritedData, eventListenerRegionTypes, eventListenerTypes); }

    // internal property
    void setEffectiveInert(bool effectiveInert) { SET_VAR(m_rareInheritedData, effectiveInert, effectiveInert); }

    void setScrollMarginTop(Length&&);
    void setScrollMarginBottom(Length&&);
    void setScrollMarginLeft(Length&&);
    void setScrollMarginRight(Length&&);

    void setScrollPaddingTop(Length&&);
    void setScrollPaddingBottom(Length&&);
    void setScrollPaddingLeft(Length&&);
    void setScrollPaddingRight(Length&&);

    void setScrollSnapType(const ScrollSnapType);
    void setScrollSnapAlign(const ScrollSnapAlign&);
    void setScrollSnapStop(const ScrollSnapStop);

#if ENABLE(TOUCH_EVENTS)
    void setTapHighlightColor(const StyleColor& c) { SET_VAR(m_rareInheritedData, tapHighlightColor, c); }
#endif

#if PLATFORM(IOS_FAMILY)
    void setTouchCalloutEnabled(bool v) { SET_VAR(m_rareInheritedData, touchCalloutEnabled, v); }
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    void setUseTouchOverflowScrolling(bool v) { SET_VAR(m_rareInheritedData, useTouchOverflowScrolling, v); }
#endif

    void setUseSmoothScrolling(bool v) { SET_VAR(m_rareNonInheritedData, useSmoothScrolling, v); }

#if ENABLE(TEXT_AUTOSIZING)
    void setTextSizeAdjust(TextSizeAdjustment adjustment) { SET_VAR(m_rareInheritedData, textSizeAdjust, adjustment); }
    void setAutosizeStatus(AutosizeStatus);
#endif

    void setTextSecurity(TextSecurity security) { SET_VAR(m_rareInheritedData, textSecurity, static_cast<unsigned>(security)); }
    void setInputSecurity(InputSecurity security) { SET_VAR(m_rareNonInheritedData, inputSecurity, static_cast<unsigned>(security)); }

#if ENABLE(CSS_TRAILING_WORD)
    void setTrailingWord(TrailingWord) { }
#endif

#if ENABLE(APPLE_PAY)
    void setApplePayButtonStyle(ApplePayButtonStyle style) { SET_VAR(m_rareNonInheritedData, applePayButtonStyle, static_cast<unsigned>(style)); }
    void setApplePayButtonType(ApplePayButtonType type) { SET_VAR(m_rareNonInheritedData, applePayButtonType, static_cast<unsigned>(type)); }
#endif

#if ENABLE(CSS_PAINTING_API)
    void addCustomPaintWatchProperty(const AtomString& name);
#endif

    // Support for paint-order, stroke-linecap, stroke-linejoin, and stroke-miterlimit from https://drafts.fxtf.org/paint/.
    void setPaintOrder(PaintOrder order) { SET_VAR(m_rareInheritedData, paintOrder, static_cast<unsigned>(order)); }
    PaintOrder paintOrder() const { return static_cast<PaintOrder>(m_rareInheritedData->paintOrder); }
    static PaintOrder initialPaintOrder() { return PaintOrder::Normal; }
    static Vector<PaintType, 3> paintTypesForPaintOrder(PaintOrder);
    
    void setCapStyle(LineCap val) { SET_VAR(m_rareInheritedData, capStyle, static_cast<unsigned>(val)); }
    LineCap capStyle() const { return static_cast<LineCap>(m_rareInheritedData->capStyle); }
    static LineCap initialCapStyle() { return LineCap::Butt; }
    
    void setJoinStyle(LineJoin val) { SET_VAR(m_rareInheritedData, joinStyle, static_cast<unsigned>(val)); }
    LineJoin joinStyle() const { return static_cast<LineJoin>(m_rareInheritedData->joinStyle); }
    static LineJoin initialJoinStyle() { return LineJoin::Miter; }
    
    const Length& strokeWidth() const { return m_rareInheritedData->strokeWidth; }
    void setStrokeWidth(Length&& w) { SET_VAR(m_rareInheritedData, strokeWidth, WTFMove(w)); }
    bool hasVisibleStroke() const { return svgStyle().hasStroke() && !strokeWidth().isZero(); }
    static Length initialStrokeWidth() { return initialOneLength(); }

    float computedStrokeWidth(const IntSize& viewportSize) const;
    void setHasExplicitlySetStrokeWidth(bool v) { SET_VAR(m_rareInheritedData, hasSetStrokeWidth, static_cast<unsigned>(v)); }
    bool hasExplicitlySetStrokeWidth() const { return m_rareInheritedData->hasSetStrokeWidth; };
    bool hasPositiveStrokeWidth() const;
    
    StyleColor strokeColor() const { return m_rareInheritedData->strokeColor; }
    void setStrokeColor(const StyleColor& v)  { SET_VAR(m_rareInheritedData, strokeColor, v); }
    void setVisitedLinkStrokeColor(const StyleColor& v) { SET_VAR(m_rareInheritedData, visitedLinkStrokeColor, v); }
    const StyleColor& visitedLinkStrokeColor() const { return m_rareInheritedData->visitedLinkStrokeColor; }
    void setHasExplicitlySetStrokeColor(bool v) { SET_VAR(m_rareInheritedData, hasSetStrokeColor, static_cast<unsigned>(v)); }
    bool hasExplicitlySetStrokeColor() const { return m_rareInheritedData->hasSetStrokeColor; };
    static StyleColor initialStrokeColor() { return { Color::transparentBlack }; }
    Color computedStrokeColor() const;
    CSSPropertyID effectiveStrokeColorProperty() const { return hasExplicitlySetStrokeColor() ? CSSPropertyStrokeColor : CSSPropertyWebkitTextStrokeColor; }

    float strokeMiterLimit() const { return m_rareInheritedData->miterLimit; }
    void setStrokeMiterLimit(float f) { SET_VAR(m_rareInheritedData, miterLimit, f); }
    static float initialStrokeMiterLimit() { return defaultMiterLimit; }


    const SVGRenderStyle& svgStyle() const { return m_svgStyle; }
    SVGRenderStyle& accessSVGStyle() { return m_svgStyle.access(); }

    SVGPaintType fillPaintType() const { return svgStyle().fillPaintType(); }
    StyleColor fillPaintColor() const { return svgStyle().fillPaintColor(); }
    void setFillPaintColor(const StyleColor& color) { accessSVGStyle().setFillPaint(SVGPaintType::RGBColor, color, emptyString()); }
    float fillOpacity() const { return svgStyle().fillOpacity(); }
    void setFillOpacity(float f) { accessSVGStyle().setFillOpacity(f); }

    SVGPaintType strokePaintType() const { return svgStyle().strokePaintType(); }
    StyleColor strokePaintColor() const { return svgStyle().strokePaintColor(); }
    void setStrokePaintColor(const StyleColor& color) { accessSVGStyle().setStrokePaint(SVGPaintType::RGBColor, color, emptyString()); }
    float strokeOpacity() const { return svgStyle().strokeOpacity(); }
    void setStrokeOpacity(float f) { accessSVGStyle().setStrokeOpacity(f); }
    Vector<SVGLengthValue> strokeDashArray() const { return svgStyle().strokeDashArray(); }
    void setStrokeDashArray(Vector<SVGLengthValue> array) { accessSVGStyle().setStrokeDashArray(array); }
    const Length& strokeDashOffset() const { return svgStyle().strokeDashOffset(); }
    void setStrokeDashOffset(Length&& d) { accessSVGStyle().setStrokeDashOffset(WTFMove(d)); }

    const Length& cx() const { return svgStyle().cx(); }
    void setCx(Length&& cx) { accessSVGStyle().setCx(WTFMove(cx)); }
    const Length& cy() const { return svgStyle().cy(); }
    void setCy(Length&& cy) { accessSVGStyle().setCy(WTFMove(cy)); }
    const Length& r() const { return svgStyle().r(); }
    void setR(Length&& r) { accessSVGStyle().setR(WTFMove(r)); }
    const Length& rx() const { return svgStyle().rx(); }
    void setRx(Length&& rx) { accessSVGStyle().setRx(WTFMove(rx)); }
    const Length& ry() const { return svgStyle().ry(); }
    void setRy(Length&& ry) { accessSVGStyle().setRy(WTFMove(ry)); }
    const Length& x() const { return svgStyle().x(); }
    void setX(Length&& x) { accessSVGStyle().setX(WTFMove(x)); }
    const Length& y() const { return svgStyle().y(); }
    void setY(Length&& y) { accessSVGStyle().setY(WTFMove(y)); }

    float floodOpacity() const { return svgStyle().floodOpacity(); }
    void setFloodOpacity(float f) { accessSVGStyle().setFloodOpacity(f); }

    float stopOpacity() const { return svgStyle().stopOpacity(); }
    void setStopOpacity(float f) { accessSVGStyle().setStopOpacity(f); }

    void setStopColor(const StyleColor& c) { accessSVGStyle().setStopColor(c); }
    void setFloodColor(const StyleColor& c) { accessSVGStyle().setFloodColor(c); }
    void setLightingColor(const StyleColor& c) { accessSVGStyle().setLightingColor(c); }

    SVGLengthValue baselineShiftValue() const { return svgStyle().baselineShiftValue(); }
    void setBaselineShiftValue(SVGLengthValue s) { accessSVGStyle().setBaselineShiftValue(s); }
    SVGLengthValue kerning() const { return svgStyle().kerning(); }
    void setKerning(SVGLengthValue k) { accessSVGStyle().setKerning(k); }

    void setShapeOutside(RefPtr<ShapeValue>&&);
    ShapeValue* shapeOutside() const { return m_rareNonInheritedData->shapeOutside.get(); }
    static ShapeValue* initialShapeOutside() { return nullptr; }

    const Length& shapeMargin() const { return m_rareNonInheritedData->shapeMargin; }
    void setShapeMargin(Length&& shapeMargin) { SET_VAR(m_rareNonInheritedData, shapeMargin, WTFMove(shapeMargin)); }
    static Length initialShapeMargin() { return Length(0, LengthType::Fixed); }

    float shapeImageThreshold() const { return m_rareNonInheritedData->shapeImageThreshold; }
    void setShapeImageThreshold(float);
    static float initialShapeImageThreshold() { return 0; }

    void setClipPath(RefPtr<PathOperation>&&);
    PathOperation* clipPath() const { return m_rareNonInheritedData->clipPath.get(); }
    static PathOperation* initialClipPath() { return nullptr; }

    bool hasEffectiveContentNone() const { return !contentData() && (m_nonInheritedFlags.hasContentNone || styleType() == PseudoId::Before || styleType() == PseudoId::After); }
    bool hasContent() const { return contentData(); }
    const ContentData* contentData() const { return m_rareNonInheritedData->content.get(); }
    void setContent(std::unique_ptr<ContentData>, bool add);
    bool contentDataEquivalent(const RenderStyle* otherStyle) const { return const_cast<RenderStyle*>(this)->m_rareNonInheritedData->contentDataEquivalent(*const_cast<RenderStyle*>(otherStyle)->m_rareNonInheritedData); }
    void clearContent();
    void setHasContentNone(bool v) { m_nonInheritedFlags.hasContentNone = v; }
    void setContent(const String&, bool add = false);
    void setContent(RefPtr<StyleImage>&&, bool add = false);
    void setContent(std::unique_ptr<CounterContent>, bool add = false);
    void setContent(QuoteType, bool add = false);
    void setContentAltText(const String&);
    const String& contentAltText() const;
    bool hasAttrContent() const { return m_rareNonInheritedData->hasAttrContent; }
    void setHasAttrContent();

    const CounterDirectiveMap* counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();

    QuotesData* quotes() const { return m_rareInheritedData->quotes.get(); }
    void setQuotes(RefPtr<QuotesData>&&);

    WillChangeData* willChange() const { return m_rareNonInheritedData->willChange.get(); }
    void setWillChange(RefPtr<WillChangeData>&&);

    bool willChangeCreatesStackingContext() const;

    const AtomString& hyphenString() const;

    bool inheritedEqual(const RenderStyle&) const;
    bool fastPathInheritedEqual(const RenderStyle&) const;
    bool nonFastPathInheritedEqual(const RenderStyle&) const;

    bool descendantAffectingNonInheritedPropertiesEqual(const RenderStyle&) const;

#if ENABLE(TEXT_AUTOSIZING)
    uint32_t hashForTextAutosizing() const;
    bool equalForTextAutosizing(const RenderStyle&) const;
#endif

    StyleDifference diff(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool diffRequiresLayerRepaint(const RenderStyle&, bool isComposited) const;

    bool isDisplayInlineType() const { return isDisplayInlineType(display()); }
    bool isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
    bool isDisplayFlexibleOrGridBox() const { return isDisplayFlexibleOrGridBox(display()); }
    bool isDisplayFlexibleBoxIncludingDeprecatedOrGridBox() const { return isDisplayFlexibleOrGridBox() || isDisplayDeprecatedFlexibleBox(display()); }
    bool isDisplayRegionType() const;
    bool isDisplayBlockLevel() const { return isDisplayBlockType(display()); }
    bool isOriginalDisplayBlockType() const { return isDisplayBlockType(originalDisplay()); }
    bool isDisplayTableOrTablePart() const { return isDisplayTableOrTablePart(display()); }
    bool isOriginalDisplayListItemType() const { return isDisplayListItemType(originalDisplay()); }

    bool setWritingMode(WritingMode);

    bool hasExplicitlySetWritingMode() const { return m_nonInheritedFlags.hasExplicitlySetWritingMode; }
    void setHasExplicitlySetWritingMode(bool v) { m_nonInheritedFlags.hasExplicitlySetWritingMode = v; }

    // A unique style is one that has matches something that makes it impossible to share.
    bool unique() const { return m_nonInheritedFlags.isUnique; }
    void setUnique() { m_nonInheritedFlags.isUnique = true; }

    bool emptyState() const { return m_nonInheritedFlags.emptyState; }
    void setEmptyState(bool v) { setUnique(); m_nonInheritedFlags.emptyState = v; }
    bool firstChildState() const { return m_nonInheritedFlags.firstChildState; }
    void setFirstChildState() { setUnique(); m_nonInheritedFlags.firstChildState = true; }
    bool lastChildState() const { return m_nonInheritedFlags.lastChildState; }
    void setLastChildState() { setUnique(); m_nonInheritedFlags.lastChildState = true; }

    StyleColor unresolvedColorForProperty(CSSPropertyID colorProperty, bool visitedLink = false) const;
    Color colorResolvingCurrentColor(CSSPropertyID colorProperty, bool visitedLink) const;
    // Resolves the currentColor keyword (except for the "color" property which has specific semantic).
    WEBCORE_EXPORT Color colorResolvingCurrentColor(const StyleColor&) const;

    WEBCORE_EXPORT Color visitedDependentColor(CSSPropertyID) const;
    WEBCORE_EXPORT Color visitedDependentColorWithColorFilter(CSSPropertyID) const;

    WEBCORE_EXPORT Color colorByApplyingColorFilter(const Color&) const;
    WEBCORE_EXPORT Color colorWithColorFilter(const StyleColor&) const;

    bool backgroundColorEqualsToColorIgnoringVisited(const StyleColor& color) const { return color == backgroundColor(); }

    void setHasExplicitlyInheritedProperties() { m_nonInheritedFlags.hasExplicitlyInheritedProperties = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedFlags.hasExplicitlyInheritedProperties; }

    bool disallowsFastPathInheritance() const { return m_nonInheritedFlags.disallowsFastPathInheritance; }
    void setDisallowsFastPathInheritance() { m_nonInheritedFlags.disallowsFastPathInheritance = true; }

    void setMathStyle(const MathStyle& v) { SET_VAR(m_rareInheritedData, mathStyle, static_cast<unsigned>(v)); }

    // Initial values for all the properties
    static Overflow initialOverflowX() { return Overflow::Visible; }
    static Overflow initialOverflowY() { return Overflow::Visible; }
    static OverscrollBehavior initialOverscrollBehaviorX() { return OverscrollBehavior::Auto; }
    static OverscrollBehavior initialOverscrollBehaviorY() { return OverscrollBehavior::Auto; }

    static Clear initialClear() { return Clear::None; }
    static DisplayType initialDisplay() { return DisplayType::Inline; }
    static UnicodeBidi initialUnicodeBidi() { return UnicodeBidi::Normal; }
    static PositionType initialPosition() { return PositionType::Static; }
    static VerticalAlign initialVerticalAlign() { return VerticalAlign::Baseline; }
    static Float initialFloating() { return Float::None; }
    static BreakBetween initialBreakBetween() { return BreakBetween::Auto; }
    static BreakInside initialBreakInside() { return BreakInside::Auto; }
    static OptionSet<HangingPunctuation> initialHangingPunctuation() { return OptionSet<HangingPunctuation> { }; }
    static TableLayoutType initialTableLayout() { return TableLayoutType::Auto; }
    static BorderCollapse initialBorderCollapse() { return BorderCollapse::Separate; }
    static BorderStyle initialBorderStyle() { return BorderStyle::None; }
    static OutlineIsAuto initialOutlineStyleIsAuto() { return OutlineIsAuto::Off; }
    static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
    static LengthSize initialBorderRadius() { return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } }; }
    static CaptionSide initialCaptionSide() { return CaptionSide::Top; }
    static ColumnAxis initialColumnAxis() { return ColumnAxis::Auto; }
    static ColumnProgression initialColumnProgression() { return ColumnProgression::Normal; }
    static TextDirection initialDirection() { return TextDirection::LTR; }
    static WritingMode initialWritingMode() { return WritingMode::TopToBottom; }
    static TextCombine initialTextCombine() { return TextCombine::None; }
    static TextOrientation initialTextOrientation() { return TextOrientation::Mixed; }
    static ObjectFit initialObjectFit() { return ObjectFit::Fill; }
    static LengthPoint initialObjectPosition() { return LengthPoint(Length(50.0f, LengthType::Percent), Length(50.0f, LengthType::Percent)); }
    static EmptyCell initialEmptyCells() { return EmptyCell::Show; }
    static ListStylePosition initialListStylePosition() { return ListStylePosition::Outside; }
    static const AtomString& initialListStyleStringValue() { return nullAtom(); }
    static ListStyleType initialListStyleType() { return ListStyleType::Disc; }
    static TextTransform initialTextTransform() { return TextTransform::None; }
    static Visibility initialVisibility() { return Visibility::Visible; }
    static WhiteSpace initialWhiteSpace() { return WhiteSpace::Normal; }
    static float initialHorizontalBorderSpacing() { return 0; }
    static float initialVerticalBorderSpacing() { return 0; }
    static CursorType initialCursor() { return CursorType::Auto; }
    static Color initialColor() { return Color::black; }
    static StyleColor initialTextStrokeColor() { return currentColor(); }
    static StyleColor initialTextDecorationColor() { return currentColor(); }
    static StyleImage* initialListStyleImage() { return 0; }
    static float initialBorderWidth() { return 3; }
    static unsigned short initialColumnRuleWidth() { return 3; }
    static float initialOutlineWidth() { return 3; }
    static float initialLetterSpacing() { return 0; }
    static Length initialWordSpacing() { return Length(LengthType::Fixed); }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(); }
    static Length initialMaxSize() { return Length(LengthType::Undefined); }
    static Length initialOffset() { return Length(); }
    static Length initialRadius() { return Length(); }
    static Length initialMargin() { return Length(LengthType::Fixed); }
    static Length initialPadding() { return Length(LengthType::Fixed); }
    static Length initialTextIndent() { return Length(LengthType::Fixed); }
    static Length initialZeroLength() { return Length(LengthType::Fixed); }
    static Length initialOneLength() { return Length(1, LengthType::Fixed); }
    static unsigned short initialWidows() { return 2; }
    static unsigned short initialOrphans() { return 2; }
    // Returning -100% percent here means the line-height is not set.
    static Length initialLineHeight() { return Length(-100.0f, LengthType::Percent); }
    static TextAlignMode initialTextAlign() { return TextAlignMode::Start; }
    static TextAlignLast initialTextAlignLast() { return TextAlignLast::Auto; }
    static OptionSet<TextDecorationLine> initialTextDecorationLine() { return OptionSet<TextDecorationLine> { }; }
    static TextDecorationStyle initialTextDecorationStyle() { return TextDecorationStyle::Solid; }
    static TextDecorationSkipInk initialTextDecorationSkipInk() { return TextDecorationSkipInk::Auto; }
    static TextUnderlinePosition initialTextUnderlinePosition() { return TextUnderlinePosition::Auto; }
    static TextUnderlineOffset initialTextUnderlineOffset() { return TextUnderlineOffset::createWithAuto(); }
    static TextDecorationThickness initialTextDecorationThickness() { return TextDecorationThickness::createWithAuto(); }
    static float initialZoom() { return 1.0f; }
    static TextZoom initialTextZoom() { return TextZoom::Normal; }
    static float initialOutlineOffset() { return 0; }
    static float initialOpacity() { return 1.0f; }
    static BoxAlignment initialBoxAlign() { return BoxAlignment::Stretch; }
    static BoxDecorationBreak initialBoxDecorationBreak() { return BoxDecorationBreak::Slice; }
    static BoxDirection initialBoxDirection() { return BoxDirection::Normal; }
    static BoxLines initialBoxLines() { return BoxLines::Single; }
    static BoxOrient initialBoxOrient() { return BoxOrient::Horizontal; }
    static BoxPack initialBoxPack() { return BoxPack::Start; }
    static float initialBoxFlex() { return 0.0f; }
    static unsigned initialBoxFlexGroup() { return 1; }
    static unsigned initialBoxOrdinalGroup() { return 1; }
    static BoxSizing initialBoxSizing() { return BoxSizing::ContentBox; }
    static StyleReflection* initialBoxReflect() { return 0; }
    static float initialFlexGrow() { return 0; }
    static float initialFlexShrink() { return 1; }
    static Length initialFlexBasis() { return Length(LengthType::Auto); }
    static int initialOrder() { return 0; }
    static StyleSelfAlignmentData initialJustifyItems() { return StyleSelfAlignmentData(ItemPosition::Legacy, OverflowAlignment::Default); }
    static StyleSelfAlignmentData initialSelfAlignment() { return StyleSelfAlignmentData(ItemPosition::Auto, OverflowAlignment::Default); }
    static StyleSelfAlignmentData initialDefaultAlignment() { return StyleSelfAlignmentData(ItemPosition::Normal, OverflowAlignment::Default); }
    static StyleContentAlignmentData initialContentAlignment() { return StyleContentAlignmentData(ContentPosition::Normal, ContentDistribution::Default, OverflowAlignment::Default); }
    static FlexDirection initialFlexDirection() { return FlexDirection::Row; }
    static FlexWrap initialFlexWrap() { return FlexWrap::NoWrap; }
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static Length initialMarqueeIncrement() { return Length(6, LengthType::Fixed); }
    static MarqueeBehavior initialMarqueeBehavior() { return MarqueeBehavior::Scroll; }
    static MarqueeDirection initialMarqueeDirection() { return MarqueeDirection::Auto; }
    static UserModify initialUserModify() { return UserModify::ReadOnly; }
    static UserDrag initialUserDrag() { return UserDrag::Auto; }
    static UserSelect initialUserSelect() { return UserSelect::Text; }
    static TextOverflow initialTextOverflow() { return TextOverflow::Clip; }
    static WordBreak initialWordBreak() { return WordBreak::Normal; }
    static OverflowWrap initialOverflowWrap() { return OverflowWrap::Normal; }
    static NBSPMode initialNBSPMode() { return NBSPMode::Normal; }
    static LineBreak initialLineBreak() { return LineBreak::Auto; }
    static OptionSet<SpeakAs> initialSpeakAs() { return OptionSet<SpeakAs> { }; }
    static Hyphens initialHyphens() { return Hyphens::Manual; }
    static short initialHyphenationLimitBefore() { return -1; }
    static short initialHyphenationLimitAfter() { return -1; }
    static short initialHyphenationLimitLines() { return -1; }
    static const AtomString& initialHyphenationString() { return nullAtom(); }
    static Resize initialResize() { return Resize::None; }
    static ControlPart initialAppearance() { return NoControlPart; }
    static AspectRatioType initialAspectRatioType() { return AspectRatioType::Auto; }
    static OptionSet<Containment> initialContainment() { return OptionSet<Containment> { }; }
    static OptionSet<Containment> strictContainment() { return OptionSet<Containment> { Containment::Size, Containment::Layout, Containment::Paint, Containment::Style }; }
    static OptionSet<Containment> contentContainment() { return OptionSet<Containment> { Containment::Layout, Containment::Paint, Containment::Style }; }
    static ContainerType initialContainerType() { return ContainerType::Normal; }
    static constexpr ContentVisibility initialContentVisibility() { return ContentVisibility::Visible; }
    static Vector<AtomString> initialContainerNames() { return { }; }
    static double initialAspectRatioWidth() { return 1.0; }
    static double initialAspectRatioHeight() { return 1.0; }

    static ContainIntrinsicSizeType initialContainIntrinsicWidthType() { return ContainIntrinsicSizeType::None; }
    static ContainIntrinsicSizeType initialContainIntrinsicHeightType() { return ContainIntrinsicSizeType::None; }
    static std::optional<Length> initialContainIntrinsicWidth() { return std::nullopt; }
    static std::optional<Length> initialContainIntrinsicHeight() { return std::nullopt; }

    static Order initialRTLOrdering() { return Order::Logical; }
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static ColumnFill initialColumnFill() { return ColumnFill::Balance; }
    static ColumnSpan initialColumnSpan() { return ColumnSpan::None; }
    static GapLength initialColumnGap() { return GapLength(); }
    static GapLength initialRowGap() { return GapLength(); }
    static const TransformOperations& initialTransform() { static NeverDestroyed<TransformOperations> ops; return ops; }
    static Length initialTransformOriginX() { return Length(50.0f, LengthType::Percent); }
    static Length initialTransformOriginY() { return Length(50.0f, LengthType::Percent); }
    static TransformBox initialTransformBox() { return TransformBox::ViewBox; }
    static RotateTransformOperation* initialRotate() { return nullptr; }
    static ScaleTransformOperation* initialScale() { return nullptr; }
    static TranslateTransformOperation* initialTranslate() { return nullptr; }
    static PointerEvents initialPointerEvents() { return PointerEvents::Auto; }
    static float initialTransformOriginZ() { return 0; }
    static TransformStyle3D initialTransformStyle3D() { return TransformStyle3D::Flat; }
    static BackfaceVisibility initialBackfaceVisibility() { return BackfaceVisibility::Visible; }
    static float initialPerspective() { return -1; }
    static Length initialPerspectiveOriginX() { return Length(50.0f, LengthType::Percent); }
    static Length initialPerspectiveOriginY() { return Length(50.0f, LengthType::Percent); }
    static StyleColor initialBackgroundColor() { return Color::transparentBlack; }
    static StyleColor initialTextEmphasisColor() { return currentColor(); }
    static TextEmphasisFill initialTextEmphasisFill() { return TextEmphasisFill::Filled; }
    static TextEmphasisMark initialTextEmphasisMark() { return TextEmphasisMark::None; }
    static const AtomString& initialTextEmphasisCustomMark() { return nullAtom(); }
    static OptionSet<TextEmphasisPosition> initialTextEmphasisPosition() { return { TextEmphasisPosition::Over, TextEmphasisPosition::Right }; }
    static RubyPosition initialRubyPosition() { return RubyPosition::Before; }
    static OptionSet<LineBoxContain> initialLineBoxContain() { return { LineBoxContain::Block, LineBoxContain::Inline, LineBoxContain::Replaced }; }
    static ImageOrientation initialImageOrientation() { return ImageOrientation::FromImage; }
    static ImageRendering initialImageRendering() { return ImageRendering::Auto; }
    static ImageResolutionSource initialImageResolutionSource() { return ImageResolutionSource::Specified; }
    static ImageResolutionSnap initialImageResolutionSnap() { return ImageResolutionSnap::None; }
    static float initialImageResolution() { return 1; }
    static StyleImage* initialBorderImageSource() { return nullptr; }
    static StyleImage* initialMaskBoxImageSource() { return nullptr; }
    static PrintColorAdjust initialPrintColorAdjust() { return PrintColorAdjust::Economy; }
    static QuotesData* initialQuotes() { return nullptr; }
    static const AtomString& initialContentAltText() { return emptyAtom(); }

#if ENABLE(DARK_MODE_CSS)
    static StyleColorScheme initialColorScheme() { return { }; }
#endif

    static TextIndentLine initialTextIndentLine() { return TextIndentLine::FirstLine; }
    static TextIndentType initialTextIndentType() { return TextIndentType::Normal; }
    static TextJustify initialTextJustify() { return TextJustify::Auto; }

#if ENABLE(CURSOR_VISIBILITY)
    static CursorVisibility initialCursorVisibility() { return CursorVisibility::Auto; }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    static Length initialSpecifiedLineHeight() { return Length(-100.0f, LengthType::Percent); }
    static TextSizeAdjustment initialTextSizeAdjust() { return TextSizeAdjustment(); }
#endif

    static WillChangeData* initialWillChange() { return nullptr; }

    static TouchAction initialTouchActions() { return TouchAction::Auto; }

    static Length initialScrollMargin() { return Length(LengthType::Fixed); }
    static Length initialScrollPadding() { return Length(LengthType::Auto); }

    static ScrollSnapType initialScrollSnapType();
    static ScrollSnapAlign initialScrollSnapAlign();
    static ScrollSnapStop initialScrollSnapStop();

#if ENABLE(CSS_TRAILING_WORD)
    static TrailingWord initialTrailingWord() { return TrailingWord::Auto; }
#endif

#if ENABLE(APPLE_PAY)
    static ApplePayButtonStyle initialApplePayButtonStyle() { return ApplePayButtonStyle::Black; }
    static ApplePayButtonType initialApplePayButtonType() { return ApplePayButtonType::Plain; }
#endif

    // The initial value is 'none' for grid tracks.
    static Vector<GridTrackSize> initialGridColumns() { return Vector<GridTrackSize>(); }
    static Vector<GridTrackSize> initialGridRows() { return Vector<GridTrackSize>(); }

    static Vector<GridTrackSize> initialGridAutoRepeatTracks() { return Vector<GridTrackSize>(); }
    static unsigned initialGridAutoRepeatInsertionPoint() { return 0; }
    static AutoRepeatType initialGridAutoRepeatType() { return AutoRepeatType::None; }

    static GridAutoFlow initialGridAutoFlow() { return AutoFlowRow; }

    static Vector<GridTrackSize> initialGridAutoColumns() { return { GridTrackSize(Length(LengthType::Auto)) }; }
    static Vector<GridTrackSize> initialGridAutoRows() { return { GridTrackSize(Length(LengthType::Auto)) }; }

    static NamedGridAreaMap initialNamedGridArea() { return NamedGridAreaMap(); }
    static size_t initialNamedGridAreaCount() { return 0; }

    static NamedGridLinesMap initialNamedGridColumnLines() { return NamedGridLinesMap(); }
    static NamedGridLinesMap initialNamedGridRowLines() { return NamedGridLinesMap(); }

    static OrderedNamedGridLinesMap initialOrderedNamedGridColumnLines() { return OrderedNamedGridLinesMap(); }
    static OrderedNamedGridLinesMap initialOrderedNamedGridRowLines() { return OrderedNamedGridLinesMap(); }

    // 'auto' is the default.
    static GridPosition initialGridItemColumnStart() { return GridPosition(); }
    static GridPosition initialGridItemColumnEnd() { return GridPosition(); }
    static GridPosition initialGridItemRowStart() { return GridPosition(); }
    static GridPosition initialGridItemRowEnd() { return GridPosition(); }

    static TabSize initialTabSize() { return 8; }

    static const AtomString& initialLineGrid() { return nullAtom(); }
    static LineSnap initialLineSnap() { return LineSnap::None; }
    static LineAlign initialLineAlign() { return LineAlign::None; }

    static IntSize initialInitialLetter() { return IntSize(); }
    static LineClampValue initialLineClamp() { return LineClampValue(); }
    static TextSecurity initialTextSecurity() { return TextSecurity::None; }
    static InputSecurity initialInputSecurity() { return InputSecurity::Auto; };

#if PLATFORM(IOS_FAMILY)
    static bool initialTouchCalloutEnabled() { return true; }
#endif

#if ENABLE(TOUCH_EVENTS)
    static StyleColor initialTapHighlightColor();
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static bool initialUseTouchOverflowScrolling() { return false; }
#endif

    static bool initialUseSmoothScrolling() { return false; }

    static const FilterOperations& initialFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }
    static const FilterOperations& initialAppleColorFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }

#if ENABLE(FILTERS_LEVEL_2)
    static const FilterOperations& initialBackdropFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }
#endif

#if ENABLE(CSS_COMPOSITING)
    static BlendMode initialBlendMode() { return BlendMode::Normal; }
    static Isolation initialIsolation() { return Isolation::Auto; }
#endif

    static MathStyle initialMathStyle() { return MathStyle::Normal; }

    // Indicates the style is likely to change due to a pending stylesheet load.
    bool isNotFinal() const { return m_rareNonInheritedData->isNotFinal; }
    void setIsNotFinal() { SET_VAR(m_rareNonInheritedData, isNotFinal, true); }

    void setVisitedLinkColor(const Color&);
    void setVisitedLinkBackgroundColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBackgroundColor, v); }
    void setVisitedLinkBorderLeftColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderLeftColor, v); }
    void setVisitedLinkBorderRightColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderRightColor, v); }
    void setVisitedLinkBorderBottomColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderBottomColor, v); }
    void setVisitedLinkBorderTopColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderTopColor, v); }
    void setVisitedLinkOutlineColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkOutlineColor, v); }
    void setVisitedLinkColumnRuleColor(const StyleColor& v) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, visitedLinkColumnRuleColor, v); }
    void setVisitedLinkTextDecorationColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, visitedLinkTextDecorationColor, v); }
    void setVisitedLinkTextEmphasisColor(const StyleColor& v) { SET_VAR(m_rareInheritedData, visitedLinkTextEmphasisColor, v); }
    void setVisitedLinkTextFillColor(const StyleColor& v) { SET_VAR(m_rareInheritedData, visitedLinkTextFillColor, v); }
    void setVisitedLinkTextStrokeColor(const StyleColor& v) { SET_VAR(m_rareInheritedData, visitedLinkTextStrokeColor, v); }
    void setVisitedLinkCaretColor(const StyleColor& v) { SET_VAR(m_rareInheritedData, visitedLinkCaretColor, v); SET_VAR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, false); }
    void setHasVisitedLinkAutoCaretColor() { SET_VAR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, true); SET_VAR(m_rareInheritedData, visitedLinkCaretColor, currentColor()); }

    void inheritUnicodeBidiFrom(const RenderStyle* parent) { m_nonInheritedFlags.unicodeBidi = parent->m_nonInheritedFlags.unicodeBidi; }

    void getShadowInlineDirectionExtent(const ShadowData*, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const;
    void getShadowBlockDirectionExtent(const ShadowData*, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const;

    static StyleColor currentColor() { return StyleColor::currentColor(); }
    static bool isCurrentColor(const StyleColor& color) { return color.isCurrentColor(); }

    const StyleColor& borderLeftColor() const { return m_surroundData->border.left().color(); }
    const StyleColor& borderRightColor() const { return m_surroundData->border.right().color(); }
    const StyleColor& borderTopColor() const { return m_surroundData->border.top().color(); }
    const StyleColor& borderBottomColor() const { return m_surroundData->border.bottom().color(); }
    const StyleColor& backgroundColor() const { return m_backgroundData->color; }
    WEBCORE_EXPORT const Color& color() const;
    const StyleColor& columnRuleColor() const { return m_rareNonInheritedData->multiCol->rule.color(); }
    const StyleColor& outlineColor() const { return m_backgroundData->outline.color(); }
    const StyleColor& textEmphasisColor() const { return m_rareInheritedData->textEmphasisColor; }
    const StyleColor& textFillColor() const { return m_rareInheritedData->textFillColor; }
    static StyleColor initialTextFillColor() { return currentColor() ; }
    const StyleColor& textStrokeColor() const { return m_rareInheritedData->textStrokeColor; }
    const StyleColor& caretColor() const { return m_rareInheritedData->caretColor; }
    bool hasAutoCaretColor() const { return m_rareInheritedData->hasAutoCaretColor; }
    const Color& visitedLinkColor() const;
    const StyleColor& visitedLinkBackgroundColor() const { return m_rareNonInheritedData->visitedLinkBackgroundColor; }
    const StyleColor& visitedLinkBorderLeftColor() const { return m_rareNonInheritedData->visitedLinkBorderLeftColor; }
    const StyleColor& visitedLinkBorderRightColor() const { return m_rareNonInheritedData->visitedLinkBorderRightColor; }
    const StyleColor& visitedLinkBorderBottomColor() const { return m_rareNonInheritedData->visitedLinkBorderBottomColor; }
    const StyleColor& visitedLinkBorderTopColor() const { return m_rareNonInheritedData->visitedLinkBorderTopColor; }
    const StyleColor& visitedLinkOutlineColor() const { return m_rareNonInheritedData->visitedLinkOutlineColor; }
    const StyleColor& visitedLinkColumnRuleColor() const { return m_rareNonInheritedData->multiCol->visitedLinkColumnRuleColor; }
    const StyleColor& textDecorationColor() const { return m_rareNonInheritedData->textDecorationColor; }
    const StyleColor& visitedLinkTextDecorationColor() const { return m_rareNonInheritedData->visitedLinkTextDecorationColor; }
    const StyleColor& visitedLinkTextEmphasisColor() const { return m_rareInheritedData->visitedLinkTextEmphasisColor; }
    const StyleColor& visitedLinkTextFillColor() const { return m_rareInheritedData->visitedLinkTextFillColor; }
    const StyleColor& visitedLinkTextStrokeColor() const { return m_rareInheritedData->visitedLinkTextStrokeColor; }
    const StyleColor& visitedLinkCaretColor() const { return m_rareInheritedData->visitedLinkCaretColor; }
    bool hasVisitedLinkAutoCaretColor() const { return m_rareInheritedData->hasVisitedLinkAutoCaretColor; }

    const StyleColor& stopColor() const { return svgStyle().stopColor(); }
    const StyleColor& floodColor() const { return svgStyle().floodColor(); }
    const StyleColor& lightingColor() const { return svgStyle().lightingColor(); }

    Color effectiveAccentColor() const;
    const StyleColor& accentColor() const { return m_rareInheritedData->accentColor; }
    bool hasAutoAccentColor() const { return m_rareInheritedData->hasAutoAccentColor; }

    PathOperation* offsetPath() const { return m_rareNonInheritedData->offsetPath.get(); }
    void setOffsetPath(RefPtr<PathOperation>&& path) { SET_VAR(m_rareNonInheritedData, offsetPath, WTFMove(path)); }
    static PathOperation* initialOffsetPath() { return nullptr; }

    const Length& offsetDistance() const { return m_rareNonInheritedData->offsetDistance; }
    void setOffsetDistance(Length&& distance) { SET_VAR(m_rareNonInheritedData, offsetDistance, WTFMove(distance)); }
    static Length initialOffsetDistance() { return Length(0, LengthType::Fixed); }

    LengthPoint offsetPosition() const { return m_rareNonInheritedData->offsetPosition; }
    void setOffsetPosition(LengthPoint&& position) { SET_VAR(m_rareNonInheritedData, offsetPosition, WTFMove(position)); }
    static LengthPoint initialOffsetPosition() { return LengthPoint(Length(LengthType::Auto), Length(LengthType::Auto)); }

    LengthPoint offsetAnchor() const { return m_rareNonInheritedData->offsetAnchor; }
    void setOffsetAnchor(LengthPoint&& position) { SET_VAR(m_rareNonInheritedData, offsetAnchor, WTFMove(position)); }
    static LengthPoint initialOffsetAnchor() { return LengthPoint(Length(LengthType::Auto), Length(LengthType::Auto)); }

    OffsetRotation offsetRotate() const { return m_rareNonInheritedData->offsetRotate; }
    void setOffsetRotate(OffsetRotation&& rotation) { SET_VAR(m_rareNonInheritedData, offsetRotate, WTFMove(rotation)); }
    static OffsetRotation initialOffsetRotate() { return OffsetRotation(true, 0); }

    bool borderAndBackgroundEqual(const RenderStyle&) const;
    
    OverflowAnchor overflowAnchor() const { return static_cast<OverflowAnchor>(m_rareNonInheritedData->overflowAnchor); }
    void setOverflowAnchor(OverflowAnchor a) { SET_VAR(m_rareNonInheritedData, overflowAnchor, static_cast<unsigned>(a)); }
    static OverflowAnchor initialOverflowAnchor() { return OverflowAnchor::Auto; }

private:
    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const;
        bool operator!=(const NonInheritedFlags& other) const { return !(*this == other); }

        void copyNonInheritedFrom(const NonInheritedFlags&);

        bool hasAnyPublicPseudoStyles() const { return static_cast<unsigned>(PseudoId::PublicPseudoIdMask) & pseudoBits; }
        bool hasPseudoStyle(PseudoId) const;
        void setHasPseudoStyles(PseudoIdSet);

        unsigned effectiveDisplay : 5; // DisplayType
        unsigned originalDisplay : 5; // DisplayType
        unsigned overflowX : 3; // Overflow
        unsigned overflowY : 3; // Overflow
        unsigned verticalAlign : 4; // VerticalAlign
        unsigned clear : 3; // Clear
        unsigned position : 3; // PositionType
        unsigned unicodeBidi : 3; // UnicodeBidi
        unsigned floating : 3; // Float
        unsigned tableLayout : 1; // TableLayoutType

        unsigned hasExplicitlySetBorderBottomLeftRadius : 1;
        unsigned hasExplicitlySetBorderBottomRightRadius : 1;
        unsigned hasExplicitlySetBorderTopLeftRadius : 1;
        unsigned hasExplicitlySetBorderTopRightRadius : 1;
        unsigned hasExplicitlySetDirection : 1;
        unsigned hasExplicitlySetWritingMode : 1;
#if ENABLE(DARK_MODE_CSS)
        unsigned hasExplicitlySetColorScheme : 1;
#endif
        unsigned usesViewportUnits : 1;
        unsigned usesContainerUnits : 1;
        unsigned hasExplicitlyInheritedProperties : 1; // Explicitly inherits a non-inherited property.
        unsigned disallowsFastPathInheritance : 1;
        unsigned isUnique : 1; // Style cannot be shared.
        unsigned emptyState : 1;
        unsigned firstChildState : 1;
        unsigned lastChildState : 1;
        unsigned isLink : 1;
        unsigned hasContentNone : 1;

        unsigned styleType : 4; // PseudoId
        unsigned pseudoBits : (static_cast<unsigned>(PseudoId::FirstInternalPseudoId) - static_cast<unsigned>(PseudoId::FirstPublicPseudoId));

        // If you add more style bits here, you will also need to update RenderStyle::NonInheritedFlags::copyNonInheritedFrom().
    };

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const;
        bool operator!=(const InheritedFlags& other) const { return !(*this == other); }

        unsigned emptyCells : 1; // EmptyCell
        unsigned captionSide : 2; // CaptionSide
        unsigned listStyleType : 7; // ListStyleType
        unsigned listStylePosition : 1; // ListStylePosition
        unsigned visibility : 2; // Visibility
        unsigned textAlign : 4; // TextAlignMode
        unsigned textTransform : 2; // TextTransform
        unsigned textDecorationLines : TextDecorationLineBits;
        unsigned cursor : 6; // CursorType
#if ENABLE(CURSOR_VISIBILITY)
        unsigned cursorVisibility : 1; // CursorVisibility
#endif
        unsigned direction : 1; // TextDirection
        unsigned whiteSpace : 3; // WhiteSpace
        // 35 bits
        unsigned borderCollapse : 1; // BorderCollapse
        unsigned boxDirection : 1; // BoxDirection

        // non CSS2 inherited
        unsigned rtlOrdering : 1; // Order
        unsigned printColorAdjust : PrintColorAdjustBits; // PrintColorAdjust
        unsigned pointerEvents : 4; // PointerEvents
        unsigned insideLink : 2; // InsideLink
        unsigned insideDefaultButton : 1;
        // 46 bits

        // CSS Text Layout Module Level 3: Vertical writing support
        unsigned writingMode : 2; // WritingMode
        // 48 bits

#if ENABLE(TEXT_AUTOSIZING)
        unsigned autosizeStatus : 5;
#endif
        // 53 bits
    };

    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    DisplayType originalDisplay() const { return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay); }

    bool hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
    bool hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }

    static bool isDisplayInlineType(DisplayType);
    static bool isDisplayBlockType(DisplayType);
    static bool isDisplayFlexibleBox(DisplayType);
    static bool isDisplayGridBox(DisplayType);
    static bool isDisplayFlexibleOrGridBox(DisplayType);
    static bool isDisplayDeprecatedFlexibleBox(DisplayType);
    static bool isDisplayListItemType(DisplayType);
    static bool isDisplayTableOrTablePart(DisplayType);

    static LayoutBoxExtent shadowExtent(const ShadowData*);
    static LayoutBoxExtent shadowInsetExtent(const ShadowData*);
    static void getShadowHorizontalExtent(const ShadowData*, LayoutUnit& left, LayoutUnit& right);
    static void getShadowVerticalExtent(const ShadowData*, LayoutUnit& top, LayoutUnit& bottom);

    bool changeAffectsVisualOverflow(const RenderStyle&) const;
    bool changeRequiresLayout(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresPositionedLayoutOnly(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresLayerRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaintIfText(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRecompositeLayer(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;

    // non-inherited attributes
    DataRef<StyleBoxData> m_boxData;
    DataRef<StyleVisualData> m_visualData;
    DataRef<StyleBackgroundData> m_backgroundData;
    DataRef<StyleSurroundData> m_surroundData;
    DataRef<StyleRareNonInheritedData> m_rareNonInheritedData;
    NonInheritedFlags m_nonInheritedFlags;

    // inherited attributes
    DataRef<StyleRareInheritedData> m_rareInheritedData;
    DataRef<StyleInheritedData> m_inheritedData;
    InheritedFlags m_inheritedFlags;

    // list of associated pseudo styles
    std::unique_ptr<PseudoStyleCache> m_cachedPseudoStyles;

    DataRef<SVGRenderStyle> m_svgStyle;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_deletionHasBegun { false };
#endif
};

int adjustForAbsoluteZoom(int, const RenderStyle&);
float adjustFloatForAbsoluteZoom(float, const RenderStyle&);
LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderStyle&);
LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderStyle&);

BorderStyle collapsedBorderStyle(BorderStyle);

bool pseudoElementRendererIsNeeded(const RenderStyle*);

inline bool RenderStyle::NonInheritedFlags::operator==(const NonInheritedFlags& other) const
{
    return effectiveDisplay == other.effectiveDisplay
        && originalDisplay == other.originalDisplay
        && overflowX == other.overflowX
        && overflowY == other.overflowY
        && verticalAlign == other.verticalAlign
        && clear == other.clear
        && position == other.position
        && unicodeBidi == other.unicodeBidi
        && floating == other.floating
        && tableLayout == other.tableLayout
        && hasExplicitlySetBorderBottomLeftRadius == other.hasExplicitlySetBorderBottomLeftRadius
        && hasExplicitlySetBorderBottomRightRadius == other.hasExplicitlySetBorderBottomRightRadius
        && hasExplicitlySetBorderTopLeftRadius == other.hasExplicitlySetBorderTopLeftRadius
        && hasExplicitlySetBorderTopRightRadius == other.hasExplicitlySetBorderTopRightRadius
        && hasExplicitlySetDirection == other.hasExplicitlySetDirection
        && hasExplicitlySetWritingMode == other.hasExplicitlySetWritingMode
#if ENABLE(DARK_MODE_CSS)
        && hasExplicitlySetColorScheme == other.hasExplicitlySetColorScheme
#endif
        && usesViewportUnits == other.usesViewportUnits
        && usesContainerUnits == other.usesContainerUnits
        && hasExplicitlyInheritedProperties == other.hasExplicitlyInheritedProperties
        && disallowsFastPathInheritance == other.disallowsFastPathInheritance
        && isUnique == other.isUnique
        && emptyState == other.emptyState
        && firstChildState == other.firstChildState
        && lastChildState == other.lastChildState
        && isLink == other.isLink
        && hasContentNone == other.hasContentNone
        && styleType == other.styleType
        && pseudoBits == other.pseudoBits;
}

inline void RenderStyle::NonInheritedFlags::copyNonInheritedFrom(const NonInheritedFlags& other)
{
    // Only a subset is copied because NonInheritedFlags contains a bunch of stuff other than real style data.
    effectiveDisplay = other.effectiveDisplay;
    originalDisplay = other.originalDisplay;
    overflowX = other.overflowX;
    overflowY = other.overflowY;
    verticalAlign = other.verticalAlign;
    clear = other.clear;
    position = other.position;
    unicodeBidi = other.unicodeBidi;
    floating = other.floating;
    tableLayout = other.tableLayout;
    usesViewportUnits = other.usesViewportUnits;
    usesContainerUnits = other.usesContainerUnits;
    hasExplicitlyInheritedProperties = other.hasExplicitlyInheritedProperties;
    disallowsFastPathInheritance = other.disallowsFastPathInheritance;

    // Unlike properties tracked by the other hasExplicitlySet* flags, border-radius is non-inherited
    // and we need to remember whether it's been explicitly set when copying m_surroundData.
    hasExplicitlySetBorderBottomLeftRadius = other.hasExplicitlySetBorderBottomLeftRadius;
    hasExplicitlySetBorderBottomRightRadius = other.hasExplicitlySetBorderBottomRightRadius;
    hasExplicitlySetBorderTopLeftRadius = other.hasExplicitlySetBorderTopLeftRadius;
    hasExplicitlySetBorderTopRightRadius = other.hasExplicitlySetBorderTopRightRadius;
}

inline bool RenderStyle::NonInheritedFlags::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > PseudoId::None);
    ASSERT(pseudo < PseudoId::FirstInternalPseudoId);
    return pseudoBits & (1 << (static_cast<unsigned>(pseudo) - 1 /* PseudoId::None */));
}

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyles(PseudoIdSet pseudoIdSet)
{
    ASSERT(pseudoIdSet);
    ASSERT((pseudoIdSet.data() & static_cast<unsigned>(PseudoId::PublicPseudoIdMask)) == pseudoIdSet.data());
    pseudoBits |= pseudoIdSet.data() >> 1; // Shift down as we do not store a bit for PseudoId::None.
}

inline bool RenderStyle::InheritedFlags::operator==(const InheritedFlags& other) const
{
    return emptyCells == other.emptyCells
        && captionSide == other.captionSide
        && listStyleType == other.listStyleType
        && listStylePosition == other.listStylePosition
        && visibility == other.visibility
        && textAlign == other.textAlign
        && textTransform == other.textTransform
        && textDecorationLines == other.textDecorationLines
        && cursor == other.cursor
#if ENABLE(CURSOR_VISIBILITY)
        && cursorVisibility == other.cursorVisibility
#endif
        && direction == other.direction
        && whiteSpace == other.whiteSpace
        && borderCollapse == other.borderCollapse
        && boxDirection == other.boxDirection
        && rtlOrdering == other.rtlOrdering
        && printColorAdjust == other.printColorAdjust
        && pointerEvents == other.pointerEvents
        && insideLink == other.insideLink
        && insideDefaultButton == other.insideDefaultButton
        && writingMode == other.writingMode;
}

inline int adjustForAbsoluteZoom(int value, const RenderStyle& style)
{
    double zoomFactor = style.effectiveZoom();
    if (zoomFactor == 1)
        return value;
    // Needed because computeLengthInt truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1) {
        if (value < 0)
            value--;
        else 
            value++;
    }

    return roundForImpreciseConversion<int>(value / zoomFactor);
}

inline float adjustFloatForAbsoluteZoom(float value, const RenderStyle& style)
{
    return value / style.effectiveZoom();
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderStyle& style)
{
    return LayoutUnit(value / style.effectiveZoom());
}

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderStyle& style)
{
    auto zoom = style.effectiveZoom();
    return {
        size.width() / zoom,
        size.height() / zoom
    };
}

inline BorderStyle collapsedBorderStyle(BorderStyle style)
{
    if (style == BorderStyle::Outset)
        return BorderStyle::Groove;
    if (style == BorderStyle::Inset)
        return BorderStyle::Ridge;
    return style;
}

inline const CSSCustomPropertyValue* RenderStyle::getCustomProperty(const AtomString& name) const
{
    for (auto* map : { &nonInheritedCustomProperties(), &inheritedCustomProperties() }) {
        if (auto* val = map->get(name))
            return val;
    }
    return nullptr;
}

inline bool RenderStyle::hasBackground() const
{
    return visitedDependentColor(CSSPropertyBackgroundColor).isVisible() ||  hasBackgroundImage();
}

inline bool RenderStyle::autoWrap(WhiteSpace whiteSpace)
{
    // Nowrap and pre don't automatically wrap.
    return whiteSpace != WhiteSpace::NoWrap && whiteSpace != WhiteSpace::Pre;
}

inline bool RenderStyle::preserveNewline(WhiteSpace whiteSpace)
{
    // Normal and nowrap do not preserve newlines.
    return whiteSpace != WhiteSpace::Normal && whiteSpace != WhiteSpace::NoWrap;
}

inline bool RenderStyle::collapseWhiteSpace(WhiteSpace ws)
{
    // Pre and prewrap do not collapse whitespace.
    return ws != WhiteSpace::Pre && ws != WhiteSpace::PreWrap && ws != WhiteSpace::BreakSpaces;
}

inline bool RenderStyle::isCollapsibleWhiteSpace(UChar character) const
{
    switch (character) {
    case ' ':
    case '\t':
        return collapseWhiteSpace();
    case '\n':
        return !preserveNewline();
    default:
        return false;
    }
}

inline bool RenderStyle::breakOnlyAfterWhiteSpace() const
{
    return whiteSpace() == WhiteSpace::PreWrap || whiteSpace() == WhiteSpace::BreakSpaces || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool RenderStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::BreakWord || overflowWrap() == OverflowWrap::Anywhere;
}

inline bool RenderStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || isHorizontalWritingMode() == (axis == ColumnAxis::Horizontal);
}

inline ImageOrientation RenderStyle::imageOrientation() const
{
    return static_cast<ImageOrientation::Orientation>(m_rareInheritedData->imageOrientation);
}

#if ENABLE(CSS_COMPOSITING)
inline void RenderStyle::setBlendMode(BlendMode mode)
{
    SET_VAR(m_rareNonInheritedData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET_VAR(m_rareInheritedData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}
#endif

inline void RenderStyle::setLogicalWidth(Length&& logicalWidth)
{
    if (isHorizontalWritingMode())
        setWidth(WTFMove(logicalWidth));
    else
        setHeight(WTFMove(logicalWidth));
}

inline void RenderStyle::setLogicalHeight(Length&& logicalHeight)
{
    if (isHorizontalWritingMode())
        setHeight(WTFMove(logicalHeight));
    else
        setWidth(WTFMove(logicalHeight));
}

inline void RenderStyle::setBorderRadius(LengthSize&& size)
{
    auto topLeft = size;
    setBorderTopLeftRadius(WTFMove(topLeft));
    auto topRight = size;
    setBorderTopRightRadius(WTFMove(topRight));
    auto bottomLeft = size;
    setBorderBottomLeftRadius(WTFMove(bottomLeft));
    setBorderBottomRightRadius(WTFMove(size));
}

inline void RenderStyle::setBorderRadius(const IntSize& size)
{
    setBorderRadius(LengthSize { { size.width(), LengthType::Fixed }, { size.height(), LengthType::Fixed } });
}

inline bool RenderStyle::setZoom(float zoomLevel)
{
    setEffectiveZoom(effectiveZoom() * zoomLevel);
    if (compareEqual(m_visualData->zoom, zoomLevel))
        return false;
    m_visualData.access().zoom = zoomLevel;
    return true;
}

inline bool RenderStyle::setEffectiveZoom(float zoomLevel)
{
    if (compareEqual(m_rareInheritedData->effectiveZoom, zoomLevel))
        return false;
    m_rareInheritedData.access().effectiveZoom = zoomLevel;
    return true;
}

inline bool RenderStyle::setTextOrientation(TextOrientation textOrientation)
{
    if (compareEqual(static_cast<TextOrientation>(m_rareInheritedData->textOrientation), textOrientation))
        return false;
    m_rareInheritedData.access().textOrientation = static_cast<unsigned>(textOrientation);
    return true;
}

inline void RenderStyle::adjustBackgroundLayers()
{
    if (backgroundLayers().next()) {
        ensureBackgroundLayers().cullEmptyLayers();
        ensureBackgroundLayers().fillUnsetProperties();
    }
}

inline void RenderStyle::adjustMaskLayers()
{
    if (maskLayers().next()) {
        ensureMaskLayers().cullEmptyLayers();
        ensureMaskLayers().fillUnsetProperties();
    }
}

inline void RenderStyle::clearAnimations()
{
    m_rareNonInheritedData.access().animations = nullptr;
}

inline void RenderStyle::clearTransitions()
{
    m_rareNonInheritedData.access().transitions = nullptr;
}

inline void RenderStyle::setShapeOutside(RefPtr<ShapeValue>&& value)
{
    if (m_rareNonInheritedData->shapeOutside == value)
        return;
    m_rareNonInheritedData.access().shapeOutside = WTFMove(value);
}

inline void RenderStyle::setShapeImageThreshold(float shapeImageThreshold)
{
    float clampedShapeImageThreshold = clampTo<float>(shapeImageThreshold, 0.f, 1.f);
    SET_VAR(m_rareNonInheritedData, shapeImageThreshold, clampedShapeImageThreshold);
}

inline void RenderStyle::setClipPath(RefPtr<PathOperation>&& operation)
{
    if (m_rareNonInheritedData->clipPath != operation)
        m_rareNonInheritedData.access().clipPath = WTFMove(operation);
}

inline bool RenderStyle::willChangeCreatesStackingContext() const
{
    return willChange() && willChange()->canCreateStackingContext();
}

inline bool RenderStyle::isDisplayRegionType() const
{
    return display() == DisplayType::Block || display() == DisplayType::InlineBlock
        || display() == DisplayType::TableCell || display() == DisplayType::TableCaption
        || display() == DisplayType::ListItem;
}

inline bool RenderStyle::isDisplayBlockType(DisplayType display)
{
    return display == DisplayType::Block || display == DisplayType::Table
        || display == DisplayType::FlowRoot || display == DisplayType::Grid
        || display == DisplayType::Flex || display == DisplayType::Box
        || display == DisplayType::ListItem;
}

inline bool RenderStyle::setWritingMode(WritingMode v)
{
    if (v == writingMode())
        return false;
    m_inheritedFlags.writingMode = static_cast<unsigned>(v);
    return true;
}

inline void RenderStyle::getShadowInlineDirectionExtent(const ShadowData* shadow, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const
{
    return isHorizontalWritingMode() ? getShadowHorizontalExtent(shadow, logicalLeft, logicalRight) : getShadowVerticalExtent(shadow, logicalLeft, logicalRight);
}

inline void RenderStyle::getShadowBlockDirectionExtent(const ShadowData* shadow, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const
{
    return isHorizontalWritingMode() ? getShadowVerticalExtent(shadow, logicalTop, logicalBottom) : getShadowHorizontalExtent(shadow, logicalTop, logicalBottom);
}

inline bool RenderStyle::isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::Inline
        || display == DisplayType::InlineBlock
        || display == DisplayType::InlineBox
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || display == DisplayType::InlineTable;
}

inline bool RenderStyle::isDisplayFlexibleBox(DisplayType display)
{
    return display == DisplayType::Flex || display == DisplayType::InlineFlex;
}

inline bool RenderStyle::isDisplayGridBox(DisplayType display)
{
    return display == DisplayType::Grid || display == DisplayType::InlineGrid;
}

inline bool RenderStyle::isDisplayFlexibleOrGridBox(DisplayType display)
{
    return isDisplayFlexibleBox(display) || isDisplayGridBox(display);
}

inline bool RenderStyle::isDisplayDeprecatedFlexibleBox(DisplayType display)
{
    return display == DisplayType::Box || display == DisplayType::InlineBox;
}

inline bool RenderStyle::isDisplayListItemType(DisplayType display)
{
    return display == DisplayType::ListItem;
}

inline bool RenderStyle::isDisplayTableOrTablePart(DisplayType display)
{
    return display == DisplayType::Table || display == DisplayType::InlineTable || display == DisplayType::TableCell
        || display == DisplayType::TableCaption || display == DisplayType::TableRowGroup || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup || display == DisplayType::TableRow || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

inline bool RenderStyle::hasAnyPublicPseudoStyles() const
{
    return m_nonInheritedFlags.hasAnyPublicPseudoStyles();
}

inline bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    return m_nonInheritedFlags.hasPseudoStyle(pseudo);
}

inline void RenderStyle::setHasPseudoStyles(PseudoIdSet pseudoIdSet)
{
    m_nonInheritedFlags.setHasPseudoStyles(pseudoIdSet);
}

inline void RenderStyle::setBoxReflect(RefPtr<StyleReflection>&& reflect)
{
    SET_VAR(m_rareNonInheritedData, boxReflect, WTFMove(reflect));
}

inline bool pseudoElementRendererIsNeeded(const RenderStyle* style)
{
    return style && style->display() != DisplayType::None && style->contentData();
}

inline bool generatesBox(const RenderStyle& style)
{
    return style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

inline bool isNonVisibleOverflow(Overflow overflow)
{
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll || overflow == Overflow::Clip;
}

} // namespace WebCore

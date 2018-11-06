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
#include "BorderValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "Color.h"
#include "CounterDirectives.h"
#include "DataRef.h"
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
#include "Pagination.h"
#include "RenderStyleConstants.h"
#include "RoundedRect.h"
#include "SVGRenderStyle.h"
#include "ShadowData.h"
#include "ShapeValue.h"
#include "StyleBackgroundData.h"
#include "StyleBoxData.h"
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
#include "TransformOperations.h"
#include "UnicodeBidi.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#include "StyleGridData.h"
#include "StyleGridItemData.h"

#if ENABLE(DASHBOARD_SUPPORT)
#include "StyleDashboardRegion.h"
#endif

#if ENABLE(TEXT_AUTOSIZING)
#include "TextSizeAdjustment.h"
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
class StyleResolver;
class StyleScrollSnapArea;
class StyleScrollSnapPort;
class TransformationMatrix;

struct ScrollSnapAlign;
struct ScrollSnapType;

using PseudoStyleCache = Vector<std::unique_ptr<RenderStyle>, 4>;

template<typename T, typename U> inline bool compareEqual(const T& t, const U& u) { return t == static_cast<const T&>(u); }

class RenderStyle {
    WTF_MAKE_FAST_ALLOCATED;

private:
    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

public:
    RenderStyle(RenderStyle&&);
    RenderStyle& operator=(RenderStyle&&);
    ~RenderStyle();

    RenderStyle replace(RenderStyle&&) WARN_UNUSED_RETURN;

    explicit RenderStyle(CreateDefaultStyleTag);
    RenderStyle(const RenderStyle&, CloneTag);

    static RenderStyle& defaultStyle();

    static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();

    static RenderStyle clone(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool operator==(const RenderStyle&) const;
    bool operator!=(const RenderStyle& other) const { return !(*this == other); }

    void inheritFrom(const RenderStyle& inheritParent);
    void copyNonInheritedFrom(const RenderStyle&);
    void copyContentFrom(const RenderStyle&);

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
    void removeCachedPseudoStyle(PseudoId);

    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    const CustomPropertyValueMap& inheritedCustomProperties() const { return m_rareInheritedData->customProperties->values; }
    const CustomPropertyValueMap& nonInheritedCustomProperties() const { return m_rareNonInheritedData->customProperties->values; }
    const CSSCustomPropertyValue* getCustomProperty(const AtomicString&) const;
    void setInheritedCustomPropertyValue(const AtomicString& name, Ref<CSSCustomPropertyValue>&& value) { return m_rareInheritedData.access().customProperties.access().setCustomPropertyValue(name, WTFMove(value)); }
    void setNonInheritedCustomPropertyValue(const AtomicString& name, Ref<CSSCustomPropertyValue>&& value) { return m_rareNonInheritedData.access().customProperties.access().setCustomPropertyValue(name, WTFMove(value)); }

    void setHasViewportUnits(bool v = true) { m_nonInheritedFlags.hasViewportUnits = v; }
    bool hasViewportUnits() const { return m_nonInheritedFlags.hasViewportUnits; }

    bool affectedByHover() const { return m_nonInheritedFlags.affectedByHover; }
    bool affectedByActive() const { return m_nonInheritedFlags.affectedByActive; }
    bool affectedByDrag() const { return m_nonInheritedFlags.affectedByDrag; }

    void setAffectedByHover() { m_nonInheritedFlags.affectedByHover = true; }
    void setAffectedByActive() { m_nonInheritedFlags.affectedByActive = true; }
    void setAffectedByDrag() { m_nonInheritedFlags.affectedByDrag = true; }

    void setColumnStylesFromPaginationMode(const Pagination::Mode&);
    
    bool isFloating() const { return static_cast<Float>(m_nonInheritedFlags.floating) != Float::No; }
    bool hasMargin() const { return !m_surroundData->margin.isZero(); }
    bool hasBorder() const { return m_surroundData->border.hasBorder(); }
    bool hasBorderFill() const { return m_surroundData->border.hasFill(); }
    bool hasVisibleBorderDecoration() const { return hasVisibleBorder() || hasBorderFill(); }
    bool hasVisibleBorder() const { return m_surroundData->border.hasVisibleBorder(); }
    bool hasPadding() const { return !m_surroundData->padding.isZero(); }
    bool hasOffset() const { return !m_surroundData->offset.isZero(); }
    bool hasMarginBeforeQuirk() const { return marginBefore().hasQuirk(); }
    bool hasMarginAfterQuirk() const { return marginAfter().hasQuirk(); }

    bool hasBackgroundImage() const { return m_backgroundData->background.hasImage(); }
    bool hasFixedBackgroundImage() const { return m_backgroundData->background.hasFixedImage(); }

    bool hasEntirelyFixedBackground() const;

    bool hasAppearance() const { return appearance() != NoControlPart; }

    bool hasBackground() const;
    
    LayoutBoxExtent imageOutsets(const NinePieceImage&) const;
    bool hasBorderImageOutsets() const { return borderImage().hasImage() && !borderImage().outset().isZero(); }
    LayoutBoxExtent borderImageOutsets() const { return imageOutsets(borderImage()); }

    LayoutBoxExtent maskBoxImageOutsets() const { return imageOutsets(maskBoxImage()); }

    bool hasFilterOutsets() const { return hasFilter() && filter().hasOutsets(); }
    FilterOutsets filterOutsets() const { return hasFilter() ? filter().outsets() : FilterOutsets(); }

    Order rtlOrdering() const { return static_cast<Order>(m_inheritedFlags.rtlOrdering); }
    void setRTLOrdering(Order ordering) { m_inheritedFlags.rtlOrdering = static_cast<unsigned>(ordering); }

    bool isStyleAvailable() const;

    bool hasAnyPublicPseudoStyles() const;
    bool hasPseudoStyle(PseudoId) const;
    void setHasPseudoStyle(PseudoId);
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

    const LengthSize& borderTopLeftRadius() const { return m_surroundData->border.topLeft(); }
    const LengthSize& borderTopRightRadius() const { return m_surroundData->border.topRight(); }
    const LengthSize& borderBottomLeftRadius() const { return m_surroundData->border.bottomLeft(); }
    const LengthSize& borderBottomRightRadius() const { return m_surroundData->border.bottomRight(); }
    bool hasBorderRadius() const { return m_surroundData->border.hasBorderRadius(); }

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
    Overflow overflowInlineDirection() const { return isHorizontalWritingMode() ? overflowX() : overflowY(); }
    Overflow overflowBlockDirection() const { return isHorizontalWritingMode() ? overflowY() : overflowX(); }
    bool isOverflowVisible() const { return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible; }
    
    Visibility visibility() const { return static_cast<Visibility>(m_inheritedFlags.visibility); }
    VerticalAlign verticalAlign() const { return static_cast<VerticalAlign>(m_nonInheritedFlags.verticalAlign); }
    const Length& verticalAlignLength() const { return m_boxData->verticalAlign(); }

    const Length& clipLeft() const { return m_visualData->clip.left(); }
    const Length& clipRight() const { return m_visualData->clip.right(); }
    const Length& clipTop() const { return m_visualData->clip.top(); }
    const Length& clipBottom() const { return m_visualData->clip.bottom(); }
    const LengthBox& clip() const { return m_visualData->clip; }
    bool hasClip() const { return m_visualData->hasClip; }

    EUnicodeBidi unicodeBidi() const { return static_cast<EUnicodeBidi>(m_nonInheritedFlags.unicodeBidi); }

    Clear clear() const { return static_cast<Clear>(m_nonInheritedFlags.clear); }
    TableLayoutType tableLayout() const { return static_cast<TableLayoutType>(m_nonInheritedFlags.tableLayout); }

    WEBCORE_EXPORT const FontCascade& fontCascade() const;
    WEBCORE_EXPORT const FontMetrics& fontMetrics() const;
    WEBCORE_EXPORT const FontCascadeDescription& fontDescription() const;
    float specifiedFontSize() const;
    float computedFontSize() const;
    unsigned computedFontPixelSize() const;
    std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();

#if ENABLE(VARIATION_FONTS)
    FontVariationSettings fontVariationSettings() const { return fontDescription().variationSettings(); }
#endif
    FontSelectionValue fontWeight() const { return fontDescription().weight(); }
    FontSelectionValue fontStretch() const { return fontDescription().stretch(); }
    std::optional<FontSelectionValue> fontItalic() const { return fontDescription().italic(); }

    const Length& textIndent() const { return m_rareInheritedData->indent; }
    TextAlignMode textAlign() const { return static_cast<TextAlignMode>(m_inheritedFlags.textAlign); }
    TextTransform textTransform() const { return static_cast<TextTransform>(m_inheritedFlags.textTransform); }
    OptionSet<TextDecoration> textDecorationsInEffect() const { return OptionSet<TextDecoration>::fromRaw(m_inheritedFlags.textDecorations); }
    OptionSet<TextDecoration> textDecoration() const { return OptionSet<TextDecoration>::fromRaw(m_visualData->textDecoration); }
    TextDecorationStyle textDecorationStyle() const { return static_cast<TextDecorationStyle>(m_rareNonInheritedData->textDecorationStyle); }
    OptionSet<TextDecorationSkip> textDecorationSkip() const { return OptionSet<TextDecorationSkip>::fromRaw(m_rareInheritedData->textDecorationSkip); }
    TextUnderlinePosition textUnderlinePosition() const { return static_cast<TextUnderlinePosition>(m_rareInheritedData->textUnderlinePosition); }
    TextUnderlineOffset textUnderlineOffset() const { return m_rareInheritedData->textUnderlineOffset; }
    TextDecorationThickness textDecorationThickness() const { return m_rareInheritedData->textDecorationThickness; }

#if ENABLE(CSS3_TEXT)
    TextIndentLine textIndentLine() const { return static_cast<TextIndentLine>(m_rareInheritedData->textIndentLine); }
    TextIndentType textIndentType() const { return static_cast<TextIndentType>(m_rareInheritedData->textIndentType); }
    TextAlignLast textAlignLast() const { return static_cast<TextAlignLast>(m_rareInheritedData->textAlignLast); }
    TextJustify textJustify() const { return static_cast<TextJustify>(m_rareInheritedData->textJustify); }
#endif

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

    FillRepeat backgroundRepeatX() const { return static_cast<FillRepeat>(m_backgroundData->background.repeatX()); }
    FillRepeat backgroundRepeatY() const { return static_cast<FillRepeat>(m_backgroundData->background.repeatY()); }
    CompositeOperator backgroundComposite() const { return static_cast<CompositeOperator>(m_backgroundData->background.composite()); }
    FillAttachment backgroundAttachment() const { return static_cast<FillAttachment>(m_backgroundData->background.attachment()); }
    FillBox backgroundClip() const { return static_cast<FillBox>(m_backgroundData->background.clip()); }
    FillBox backgroundOrigin() const { return static_cast<FillBox>(m_backgroundData->background.origin()); }
    const Length& backgroundXPosition() const { return m_backgroundData->background.xPosition(); }
    const Length& backgroundYPosition() const { return m_backgroundData->background.yPosition(); }
    FillSizeType backgroundSizeType() const { return m_backgroundData->background.sizeType(); }
    const LengthSize& backgroundSizeLength() const { return m_backgroundData->background.sizeLength(); }
    FillLayer& ensureBackgroundLayers() { return m_backgroundData.access().background; }
    const FillLayer& backgroundLayers() const { return m_backgroundData->background; }

    StyleImage* maskImage() const { return m_rareNonInheritedData->mask.image(); }
    FillRepeat maskRepeatX() const { return static_cast<FillRepeat>(m_rareNonInheritedData->mask.repeatX()); }
    FillRepeat maskRepeatY() const { return static_cast<FillRepeat>(m_rareNonInheritedData->mask.repeatY()); }
    CompositeOperator maskComposite() const { return static_cast<CompositeOperator>(m_rareNonInheritedData->mask.composite()); }
    FillBox maskClip() const { return static_cast<FillBox>(m_rareNonInheritedData->mask.clip()); }
    FillBox maskOrigin() const { return static_cast<FillBox>(m_rareNonInheritedData->mask.origin()); }
    const Length& maskXPosition() const { return m_rareNonInheritedData->mask.xPosition(); }
    const Length& maskYPosition() const { return m_rareNonInheritedData->mask.yPosition(); }
    FillSizeType maskSizeType() const { return m_rareNonInheritedData->mask.sizeType(); }
    const LengthSize& maskSizeLength() const { return m_rareNonInheritedData->mask.sizeLength(); }
    FillLayer& ensureMaskLayers() { return m_rareNonInheritedData.access().mask; }
    const FillLayer& maskLayers() const { return m_rareNonInheritedData->mask; }
    const NinePieceImage& maskBoxImage() const { return m_rareNonInheritedData->maskBoxImage; }
    StyleImage* maskBoxImageSource() const { return m_rareNonInheritedData->maskBoxImage.image(); }

    BorderCollapse borderCollapse() const { return static_cast<BorderCollapse>(m_inheritedFlags.borderCollapse); }
    float horizontalBorderSpacing() const;
    float verticalBorderSpacing() const;
    EmptyCell emptyCells() const { return static_cast<EmptyCell>(m_inheritedFlags.emptyCells); }
    CaptionSide captionSide() const { return static_cast<CaptionSide>(m_inheritedFlags.captionSide); }

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

    short widows() const { return m_rareInheritedData->widows; }
    short orphans() const { return m_rareInheritedData->orphans; }
    bool hasAutoWidows() const { return m_rareInheritedData->hasAutoWidows; }
    bool hasAutoOrphans() const { return m_rareInheritedData->hasAutoOrphans; }

    BreakInside breakInside() const { return static_cast<BreakInside>(m_rareNonInheritedData->breakInside); }
    BreakBetween breakBefore() const { return static_cast<BreakBetween>(m_rareNonInheritedData->breakBefore); }
    BreakBetween breakAfter() const { return static_cast<BreakBetween>(m_rareNonInheritedData->breakAfter); }

    OptionSet<HangingPunctuation> hangingPunctuation() const { return OptionSet<HangingPunctuation>::fromRaw(m_rareInheritedData->hangingPunctuation); }

    float outlineOffset() const;
    const ShadowData* textShadow() const { return m_rareInheritedData->textShadow.get(); }
    void getTextShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const { getShadowInlineDirectionExtent(textShadow(), logicalLeft, logicalRight); }
    void getTextShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const { getShadowBlockDirectionExtent(textShadow(), logicalTop, logicalBottom); }

    float textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
    float opacity() const { return m_rareNonInheritedData->opacity; }
    ControlPart appearance() const { return static_cast<ControlPart>(m_rareNonInheritedData->appearance); }
    AspectRatioType aspectRatioType() const { return static_cast<AspectRatioType>(m_rareNonInheritedData->aspectRatioType); }
    float aspectRatioDenominator() const { return m_rareNonInheritedData->aspectRatioDenominator; }
    float aspectRatioNumerator() const { return m_rareNonInheritedData->aspectRatioNumerator; }
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

    const Vector<GridTrackSize>& gridColumns() const { return m_rareNonInheritedData->grid->gridColumns; }
    const Vector<GridTrackSize>& gridRows() const { return m_rareNonInheritedData->grid->gridRows; }
    const Vector<GridTrackSize>& gridAutoRepeatColumns() const { return m_rareNonInheritedData->grid->gridAutoRepeatColumns; }
    const Vector<GridTrackSize>& gridAutoRepeatRows() const { return m_rareNonInheritedData->grid->gridAutoRepeatRows; }
    unsigned gridAutoRepeatColumnsInsertionPoint() const { return m_rareNonInheritedData->grid->autoRepeatColumnsInsertionPoint; }
    unsigned gridAutoRepeatRowsInsertionPoint() const { return m_rareNonInheritedData->grid->autoRepeatRowsInsertionPoint; }
    AutoRepeatType gridAutoRepeatColumnsType() const  { return m_rareNonInheritedData->grid->autoRepeatColumnsType; }
    AutoRepeatType gridAutoRepeatRowsType() const  { return m_rareNonInheritedData->grid->autoRepeatRowsType; }
    const NamedGridLinesMap& namedGridColumnLines() const { return m_rareNonInheritedData->grid->namedGridColumnLines; }
    const NamedGridLinesMap& namedGridRowLines() const { return m_rareNonInheritedData->grid->namedGridRowLines; }
    const OrderedNamedGridLinesMap& orderedNamedGridColumnLines() const { return m_rareNonInheritedData->grid->orderedNamedGridColumnLines; }
    const OrderedNamedGridLinesMap& orderedNamedGridRowLines() const { return m_rareNonInheritedData->grid->orderedNamedGridRowLines; }
    const NamedGridLinesMap& autoRepeatNamedGridColumnLines() const { return m_rareNonInheritedData->grid->autoRepeatNamedGridColumnLines; }
    const NamedGridLinesMap& autoRepeatNamedGridRowLines() const { return m_rareNonInheritedData->grid->autoRepeatNamedGridRowLines; }
    const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridColumnLines() const { return m_rareNonInheritedData->grid->autoRepeatOrderedNamedGridColumnLines; }
    const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridRowLines() const { return m_rareNonInheritedData->grid->autoRepeatOrderedNamedGridRowLines; }
    const NamedGridAreaMap& namedGridArea() const { return m_rareNonInheritedData->grid->namedGridArea; }
    size_t namedGridAreaRowCount() const { return m_rareNonInheritedData->grid->namedGridAreaRowCount; }
    size_t namedGridAreaColumnCount() const { return m_rareNonInheritedData->grid->namedGridAreaColumnCount; }
    GridAutoFlow gridAutoFlow() const { return static_cast<GridAutoFlow>(m_rareNonInheritedData->grid->gridAutoFlow); }
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
    void getBoxShadowExtent(LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const { getShadowExtent(boxShadow(), top, right, bottom, left); }
    LayoutBoxExtent getBoxShadowInsetExtent() const { return getShadowInsetExtent(boxShadow()); }
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
    UserModify userModify() const { return static_cast<UserModify>(m_rareInheritedData->userModify); }
    UserDrag userDrag() const { return static_cast<UserDrag>(m_rareNonInheritedData->userDrag); }
    UserSelect userSelect() const { return static_cast<UserSelect>(m_rareInheritedData->userSelect); }
    TextOverflow textOverflow() const { return static_cast<TextOverflow>(m_rareNonInheritedData->textOverflow); }
    MarginCollapse marginBeforeCollapse() const { return static_cast<MarginCollapse>(m_rareNonInheritedData->marginBeforeCollapse); }
    MarginCollapse marginAfterCollapse() const { return static_cast<MarginCollapse>(m_rareNonInheritedData->marginAfterCollapse); }
    WordBreak wordBreak() const { return static_cast<WordBreak>(m_rareInheritedData->wordBreak); }
    OverflowWrap overflowWrap() const { return static_cast<OverflowWrap>(m_rareInheritedData->overflowWrap); }
    NBSPMode nbspMode() const { return static_cast<NBSPMode>(m_rareInheritedData->nbspMode); }
    LineBreak lineBreak() const { return static_cast<LineBreak>(m_rareInheritedData->lineBreak); }
    Hyphens hyphens() const { return static_cast<Hyphens>(m_rareInheritedData->hyphens); }
    short hyphenationLimitBefore() const { return m_rareInheritedData->hyphenationLimitBefore; }
    short hyphenationLimitAfter() const { return m_rareInheritedData->hyphenationLimitAfter; }
    short hyphenationLimitLines() const { return m_rareInheritedData->hyphenationLimitLines; }
    const AtomicString& hyphenationString() const { return m_rareInheritedData->hyphenationString; }
    const AtomicString& locale() const { return fontDescription().locale(); }
    BorderFit borderFit() const { return static_cast<BorderFit>(m_rareNonInheritedData->borderFit); }
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
    bool hasTransform() const { return !m_rareNonInheritedData->transform->operations.operations().isEmpty(); }
    const Length& transformOriginX() const { return m_rareNonInheritedData->transform->x; }
    const Length& transformOriginY() const { return m_rareNonInheritedData->transform->y; }
    float transformOriginZ() const { return m_rareNonInheritedData->transform->z; }
    TransformBox transformBox() const { return m_rareNonInheritedData->transform->transformBox; }

    TextEmphasisFill textEmphasisFill() const { return static_cast<TextEmphasisFill>(m_rareInheritedData->textEmphasisFill); }
    TextEmphasisMark textEmphasisMark() const;
    const AtomicString& textEmphasisCustomMark() const { return m_rareInheritedData->textEmphasisCustomMark; }
    OptionSet<TextEmphasisPosition> textEmphasisPosition() const { return OptionSet<TextEmphasisPosition>::fromRaw(m_rareInheritedData->textEmphasisPosition); }
    const AtomicString& textEmphasisMarkString() const;

    RubyPosition rubyPosition() const { return static_cast<RubyPosition>(m_rareInheritedData->rubyPosition); }

    TextOrientation textOrientation() const { return static_cast<TextOrientation>(m_rareInheritedData->textOrientation); }

    ObjectFit objectFit() const { return static_cast<ObjectFit>(m_rareNonInheritedData->objectFit); }
    LengthPoint objectPosition() const { return m_rareNonInheritedData->objectPosition; }

    // Return true if any transform related property (currently transform, transformStyle3D or perspective)
    // indicates that we are transforming.
    bool hasTransformRelatedProperty() const { return hasTransform() || preserves3D() || hasPerspective(); }

    enum ApplyTransformOrigin { IncludeTransformOrigin, ExcludeTransformOrigin };
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, ApplyTransformOrigin = IncludeTransformOrigin) const;
    void setPageScaleTransform(float);

    bool hasMask() const { return m_rareNonInheritedData->mask.hasImage() || m_rareNonInheritedData->maskBoxImage.hasImage(); }

    TextCombine textCombine() const { return static_cast<TextCombine>(m_rareNonInheritedData->textCombine); }
    bool hasTextCombine() const { return textCombine() != TextCombine::None; }

    unsigned tabSize() const { return m_rareInheritedData->tabSize; }

    // End CSS3 Getters

    const AtomicString& lineGrid() const { return m_rareInheritedData->lineGrid; }
    LineSnap lineSnap() const { return static_cast<LineSnap>(m_rareInheritedData->lineSnap); }
    LineAlign lineAlign() const { return static_cast<LineAlign>(m_rareInheritedData->lineAlign); }

    PointerEvents pointerEvents() const { return static_cast<PointerEvents>(m_inheritedFlags.pointerEvents); }
    const AnimationList* animations() const { return m_rareNonInheritedData->animations.get(); }
    const AnimationList* transitions() const { return m_rareNonInheritedData->transitions.get(); }

    AnimationList* animations() { return m_rareNonInheritedData->animations.get(); }
    AnimationList* transitions() { return m_rareNonInheritedData->transitions.get(); }
    
    bool hasAnimationsOrTransitions() const { return hasAnimations() || hasTransitions(); }

    AnimationList& ensureAnimations();
    AnimationList& ensureTransitions();

    bool hasAnimations() const { return m_rareNonInheritedData->animations && m_rareNonInheritedData->animations->size() > 0; }
    bool hasTransitions() const { return m_rareNonInheritedData->transitions && m_rareNonInheritedData->transitions->size() > 0; }

    // Return the first found Animation (including 'all' transitions).
    const Animation* transitionForProperty(CSSPropertyID) const;

    TransformStyle3D transformStyle3D() const { return static_cast<TransformStyle3D>(m_rareNonInheritedData->transformStyle3D); }
    bool preserves3D() const { return transformStyle3D() == TransformStyle3D::Preserve3D; }

    BackfaceVisibility backfaceVisibility() const { return static_cast<BackfaceVisibility>(m_rareNonInheritedData->backfaceVisibility); }
    float perspective() const { return m_rareNonInheritedData->perspective; }
    bool hasPerspective() const { return m_rareNonInheritedData->perspective > 0; }
    const Length& perspectiveOriginX() const { return m_rareNonInheritedData->perspectiveOriginX; }
    const Length& perspectiveOriginY() const { return m_rareNonInheritedData->perspectiveOriginY; }
    const LengthSize& pageSize() const { return m_rareNonInheritedData->pageSize; }
    PageSizeType pageSizeType() const { return static_cast<PageSizeType>(m_rareNonInheritedData->pageSizeType); }
    
    LineBoxContain lineBoxContain() const { return m_rareInheritedData->lineBoxContain; }
    const LineClampValue& lineClamp() const { return m_rareNonInheritedData->lineClamp; }
    const IntSize& initialLetter() const { return m_rareNonInheritedData->initialLetter; }
    int initialLetterDrop() const { return initialLetter().width(); }
    int initialLetterHeight() const { return initialLetter().height(); }

#if ENABLE(TOUCH_EVENTS)
    TouchAction touchAction() const { return static_cast<TouchAction>(m_rareNonInheritedData->touchAction); }
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    // Scroll snap port style.
    const StyleScrollSnapPort& scrollSnapPort() const;
    const ScrollSnapType& scrollSnapType() const;
    const LengthBox& scrollPadding() const;
    const Length& scrollPaddingTop() const;
    const Length& scrollPaddingBottom() const;
    const Length& scrollPaddingLeft() const;
    const Length& scrollPaddingRight() const;

    // Scroll snap area style.
    const StyleScrollSnapArea& scrollSnapArea() const;
    const ScrollSnapAlign& scrollSnapAlign() const;
    const LengthBox& scrollSnapMargin() const;
    const Length& scrollSnapMarginTop() const;
    const Length& scrollSnapMarginBottom() const;
    const Length& scrollSnapMarginLeft() const;
    const Length& scrollSnapMarginRight() const;
#endif

#if ENABLE(TOUCH_EVENTS)
    Color tapHighlightColor() const { return m_rareInheritedData->tapHighlightColor; }
#endif

#if PLATFORM(IOS_FAMILY)
    bool touchCalloutEnabled() const { return m_rareInheritedData->touchCalloutEnabled; }
#endif

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    bool useTouchOverflowScrolling() const { return m_rareInheritedData->useTouchOverflowScrolling; }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjustment textSizeAdjust() const { return m_rareInheritedData->textSizeAdjust; }
#endif

    TextSecurity textSecurity() const { return static_cast<TextSecurity>(m_rareInheritedData->textSecurity); }

    WritingMode writingMode() const { return static_cast<WritingMode>(m_inheritedFlags.writingMode); }
    bool isHorizontalWritingMode() const { return WebCore::isHorizontalWritingMode(writingMode()); }
    bool isVerticalWritingMode() const { return WebCore::isVerticalWritingMode(writingMode()); }
    bool isFlippedLinesWritingMode() const { return WebCore::isFlippedLinesWritingMode(writingMode()); }
    bool isFlippedBlocksWritingMode() const { return WebCore::isFlippedWritingMode(writingMode()); }

    ImageOrientationEnum imageOrientation() const;

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
    void setBlendMode(BlendMode mode) { SET_VAR(m_rareNonInheritedData, effectiveBlendMode, static_cast<unsigned>(mode)); }
    bool hasBlendMode() const { return static_cast<BlendMode>(m_rareNonInheritedData->effectiveBlendMode) != BlendMode::Normal; }

    Isolation isolation() const { return static_cast<Isolation>(m_rareNonInheritedData->isolation); }
    void setIsolation(Isolation isolation) { SET_VAR(m_rareNonInheritedData, isolation, static_cast<unsigned>(isolation)); }
    bool hasIsolation() const { return isolation() != Isolation::Auto; }
#else
    BlendMode blendMode() const { return BlendMode::Normal; }
    bool hasBlendMode() const { return false; }

    Isolation isolation() const { return Isolation::Auto; }
    bool hasIsolation() const { return false; }
#endif

    bool shouldPlaceBlockDirectionScrollbarOnLeft() const;

#if ENABLE(CSS_TRAILING_WORD)
    TrailingWord trailingWord() const { return static_cast<TrailingWord>(m_rareInheritedData->trailingWord); }
#endif

#if ENABLE(APPLE_PAY)
    ApplePayButtonStyle applePayButtonStyle() const { return static_cast<ApplePayButtonStyle>(m_rareNonInheritedData->applePayButtonStyle); }
    ApplePayButtonType applePayButtonType() const { return static_cast<ApplePayButtonType>(m_rareNonInheritedData->applePayButtonType); }
#endif

// attribute setter methods

    void setDisplay(DisplayType v) { m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(v); }
    void setOriginalDisplay(DisplayType v) { m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(v); }
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

#if ENABLE(DASHBOARD_SUPPORT)
    const Vector<StyleDashboardRegion>& dashboardRegions() const { return m_rareNonInheritedData->dashboardRegions; }
    void setDashboardRegions(const Vector<StyleDashboardRegion>& regions) { SET_VAR(m_rareNonInheritedData, dashboardRegions, regions); }
    void setDashboardRegion(int type, const String& label, Length&& top, Length&& right, Length&& bottom, Length&& left, bool append);
#endif

    void resetBorder() { resetBorderImage(); resetBorderTop(); resetBorderRight(); resetBorderBottom(); resetBorderLeft(); resetBorderRadius(); }
    void resetBorderTop() { SET_VAR(m_surroundData, border.m_top, BorderValue()); }
    void resetBorderRight() { SET_VAR(m_surroundData, border.m_right, BorderValue()); }
    void resetBorderBottom() { SET_VAR(m_surroundData, border.m_bottom, BorderValue()); }
    void resetBorderLeft() { SET_VAR(m_surroundData, border.m_left, BorderValue()); }
    void resetBorderImage() { SET_VAR(m_surroundData, border.m_image, NinePieceImage()); }
    void resetBorderRadius() { resetBorderTopLeftRadius(); resetBorderTopRightRadius(); resetBorderBottomLeftRadius(); resetBorderBottomRightRadius(); }
    void resetBorderTopLeftRadius() { SET_VAR(m_surroundData, border.m_topLeft, initialBorderRadius()); }
    void resetBorderTopRightRadius() { SET_VAR(m_surroundData, border.m_topRight, initialBorderRadius()); }
    void resetBorderBottomLeftRadius() { SET_VAR(m_surroundData, border.m_bottomLeft, initialBorderRadius()); }
    void resetBorderBottomRightRadius() { SET_VAR(m_surroundData, border.m_bottomRight, initialBorderRadius()); }

    void setBackgroundColor(const Color& v) { SET_VAR(m_backgroundData, color, v); }

    void setBackgroundXPosition(Length&& length) { SET_VAR(m_backgroundData, background.m_xPosition, WTFMove(length)); }
    void setBackgroundYPosition(Length&& length) { SET_VAR(m_backgroundData, background.m_yPosition, WTFMove(length)); }
    void setBackgroundSize(FillSizeType b) { SET_VAR(m_backgroundData, background.m_sizeType, static_cast<unsigned>(b)); }
    void setBackgroundSizeLength(LengthSize&& size) { SET_VAR(m_backgroundData, background.m_sizeLength, WTFMove(size)); }
    
    void setBorderImage(const NinePieceImage& b) { SET_VAR(m_surroundData, border.m_image, b); }
    void setBorderImageSource(RefPtr<StyleImage>&&);
    void setBorderImageSlices(LengthBox&&);
    void setBorderImageWidth(LengthBox&&);
    void setBorderImageOutset(LengthBox&&);

    void setBorderTopLeftRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_topLeft, WTFMove(size)); }
    void setBorderTopRightRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_topRight, WTFMove(size)); }
    void setBorderBottomLeftRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_bottomLeft, WTFMove(size)); }
    void setBorderBottomRightRadius(LengthSize&& size) { SET_VAR(m_surroundData, border.m_bottomRight, WTFMove(size)); }

    void setBorderRadius(LengthSize&&);
    void setBorderRadius(const IntSize&);

    RoundedRect getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth,
        LayoutUnit leftWidth, LayoutUnit rightWidth, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    void setBorderLeftWidth(float v) { SET_VAR(m_surroundData, border.m_left.m_width, v); }
    void setBorderLeftStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_left.m_style, static_cast<unsigned>(v)); }
    void setBorderLeftColor(const Color& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_left, v); }
    void setBorderRightWidth(float v) { SET_VAR(m_surroundData, border.m_right.m_width, v); }
    void setBorderRightStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_right.m_style, static_cast<unsigned>(v)); }
    void setBorderRightColor(const Color& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_right, v); }
    void setBorderTopWidth(float v) { SET_VAR(m_surroundData, border.m_top.m_width, v); }
    void setBorderTopStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_top.m_style, static_cast<unsigned>(v)); }
    void setBorderTopColor(const Color& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_top, v); }
    void setBorderBottomWidth(float v) { SET_VAR(m_surroundData, border.m_bottom.m_width, v); }
    void setBorderBottomStyle(BorderStyle v) { SET_VAR(m_surroundData, border.m_bottom.m_style, static_cast<unsigned>(v)); }
    void setBorderBottomColor(const Color& v) { SET_BORDERVALUE_COLOR(m_surroundData, border.m_bottom, v); }

    void setOutlineWidth(float v) { SET_VAR(m_backgroundData, outline.m_width, v); }
    void setOutlineStyleIsAuto(OutlineIsAuto isAuto) { SET_VAR(m_backgroundData, outline.m_isAuto, static_cast<unsigned>(isAuto)); }
    void setOutlineStyle(BorderStyle v) { SET_VAR(m_backgroundData, outline.m_style, static_cast<unsigned>(v)); }
    void setOutlineColor(const Color& v) { SET_BORDERVALUE_COLOR(m_backgroundData, outline, v); }

    void setOverflowX(Overflow v) { m_nonInheritedFlags.overflowX =  static_cast<unsigned>(v); }
    void setOverflowY(Overflow v) { m_nonInheritedFlags.overflowY = static_cast<unsigned>(v); }
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

    void setUnicodeBidi(EUnicodeBidi v) { m_nonInheritedFlags.unicodeBidi = v; }

    void setClear(Clear v) { m_nonInheritedFlags.clear = static_cast<unsigned>(v); }
    void setTableLayout(TableLayoutType v) { m_nonInheritedFlags.tableLayout = static_cast<unsigned>(v); }

    bool setFontDescription(FontCascadeDescription&&);

    // Only used for blending font sizes when animating, for MathML anonymous blocks, and for text autosizing.
    void setFontSize(float);

#if ENABLE(VARIATION_FONTS)
    void setFontVariationSettings(FontVariationSettings);
#endif
    void setFontWeight(FontSelectionValue);
    void setFontStretch(FontSelectionValue);
    void setFontItalic(std::optional<FontSelectionValue>);

    void setColor(const Color&);
    void setTextIndent(Length&& length) { SET_VAR(m_rareInheritedData, indent, WTFMove(length)); }
    void setTextAlign(TextAlignMode v) { m_inheritedFlags.textAlign = static_cast<unsigned>(v); }
    void setTextTransform(TextTransform v) { m_inheritedFlags.textTransform = static_cast<unsigned>(v); }
    void addToTextDecorationsInEffect(OptionSet<TextDecoration> v) { m_inheritedFlags.textDecorations |= static_cast<unsigned>(v.toRaw()); }
    void setTextDecorationsInEffect(OptionSet<TextDecoration> v) { m_inheritedFlags.textDecorations = v.toRaw(); }
    void setTextDecoration(OptionSet<TextDecoration> v) { SET_VAR(m_visualData, textDecoration, v.toRaw()); }
    void setTextDecorationStyle(TextDecorationStyle v) { SET_VAR(m_rareNonInheritedData, textDecorationStyle, static_cast<unsigned>(v)); }
    void setTextDecorationSkip(OptionSet<TextDecorationSkip> skip) { SET_VAR(m_rareInheritedData, textDecorationSkip, skip.toRaw()); }
    void setTextUnderlinePosition(TextUnderlinePosition position) { SET_VAR(m_rareInheritedData, textUnderlinePosition, static_cast<unsigned>(position)); }
    void setTextUnderlineOffset(TextUnderlineOffset textUnderlineOffset) { SET_VAR(m_rareInheritedData, textUnderlineOffset, textUnderlineOffset); }
    void setTextDecorationThickness(TextDecorationThickness textDecorationThickness) { SET_VAR(m_rareInheritedData, textDecorationThickness, textDecorationThickness); }
    void setDirection(TextDirection v) { m_inheritedFlags.direction = static_cast<unsigned>(v); }
    void setHasExplicitlySetDirection(bool v) { m_nonInheritedFlags.hasExplicitlySetDirection = v; }
    void setLineHeight(Length&&);
    bool setZoom(float);
    void setZoomWithoutReturnValue(float f) { setZoom(f); }
    bool setEffectiveZoom(float);
    void setTextZoom(TextZoom v) { SET_VAR(m_rareInheritedData, textZoom, static_cast<unsigned>(v)); }

#if ENABLE(CSS3_TEXT)
    void setTextIndentLine(TextIndentLine v) { SET_VAR(m_rareInheritedData, textIndentLine, v); }
    void setTextIndentType(TextIndentType v) { SET_VAR(m_rareInheritedData, textIndentType, v); }
    void setTextAlignLast(TextAlignLast v) { SET_VAR(m_rareInheritedData, textAlignLast, v); }
    void setTextJustify(TextJustify v) { SET_VAR(m_rareInheritedData, textJustify, v); }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(Length&&);
#endif

#if ENABLE(CSS_IMAGE_ORIENTATION)
    void setImageOrientation(ImageOrientationEnum v) { SET_VAR(m_rareInheritedData, imageOrientation, static_cast<int>(v)); }
#endif

    void setImageRendering(ImageRendering v) { SET_VAR(m_rareInheritedData, imageRendering, static_cast<unsigned>(v)); }

#if ENABLE(CSS_IMAGE_RESOLUTION)
    void setImageResolutionSource(ImageResolutionSource v) { SET_VAR(m_rareInheritedData, imageResolutionSource, v); }
    void setImageResolutionSnap(ImageResolutionSnap v) { SET_VAR(m_rareInheritedData, imageResolutionSnap, v); }
    void setImageResolution(float f) { SET_VAR(m_rareInheritedData, imageResolution, f); }
#endif

    void setWhiteSpace(WhiteSpace v) { m_inheritedFlags.whiteSpace = static_cast<unsigned>(v); }

    void setWordSpacing(Length&&);
    void setLetterSpacing(float);

    void clearBackgroundLayers() { m_backgroundData.access().background = FillLayer(FillLayerType::Background); }
    void inheritBackgroundLayers(const FillLayer& parent) { m_backgroundData.access().background = parent; }

    void adjustBackgroundLayers();

    void clearMaskLayers() { m_rareNonInheritedData.access().mask = FillLayer(FillLayerType::Mask); }
    void inheritMaskLayers(const FillLayer& parent) { m_rareNonInheritedData.access().mask = parent; }

    void adjustMaskLayers();

    void setMaskImage(RefPtr<StyleImage>&& v) { m_rareNonInheritedData.access().mask.setImage(WTFMove(v)); }

    void setMaskBoxImage(const NinePieceImage& b) { SET_VAR(m_rareNonInheritedData, maskBoxImage, b); }
    void setMaskBoxImageSource(RefPtr<StyleImage>&& v) { m_rareNonInheritedData.access().maskBoxImage.setImage(WTFMove(v)); }
    void setMaskXPosition(Length&& length) { SET_VAR(m_rareNonInheritedData, mask.m_xPosition, WTFMove(length)); }
    void setMaskYPosition(Length&& length) { SET_VAR(m_rareNonInheritedData, mask.m_yPosition, WTFMove(length)); }
    void setMaskSize(LengthSize size) { SET_VAR(m_rareNonInheritedData, mask.m_sizeLength, WTFMove(size)); }

    void setBorderCollapse(BorderCollapse collapse) { m_inheritedFlags.borderCollapse = static_cast<unsigned>(collapse); }
    void setHorizontalBorderSpacing(float);
    void setVerticalBorderSpacing(float);
    void setEmptyCells(EmptyCell v) { m_inheritedFlags.emptyCells = static_cast<unsigned>(v); }
    void setCaptionSide(CaptionSide v) { m_inheritedFlags.captionSide = static_cast<unsigned>(v); }

    void setAspectRatioType(AspectRatioType aspectRatioType) { SET_VAR(m_rareNonInheritedData, aspectRatioType, static_cast<unsigned>(aspectRatioType)); }
    void setAspectRatioDenominator(float v) { SET_VAR(m_rareNonInheritedData, aspectRatioDenominator, v); }
    void setAspectRatioNumerator(float v) { SET_VAR(m_rareNonInheritedData, aspectRatioNumerator, v); }

    void setListStyleType(ListStyleType v) { m_inheritedFlags.listStyleType = static_cast<unsigned>(v); }
    void setListStyleImage(RefPtr<StyleImage>&&);
    void setListStylePosition(ListStylePosition v) { m_inheritedFlags.listStylePosition = static_cast<unsigned>(v); }

    void resetMargin() { SET_VAR(m_surroundData, margin, LengthBox(Fixed)); }
    void setMarginTop(Length&& length) { SET_VAR(m_surroundData, margin.top(), WTFMove(length)); }
    void setMarginBottom(Length&& length) { SET_VAR(m_surroundData, margin.bottom(), WTFMove(length)); }
    void setMarginLeft(Length&& length) { SET_VAR(m_surroundData, margin.left(), WTFMove(length)); }
    void setMarginRight(Length&& length) { SET_VAR(m_surroundData, margin.right(), WTFMove(length)); }
    void setMarginStart(Length&&);
    void setMarginEnd(Length&&);

    void resetPadding() { SET_VAR(m_surroundData, padding, LengthBox(Auto)); }
    void setPaddingBox(LengthBox&& box) { SET_VAR(m_surroundData, padding, WTFMove(box)); }
    void setPaddingTop(Length&& length) { SET_VAR(m_surroundData, padding.top(), WTFMove(length)); }
    void setPaddingBottom(Length&& length) { SET_VAR(m_surroundData, padding.bottom(), WTFMove(length)); }
    void setPaddingLeft(Length&& length) { SET_VAR(m_surroundData, padding.left(), WTFMove(length)); }
    void setPaddingRight(Length&& length) { SET_VAR(m_surroundData, padding.right(), WTFMove(length)); }

    void setCursor(CursorType c) { m_inheritedFlags.cursor = static_cast<unsigned>(c); }
    void addCursor(RefPtr<StyleImage>&&, const IntPoint& hotSpot = IntPoint());
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

    bool hasAutoZIndex() const { return m_boxData->hasAutoZIndex(); }
    void setHasAutoZIndex() { SET_VAR(m_boxData, m_hasAutoZIndex, true); SET_VAR(m_boxData, m_zIndex, 0); }
    int zIndex() const { return m_boxData->zIndex(); }
    void setZIndex(int v) { SET_VAR(m_boxData, m_hasAutoZIndex, false); SET_VAR(m_boxData, m_zIndex, v); }

    void setHasAutoWidows() { SET_VAR(m_rareInheritedData, hasAutoWidows, true); SET_VAR(m_rareInheritedData, widows, initialWidows()); }
    void setWidows(short w) { SET_VAR(m_rareInheritedData, hasAutoWidows, false); SET_VAR(m_rareInheritedData, widows, w); }

    void setHasAutoOrphans() { SET_VAR(m_rareInheritedData, hasAutoOrphans, true); SET_VAR(m_rareInheritedData, orphans, initialOrphans()); }
    void setOrphans(short o) { SET_VAR(m_rareInheritedData, hasAutoOrphans, false); SET_VAR(m_rareInheritedData, orphans, o); }

    // CSS3 Setters
    void setOutlineOffset(float v) { SET_VAR(m_backgroundData, outline.m_offset, v); }
    void setTextShadow(std::unique_ptr<ShadowData>, bool add = false);
    void setTextStrokeColor(const Color& c) { SET_VAR(m_rareInheritedData, textStrokeColor, c); }
    void setTextStrokeWidth(float w) { SET_VAR(m_rareInheritedData, textStrokeWidth, w); }
    void setTextFillColor(const Color& c) { SET_VAR(m_rareInheritedData, textFillColor, c); }
    void setCaretColor(const Color& c) { SET_VAR(m_rareInheritedData, caretColor, c); }
    void setOpacity(float f) { float v = clampTo<float>(f, 0, 1); SET_VAR(m_rareNonInheritedData, opacity, v); }
    void setAppearance(ControlPart a) { SET_VAR(m_rareNonInheritedData, appearance, a); }
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
    void setFlexGrow(float f) { SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexGrow, f); }
    void setFlexShrink(float f) { SET_NESTED_VAR(m_rareNonInheritedData, flexibleBox, flexShrink, f); }
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

    void setGridAutoColumns(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoColumns, trackSizeList); }
    void setGridAutoRows(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoRows, trackSizeList); }
    void setGridColumns(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridColumns, lengths); }
    void setGridRows(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridRows, lengths); }
    void setGridAutoRepeatColumns(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoRepeatColumns, lengths); }
    void setGridAutoRepeatRows(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, grid, gridAutoRepeatRows, lengths); }
    void setGridAutoRepeatColumnsInsertionPoint(const unsigned insertionPoint) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatColumnsInsertionPoint, insertionPoint); }
    void setGridAutoRepeatRowsInsertionPoint(const unsigned insertionPoint) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatRowsInsertionPoint, insertionPoint); }
    void setGridAutoRepeatColumnsType(const AutoRepeatType autoRepeatType) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatColumnsType, autoRepeatType); }
    void setGridAutoRepeatRowsType(const AutoRepeatType autoRepeatType) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatRowsType, autoRepeatType); }
    void setNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, namedGridColumnLines, namedGridColumnLines); }
    void setNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, namedGridRowLines, namedGridRowLines); }
    void setOrderedNamedGridColumnLines(const OrderedNamedGridLinesMap& orderedNamedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, orderedNamedGridColumnLines, orderedNamedGridColumnLines); }
    void setOrderedNamedGridRowLines(const OrderedNamedGridLinesMap& orderedNamedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, orderedNamedGridRowLines, orderedNamedGridRowLines); }
    void setAutoRepeatNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatNamedGridColumnLines, namedGridColumnLines); }
    void setAutoRepeatNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatNamedGridRowLines, namedGridRowLines); }
    void setAutoRepeatOrderedNamedGridColumnLines(const OrderedNamedGridLinesMap& orderedNamedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatOrderedNamedGridColumnLines, orderedNamedGridColumnLines); }
    void setAutoRepeatOrderedNamedGridRowLines(const OrderedNamedGridLinesMap& orderedNamedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, grid, autoRepeatOrderedNamedGridRowLines, orderedNamedGridRowLines); }
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
    void setMarginBeforeCollapse(MarginCollapse c) { SET_VAR(m_rareNonInheritedData, marginBeforeCollapse, static_cast<unsigned>(c)); }
    void setMarginAfterCollapse(MarginCollapse c) { SET_VAR(m_rareNonInheritedData, marginAfterCollapse, static_cast<unsigned>(c)); }
    void setWordBreak(WordBreak b) { SET_VAR(m_rareInheritedData, wordBreak, static_cast<unsigned>(b)); }
    void setOverflowWrap(OverflowWrap b) { SET_VAR(m_rareInheritedData, overflowWrap, static_cast<unsigned>(b)); }
    void setNBSPMode(NBSPMode b) { SET_VAR(m_rareInheritedData, nbspMode, static_cast<unsigned>(b)); }
    void setLineBreak(LineBreak b) { SET_VAR(m_rareInheritedData, lineBreak, static_cast<unsigned>(b)); }
    void setHyphens(Hyphens h) { SET_VAR(m_rareInheritedData, hyphens, static_cast<unsigned>(h)); }
    void setHyphenationLimitBefore(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitBefore, limit); }
    void setHyphenationLimitAfter(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitAfter, limit); }
    void setHyphenationLimitLines(short limit) { SET_VAR(m_rareInheritedData, hyphenationLimitLines, limit); }
    void setHyphenationString(const AtomicString& h) { SET_VAR(m_rareInheritedData, hyphenationString, h); }
    void setBorderFit(BorderFit b) { SET_VAR(m_rareNonInheritedData, borderFit, static_cast<unsigned>(b)); }
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
    void setColumnRuleColor(const Color& c) { SET_BORDERVALUE_COLOR(m_rareNonInheritedData.access().multiCol, rule, c); }
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

    void setSpeakAs(OptionSet<SpeakAs> s) { SET_VAR(m_rareInheritedData, speakAs, s.toRaw()); }
    void setTextCombine(TextCombine v) { SET_VAR(m_rareNonInheritedData, textCombine, static_cast<unsigned>(v)); }
    void setTextDecorationColor(const Color& c) { SET_VAR(m_rareNonInheritedData, textDecorationColor, c); }
    void setTextEmphasisColor(const Color& c) { SET_VAR(m_rareInheritedData, textEmphasisColor, c); }
    void setTextEmphasisFill(TextEmphasisFill fill) { SET_VAR(m_rareInheritedData, textEmphasisFill, static_cast<unsigned>(fill)); }
    void setTextEmphasisMark(TextEmphasisMark mark) { SET_VAR(m_rareInheritedData, textEmphasisMark, static_cast<unsigned>(mark)); }
    void setTextEmphasisCustomMark(const AtomicString& mark) { SET_VAR(m_rareInheritedData, textEmphasisCustomMark, mark); }
    void setTextEmphasisPosition(OptionSet<TextEmphasisPosition> position) { SET_VAR(m_rareInheritedData, textEmphasisPosition, static_cast<unsigned>(position.toRaw())); }
    bool setTextOrientation(TextOrientation);

    void setObjectFit(ObjectFit fit) { SET_VAR(m_rareNonInheritedData, objectFit, static_cast<unsigned>(fit)); }
    void setObjectPosition(LengthPoint&& position) { SET_VAR(m_rareNonInheritedData, objectPosition, WTFMove(position)); }

    void setRubyPosition(RubyPosition position) { SET_VAR(m_rareInheritedData, rubyPosition, static_cast<unsigned>(position)); }

    void setFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, filter, operations, ops); }
    void setAppleColorFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareInheritedData, appleColorFilter, operations, ops); }

#if ENABLE(FILTERS_LEVEL_2)
    void setBackdropFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, backdropFilter, operations, ops); }
#endif

    void setTabSize(unsigned size) { SET_VAR(m_rareInheritedData, tabSize, size); }

    void setBreakBefore(BreakBetween breakBehavior) { SET_VAR(m_rareNonInheritedData, breakBefore, static_cast<unsigned>(breakBehavior)); }
    void setBreakAfter(BreakBetween breakBehavior) { SET_VAR(m_rareNonInheritedData, breakAfter, static_cast<unsigned>(breakBehavior)); }
    void setBreakInside(BreakInside breakBehavior) { SET_VAR(m_rareNonInheritedData, breakInside, static_cast<unsigned>(breakBehavior)); }
    
    void setHangingPunctuation(OptionSet<HangingPunctuation> punctuation) { SET_VAR(m_rareInheritedData, hangingPunctuation, punctuation.toRaw()); }

    // End CSS3 Setters

    void setLineGrid(const AtomicString& lineGrid) { SET_VAR(m_rareInheritedData, lineGrid, lineGrid); }
    void setLineSnap(LineSnap lineSnap) { SET_VAR(m_rareInheritedData, lineSnap, static_cast<unsigned>(lineSnap)); }
    void setLineAlign(LineAlign lineAlign) { SET_VAR(m_rareInheritedData, lineAlign, static_cast<unsigned>(lineAlign)); }

    void setPointerEvents(PointerEvents p) { m_inheritedFlags.pointerEvents = static_cast<unsigned>(p); }

    void clearAnimations();
    void clearTransitions();

    void adjustAnimations();
    void adjustTransitions();

    void setTransformStyle3D(TransformStyle3D b) { SET_VAR(m_rareNonInheritedData, transformStyle3D, static_cast<unsigned>(b)); }
    void setBackfaceVisibility(BackfaceVisibility b) { SET_VAR(m_rareNonInheritedData, backfaceVisibility, static_cast<unsigned>(b)); }
    void setPerspective(float p) { SET_VAR(m_rareNonInheritedData, perspective, p); }
    void setPerspectiveOriginX(Length&& length) { SET_VAR(m_rareNonInheritedData, perspectiveOriginX, WTFMove(length)); }
    void setPerspectiveOriginY(Length&& length) { SET_VAR(m_rareNonInheritedData, perspectiveOriginY, WTFMove(length)); }
    void setPageSize(LengthSize size) { SET_VAR(m_rareNonInheritedData, pageSize, WTFMove(size)); }
    void setPageSizeType(PageSizeType t) { SET_VAR(m_rareNonInheritedData, pageSizeType, t); }
    void resetPageSizeType() { SET_VAR(m_rareNonInheritedData, pageSizeType, PAGE_SIZE_AUTO); }

    void setLineBoxContain(LineBoxContain c) { SET_VAR(m_rareInheritedData, lineBoxContain, c); }
    void setLineClamp(LineClampValue c) { SET_VAR(m_rareNonInheritedData, lineClamp, c); }
    
    void setInitialLetter(const IntSize& size) { SET_VAR(m_rareNonInheritedData, initialLetter, size); }
    
#if ENABLE(TOUCH_EVENTS)
    void setTouchAction(TouchAction touchAction) { SET_VAR(m_rareNonInheritedData, touchAction, static_cast<unsigned>(touchAction)); }
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    void setScrollSnapType(const ScrollSnapType&);
    void setScrollPaddingTop(Length&&);
    void setScrollPaddingBottom(Length&&);
    void setScrollPaddingLeft(Length&&);
    void setScrollPaddingRight(Length&&);

    void setScrollSnapAlign(const ScrollSnapAlign&);
    void setScrollSnapMarginTop(Length&&);
    void setScrollSnapMarginBottom(Length&&);
    void setScrollSnapMarginLeft(Length&&);
    void setScrollSnapMarginRight(Length&&);
#endif

#if ENABLE(TOUCH_EVENTS)
    void setTapHighlightColor(const Color& c) { SET_VAR(m_rareInheritedData, tapHighlightColor, c); }
#endif

#if PLATFORM(IOS_FAMILY)
    void setTouchCalloutEnabled(bool v) { SET_VAR(m_rareInheritedData, touchCalloutEnabled, v); }
#endif

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    void setUseTouchOverflowScrolling(bool v) { SET_VAR(m_rareInheritedData, useTouchOverflowScrolling, v); }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    void setTextSizeAdjust(TextSizeAdjustment adjustment) { SET_VAR(m_rareInheritedData, textSizeAdjust, adjustment); }
#endif

    void setTextSecurity(TextSecurity security) { SET_VAR(m_rareInheritedData, textSecurity, static_cast<unsigned>(security)); }

#if ENABLE(CSS_TRAILING_WORD)
    void setTrailingWord(TrailingWord v) { SET_VAR(m_rareInheritedData, trailingWord, static_cast<unsigned>(v)); }
#endif

#if ENABLE(APPLE_PAY)
    void setApplePayButtonStyle(ApplePayButtonStyle style) { SET_VAR(m_rareNonInheritedData, applePayButtonStyle, static_cast<unsigned>(style)); }
    void setApplePayButtonType(ApplePayButtonType type) { SET_VAR(m_rareNonInheritedData, applePayButtonType, static_cast<unsigned>(type)); }
#endif

    // Support for paint-order, stroke-linecap, stroke-linejoin, and stroke-miterlimit from https://drafts.fxtf.org/paint/.
    void setPaintOrder(PaintOrder order) { SET_VAR(m_rareInheritedData, paintOrder, static_cast<unsigned>(order)); }
    PaintOrder paintOrder() const { return static_cast<PaintOrder>(m_rareInheritedData->paintOrder); }
    static PaintOrder initialPaintOrder() { return PaintOrder::Normal; }
    static Vector<PaintType, 3> paintTypesForPaintOrder(PaintOrder);
    
    void setCapStyle(LineCap val) { SET_VAR(m_rareInheritedData, capStyle, val); }
    LineCap capStyle() const { return static_cast<LineCap>(m_rareInheritedData->capStyle); }
    static LineCap initialCapStyle() { return ButtCap; }
    
    void setJoinStyle(LineJoin val) { SET_VAR(m_rareInheritedData, joinStyle, val); }
    LineJoin joinStyle() const { return static_cast<LineJoin>(m_rareInheritedData->joinStyle); }
    static LineJoin initialJoinStyle() { return MiterJoin; }
    
    const Length& strokeWidth() const { return m_rareInheritedData->strokeWidth; }
    void setStrokeWidth(Length&& w) { SET_VAR(m_rareInheritedData, strokeWidth, WTFMove(w)); }
    bool hasVisibleStroke() const { return svgStyle().hasStroke() && !strokeWidth().isZero(); }
    static Length initialStrokeWidth() { return initialOneLength(); }

    float computedStrokeWidth(const IntSize& viewportSize) const;
    void setHasExplicitlySetStrokeWidth(bool v) { SET_VAR(m_rareInheritedData, hasSetStrokeWidth, static_cast<unsigned>(v)); }
    bool hasExplicitlySetStrokeWidth() const { return m_rareInheritedData->hasSetStrokeWidth; };
    bool hasPositiveStrokeWidth() const;
    
    Color strokeColor() const { return m_rareInheritedData->strokeColor; }
    void setStrokeColor(const Color& v)  { SET_VAR(m_rareInheritedData, strokeColor, v); }
    void setVisitedLinkStrokeColor(const Color& v) { SET_VAR(m_rareInheritedData, visitedLinkStrokeColor, v); }
    const Color& visitedLinkStrokeColor() const { return m_rareInheritedData->visitedLinkStrokeColor; }
    void setHasExplicitlySetStrokeColor(bool v) { SET_VAR(m_rareInheritedData, hasSetStrokeColor, static_cast<unsigned>(v)); }
    bool hasExplicitlySetStrokeColor() const { return m_rareInheritedData->hasSetStrokeColor; };
    static Color initialStrokeColor() { return Color(Color::transparent); }
    Color computedStrokeColor() const;

    float strokeMiterLimit() const { return m_rareInheritedData->miterLimit; }
    void setStrokeMiterLimit(float f) { SET_VAR(m_rareInheritedData, miterLimit, f); }
    static float initialStrokeMiterLimit() { return defaultMiterLimit; }


    const SVGRenderStyle& svgStyle() const { return m_svgStyle; }
    SVGRenderStyle& accessSVGStyle() { return m_svgStyle.access(); }

    SVGPaintType fillPaintType() const { return svgStyle().fillPaintType(); }
    Color fillPaintColor() const { return svgStyle().fillPaintColor(); }
    void setFillPaintColor(const Color& color) { accessSVGStyle().setFillPaint(SVGPaintType::RGBColor, color, emptyString()); }
    float fillOpacity() const { return svgStyle().fillOpacity(); }
    void setFillOpacity(float f) { accessSVGStyle().setFillOpacity(f); }

    SVGPaintType strokePaintType() const { return svgStyle().strokePaintType(); }
    Color strokePaintColor() const { return svgStyle().strokePaintColor(); }
    void setStrokePaintColor(const Color& color) { accessSVGStyle().setStrokePaint(SVGPaintType::RGBColor, color, emptyString()); }
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

    void setStopColor(const Color& c) { accessSVGStyle().setStopColor(c); }
    void setFloodColor(const Color& c) { accessSVGStyle().setFloodColor(c); }
    void setLightingColor(const Color& c) { accessSVGStyle().setLightingColor(c); }

    SVGLengthValue baselineShiftValue() const { return svgStyle().baselineShiftValue(); }
    void setBaselineShiftValue(SVGLengthValue s) { accessSVGStyle().setBaselineShiftValue(s); }
    SVGLengthValue kerning() const { return svgStyle().kerning(); }
    void setKerning(SVGLengthValue k) { accessSVGStyle().setKerning(k); }

    void setShapeOutside(RefPtr<ShapeValue>&&);
    ShapeValue* shapeOutside() const { return m_rareNonInheritedData->shapeOutside.get(); }
    static ShapeValue* initialShapeOutside() { return nullptr; }

    const Length& shapeMargin() const { return m_rareNonInheritedData->shapeMargin; }
    void setShapeMargin(Length&& shapeMargin) { SET_VAR(m_rareNonInheritedData, shapeMargin, WTFMove(shapeMargin)); }
    static Length initialShapeMargin() { return Length(0, Fixed); }

    float shapeImageThreshold() const { return m_rareNonInheritedData->shapeImageThreshold; }
    void setShapeImageThreshold(float);
    static float initialShapeImageThreshold() { return 0; }

    void setClipPath(RefPtr<ClipPathOperation>&&);
    ClipPathOperation* clipPath() const { return m_rareNonInheritedData->clipPath.get(); }
    static ClipPathOperation* initialClipPath() { return nullptr; }

    bool hasContent() const { return contentData(); }
    const ContentData* contentData() const { return m_rareNonInheritedData->content.get(); }
    bool contentDataEquivalent(const RenderStyle* otherStyle) const { return const_cast<RenderStyle*>(this)->m_rareNonInheritedData->contentDataEquivalent(*const_cast<RenderStyle*>(otherStyle)->m_rareNonInheritedData); }
    void clearContent();
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

    const AtomicString& hyphenString() const;

    bool inheritedNotEqual(const RenderStyle*) const;
    bool inheritedDataShared(const RenderStyle*) const;

#if ENABLE(TEXT_AUTOSIZING)
    uint32_t hashForTextAutosizing() const;
    bool equalForTextAutosizing(const RenderStyle&) const;
#endif

    StyleDifference diff(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool diffRequiresLayerRepaint(const RenderStyle&, bool isComposited) const;

    bool isDisplayInlineType() const { return isDisplayInlineType(display()); }
    bool isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
    bool isDisplayFlexibleOrGridBox() const { return isDisplayFlexibleOrGridBox(display()); }
    bool isDisplayRegionType() const;

    bool setWritingMode(WritingMode);

    bool hasExplicitlySetWritingMode() const { return m_nonInheritedFlags.hasExplicitlySetWritingMode; }
    void setHasExplicitlySetWritingMode(bool v) { m_nonInheritedFlags.hasExplicitlySetWritingMode = v; }

    bool hasExplicitlySetTextAlign() const { return m_nonInheritedFlags.hasExplicitlySetTextAlign; }
    void setHasExplicitlySetTextAlign(bool v) { m_nonInheritedFlags.hasExplicitlySetTextAlign = v; }

    // A unique style is one that has matches something that makes it impossible to share.
    bool unique() const { return m_nonInheritedFlags.isUnique; }
    void setUnique() { m_nonInheritedFlags.isUnique = true; }

    bool emptyState() const { return m_nonInheritedFlags.emptyState; }
    void setEmptyState(bool v) { setUnique(); m_nonInheritedFlags.emptyState = v; }
    bool firstChildState() const { return m_nonInheritedFlags.firstChildState; }
    void setFirstChildState() { setUnique(); m_nonInheritedFlags.firstChildState = true; }
    bool lastChildState() const { return m_nonInheritedFlags.lastChildState; }
    void setLastChildState() { setUnique(); m_nonInheritedFlags.lastChildState = true; }

    WEBCORE_EXPORT Color visitedDependentColor(CSSPropertyID) const;
    WEBCORE_EXPORT Color visitedDependentColorWithColorFilter(CSSPropertyID) const;

    WEBCORE_EXPORT Color colorByApplyingColorFilter(const Color&) const;

    bool backgroundColorEqualsToColorIgnoringVisited(const Color& color) const { return color == backgroundColor(); }

    void setHasExplicitlyInheritedProperties() { m_nonInheritedFlags.hasExplicitlyInheritedProperties = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedFlags.hasExplicitlyInheritedProperties; }
    
    // Initial values for all the properties
    static Overflow initialOverflowX() { return Overflow::Visible; }
    static Overflow initialOverflowY() { return Overflow::Visible; }
    static Clear initialClear() { return Clear::None; }
    static DisplayType initialDisplay() { return DisplayType::Inline; }
    static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
    static PositionType initialPosition() { return PositionType::Static; }
    static VerticalAlign initialVerticalAlign() { return VerticalAlign::Baseline; }
    static Float initialFloating() { return Float::No; }
    static BreakBetween initialBreakBetween() { return BreakBetween::Auto; }
    static BreakInside initialBreakInside() { return BreakInside::Auto; }
    static OptionSet<HangingPunctuation> initialHangingPunctuation() { return OptionSet<HangingPunctuation> { }; }
    static TableLayoutType initialTableLayout() { return TableLayoutType::Auto; }
    static BorderCollapse initialBorderCollapse() { return BorderCollapse::Separate; }
    static BorderStyle initialBorderStyle() { return BorderStyle::None; }
    static OutlineIsAuto initialOutlineStyleIsAuto() { return OutlineIsAuto::Off; }
    static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
    static LengthSize initialBorderRadius() { return { { 0, Fixed }, { 0, Fixed } }; }
    static CaptionSide initialCaptionSide() { return CaptionSide::Top; }
    static ColumnAxis initialColumnAxis() { return ColumnAxis::Auto; }
    static ColumnProgression initialColumnProgression() { return ColumnProgression::Normal; }
    static TextDirection initialDirection() { return TextDirection::LTR; }
    static WritingMode initialWritingMode() { return TopToBottomWritingMode; }
    static TextCombine initialTextCombine() { return TextCombine::None; }
    static TextOrientation initialTextOrientation() { return TextOrientation::Mixed; }
    static ObjectFit initialObjectFit() { return ObjectFit::Fill; }
    static LengthPoint initialObjectPosition() { return LengthPoint(Length(50.0f, Percent), Length(50.0f, Percent)); }
    static EmptyCell initialEmptyCells() { return EmptyCell::Show; }
    static ListStylePosition initialListStylePosition() { return ListStylePosition::Outside; }
    static ListStyleType initialListStyleType() { return ListStyleType::Disc; }
    static TextTransform initialTextTransform() { return TextTransform::None; }
    static Visibility initialVisibility() { return Visibility::Visible; }
    static WhiteSpace initialWhiteSpace() { return WhiteSpace::Normal; }
    static float initialHorizontalBorderSpacing() { return 0; }
    static float initialVerticalBorderSpacing() { return 0; }
    static CursorType initialCursor() { return CursorType::Auto; }
    static Color initialColor() { return Color::black; }
    static StyleImage* initialListStyleImage() { return 0; }
    static float initialBorderWidth() { return 3; }
    static unsigned short initialColumnRuleWidth() { return 3; }
    static float initialOutlineWidth() { return 3; }
    static float initialLetterSpacing() { return 0; }
    static Length initialWordSpacing() { return Length(Fixed); }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(); }
    static Length initialMaxSize() { return Length(Undefined); }
    static Length initialOffset() { return Length(); }
    static Length initialMargin() { return Length(Fixed); }
    static Length initialPadding() { return Length(Fixed); }
    static Length initialTextIndent() { return Length(Fixed); }
    static Length initialZeroLength() { return Length(Fixed); }
    static Length initialOneLength() { return Length(1, Fixed); }
    static short initialWidows() { return 2; }
    static short initialOrphans() { return 2; }
    static Length initialLineHeight() { return Length(-100.0f, Percent); }
    static TextAlignMode initialTextAlign() { return TextAlignMode::Start; }
    static OptionSet<TextDecoration> initialTextDecoration() { return OptionSet<TextDecoration> { }; }
    static TextDecorationStyle initialTextDecorationStyle() { return TextDecorationStyle::Solid; }
    static OptionSet<TextDecorationSkip> initialTextDecorationSkip() { return TextDecorationSkip::Auto; }
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
    static Length initialFlexBasis() { return Length(Auto); }
    static int initialOrder() { return 0; }
    static StyleSelfAlignmentData initialJustifyItems() { return StyleSelfAlignmentData(ItemPosition::Legacy, OverflowAlignment::Default); }
    static StyleSelfAlignmentData initialSelfAlignment() { return StyleSelfAlignmentData(ItemPosition::Auto, OverflowAlignment::Default); }
    static StyleSelfAlignmentData initialDefaultAlignment() { return StyleSelfAlignmentData(ItemPosition::Normal, OverflowAlignment::Default); }
    static StyleContentAlignmentData initialContentAlignment() { return StyleContentAlignmentData(ContentPosition::Normal, ContentDistribution::Default, OverflowAlignment::Default); }
    static FlexDirection initialFlexDirection() { return FlexDirection::Row; }
    static FlexWrap initialFlexWrap() { return FlexWrap::NoWrap; }
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static Length initialMarqueeIncrement() { return Length(6, Fixed); }
    static MarqueeBehavior initialMarqueeBehavior() { return MarqueeBehavior::Scroll; }
    static MarqueeDirection initialMarqueeDirection() { return MarqueeDirection::Auto; }
    static UserModify initialUserModify() { return UserModify::ReadOnly; }
    static UserDrag initialUserDrag() { return UserDrag::Auto; }
    static UserSelect initialUserSelect() { return UserSelect::Text; }
    static TextOverflow initialTextOverflow() { return TextOverflow::Clip; }
    static MarginCollapse initialMarginBeforeCollapse() { return MarginCollapse::Collapse; }
    static MarginCollapse initialMarginAfterCollapse() { return MarginCollapse::Collapse; }
    static WordBreak initialWordBreak() { return WordBreak::Normal; }
    static OverflowWrap initialOverflowWrap() { return OverflowWrap::Normal; }
    static NBSPMode initialNBSPMode() { return NBSPMode::Normal; }
    static LineBreak initialLineBreak() { return LineBreak::Auto; }
    static OptionSet<SpeakAs> initialSpeakAs() { return OptionSet<SpeakAs> { }; }
    static Hyphens initialHyphens() { return Hyphens::Manual; }
    static short initialHyphenationLimitBefore() { return -1; }
    static short initialHyphenationLimitAfter() { return -1; }
    static short initialHyphenationLimitLines() { return -1; }
    static const AtomicString& initialHyphenationString() { return nullAtom(); }
    static BorderFit initialBorderFit() { return BorderFit::Border; }
    static Resize initialResize() { return Resize::None; }
    static ControlPart initialAppearance() { return NoControlPart; }
    static AspectRatioType initialAspectRatioType() { return AspectRatioType::Auto; }
    static float initialAspectRatioDenominator() { return 1; }
    static float initialAspectRatioNumerator() { return 1; }
    static Order initialRTLOrdering() { return Order::Logical; }
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static ColumnFill initialColumnFill() { return ColumnFill::Balance; }
    static ColumnSpan initialColumnSpan() { return ColumnSpan::None; }
    static GapLength initialColumnGap() { return GapLength(); }
    static GapLength initialRowGap() { return GapLength(); }
    static const TransformOperations& initialTransform() { static NeverDestroyed<TransformOperations> ops; return ops; }
    static Length initialTransformOriginX() { return Length(50.0f, Percent); }
    static Length initialTransformOriginY() { return Length(50.0f, Percent); }
    static TransformBox initialTransformBox() { return TransformBox::BorderBox; }
    static PointerEvents initialPointerEvents() { return PointerEvents::Auto; }
    static float initialTransformOriginZ() { return 0; }
    static TransformStyle3D initialTransformStyle3D() { return TransformStyle3D::Flat; }
    static BackfaceVisibility initialBackfaceVisibility() { return BackfaceVisibility::Visible; }
    static float initialPerspective() { return 0; }
    static Length initialPerspectiveOriginX() { return Length(50.0f, Percent); }
    static Length initialPerspectiveOriginY() { return Length(50.0f, Percent); }
    static Color initialBackgroundColor() { return Color::transparent; }
    static Color initialTextEmphasisColor() { return Color(); }
    static TextEmphasisFill initialTextEmphasisFill() { return TextEmphasisFill::Filled; }
    static TextEmphasisMark initialTextEmphasisMark() { return TextEmphasisMark::None; }
    static const AtomicString& initialTextEmphasisCustomMark() { return nullAtom(); }
    static OptionSet<TextEmphasisPosition> initialTextEmphasisPosition() { return { TextEmphasisPosition::Over, TextEmphasisPosition::Right }; }
    static RubyPosition initialRubyPosition() { return RubyPosition::Before; }
    static LineBoxContain initialLineBoxContain() { return LineBoxContainBlock | LineBoxContainInline | LineBoxContainReplaced; }
    static ImageOrientationEnum initialImageOrientation() { return OriginTopLeft; }
    static ImageRendering initialImageRendering() { return ImageRendering::Auto; }
    static ImageResolutionSource initialImageResolutionSource() { return ImageResolutionSource::Specified; }
    static ImageResolutionSnap initialImageResolutionSnap() { return ImageResolutionSnap::None; }
    static float initialImageResolution() { return 1; }
    static StyleImage* initialBorderImageSource() { return nullptr; }
    static StyleImage* initialMaskBoxImageSource() { return nullptr; }
    static PrintColorAdjust initialPrintColorAdjust() { return PrintColorAdjust::Economy; }
    static QuotesData* initialQuotes() { return nullptr; }
    static const AtomicString& initialContentAltText() { return emptyAtom(); }

#if ENABLE(CSS3_TEXT)
    static TextIndentLine initialTextIndentLine() { return TextIndentLine::FirstLine; }
    static TextIndentType initialTextIndentType() { return TextIndentType::Normal; }
    static TextAlignLast initialTextAlignLast() { return TextAlignLast::Auto; }
    static TextJustify initialTextJustify() { return TextJustify::Auto; }
#endif

#if ENABLE(CURSOR_VISIBILITY)
    static CursorVisibility initialCursorVisibility() { return CursorVisibility::Auto; }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    static Length initialSpecifiedLineHeight() { return Length(-100.0f, Percent); }
    static TextSizeAdjustment initialTextSizeAdjust() { return TextSizeAdjustment(); }
#endif

    static WillChangeData* initialWillChange() { return nullptr; }

#if ENABLE(TOUCH_EVENTS)
    static TouchAction initialTouchAction() { return TouchAction::Auto; }
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    static ScrollSnapType initialScrollSnapType();
    static ScrollSnapAlign initialScrollSnapAlign();
    static Length initialScrollSnapMargin() { return Length(Fixed); }
    static Length initialScrollPadding() { return Length(Fixed); }
#endif

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

    static Vector<GridTrackSize> initialGridAutoColumns() { return { GridTrackSize(Length(Auto)) }; }
    static Vector<GridTrackSize> initialGridAutoRows() { return { GridTrackSize(Length(Auto)) }; }

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

    static unsigned initialTabSize() { return 8; }

    static const AtomicString& initialLineGrid() { return nullAtom(); }
    static LineSnap initialLineSnap() { return LineSnap::None; }
    static LineAlign initialLineAlign() { return LineAlign::None; }

    static IntSize initialInitialLetter() { return IntSize(); }
    static LineClampValue initialLineClamp() { return LineClampValue(); }
    static TextSecurity initialTextSecurity() { return TextSecurity::None; }

#if PLATFORM(IOS_FAMILY)
    static bool initialTouchCalloutEnabled() { return true; }
#endif

#if ENABLE(TOUCH_EVENTS)
    static Color initialTapHighlightColor();
#endif

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    static bool initialUseTouchOverflowScrolling() { return false; }
#endif

#if ENABLE(DASHBOARD_SUPPORT)
    static const Vector<StyleDashboardRegion>& initialDashboardRegions();
    static const Vector<StyleDashboardRegion>& noneDashboardRegions();
#endif

    static const FilterOperations& initialFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }
    static const FilterOperations& initialAppleColorFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }

#if ENABLE(FILTERS_LEVEL_2)
    static const FilterOperations& initialBackdropFilter() { static NeverDestroyed<FilterOperations> ops; return ops; }
#endif

#if ENABLE(CSS_COMPOSITING)
    static BlendMode initialBlendMode() { return BlendMode::Normal; }
    static Isolation initialIsolation() { return Isolation::Auto; }
#endif

    // Indicates the style is likely to change due to a pending stylesheet load.
    bool isNotFinal() const { return m_rareNonInheritedData->isNotFinal; }
    void setIsNotFinal() { SET_VAR(m_rareNonInheritedData, isNotFinal, true); }

    void setVisitedLinkColor(const Color&);
    void setVisitedLinkBackgroundColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBackgroundColor, v); }
    void setVisitedLinkBorderLeftColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderLeftColor, v); }
    void setVisitedLinkBorderRightColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderRightColor, v); }
    void setVisitedLinkBorderBottomColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderBottomColor, v); }
    void setVisitedLinkBorderTopColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkBorderTopColor, v); }
    void setVisitedLinkOutlineColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkOutlineColor, v); }
    void setVisitedLinkColumnRuleColor(const Color& v) { SET_NESTED_VAR(m_rareNonInheritedData, multiCol, visitedLinkColumnRuleColor, v); }
    void setVisitedLinkTextDecorationColor(const Color& v) { SET_VAR(m_rareNonInheritedData, visitedLinkTextDecorationColor, v); }
    void setVisitedLinkTextEmphasisColor(const Color& v) { SET_VAR(m_rareInheritedData, visitedLinkTextEmphasisColor, v); }
    void setVisitedLinkTextFillColor(const Color& v) { SET_VAR(m_rareInheritedData, visitedLinkTextFillColor, v); }
    void setVisitedLinkTextStrokeColor(const Color& v) { SET_VAR(m_rareInheritedData, visitedLinkTextStrokeColor, v); }
    void setVisitedLinkCaretColor(const Color& v) { SET_VAR(m_rareInheritedData, visitedLinkCaretColor, v); }

    void inheritUnicodeBidiFrom(const RenderStyle* parent) { m_nonInheritedFlags.unicodeBidi = parent->m_nonInheritedFlags.unicodeBidi; }
    void getShadowExtent(const ShadowData*, LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const;
    void getShadowHorizontalExtent(const ShadowData*, LayoutUnit& left, LayoutUnit& right) const;
    void getShadowVerticalExtent(const ShadowData*, LayoutUnit& top, LayoutUnit& bottom) const;
    void getShadowInlineDirectionExtent(const ShadowData*, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const;
    void getShadowBlockDirectionExtent(const ShadowData*, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const;

    static Color invalidColor() { return Color(); }
    const Color& borderLeftColor() const { return m_surroundData->border.left().color(); }
    const Color& borderRightColor() const { return m_surroundData->border.right().color(); }
    const Color& borderTopColor() const { return m_surroundData->border.top().color(); }
    const Color& borderBottomColor() const { return m_surroundData->border.bottom().color(); }
    const Color& backgroundColor() const { return m_backgroundData->color; }
    const Color& color() const;
    const Color& columnRuleColor() const { return m_rareNonInheritedData->multiCol->rule.color(); }
    const Color& outlineColor() const { return m_backgroundData->outline.color(); }
    const Color& textEmphasisColor() const { return m_rareInheritedData->textEmphasisColor; }
    const Color& textFillColor() const { return m_rareInheritedData->textFillColor; }
    const Color& textStrokeColor() const { return m_rareInheritedData->textStrokeColor; }
    const Color& caretColor() const { return m_rareInheritedData->caretColor; }
    const Color& visitedLinkColor() const;
    const Color& visitedLinkBackgroundColor() const { return m_rareNonInheritedData->visitedLinkBackgroundColor; }
    const Color& visitedLinkBorderLeftColor() const { return m_rareNonInheritedData->visitedLinkBorderLeftColor; }
    const Color& visitedLinkBorderRightColor() const { return m_rareNonInheritedData->visitedLinkBorderRightColor; }
    const Color& visitedLinkBorderBottomColor() const { return m_rareNonInheritedData->visitedLinkBorderBottomColor; }
    const Color& visitedLinkBorderTopColor() const { return m_rareNonInheritedData->visitedLinkBorderTopColor; }
    const Color& visitedLinkOutlineColor() const { return m_rareNonInheritedData->visitedLinkOutlineColor; }
    const Color& visitedLinkColumnRuleColor() const { return m_rareNonInheritedData->multiCol->visitedLinkColumnRuleColor; }
    const Color& textDecorationColor() const { return m_rareNonInheritedData->textDecorationColor; }
    const Color& visitedLinkTextDecorationColor() const { return m_rareNonInheritedData->visitedLinkTextDecorationColor; }
    const Color& visitedLinkTextEmphasisColor() const { return m_rareInheritedData->visitedLinkTextEmphasisColor; }
    const Color& visitedLinkTextFillColor() const { return m_rareInheritedData->visitedLinkTextFillColor; }
    const Color& visitedLinkTextStrokeColor() const { return m_rareInheritedData->visitedLinkTextStrokeColor; }
    const Color& visitedLinkCaretColor() const { return m_rareInheritedData->visitedLinkCaretColor; }

    const Color& stopColor() const { return svgStyle().stopColor(); }
    const Color& floodColor() const { return svgStyle().floodColor(); }
    const Color& lightingColor() const { return svgStyle().lightingColor(); }

private:
    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const;
        bool operator!=(const NonInheritedFlags& other) const { return !(*this == other); }

        void copyNonInheritedFrom(const NonInheritedFlags&);

        bool hasAnyPublicPseudoStyles() const { return static_cast<unsigned>(PseudoId::PublicPseudoIdMask) & pseudoBits; }
        bool hasPseudoStyle(PseudoId) const;
        void setHasPseudoStyle(PseudoId);
        void setHasPseudoStyles(PseudoIdSet);

        unsigned effectiveDisplay : 5; // DisplayType
        unsigned originalDisplay : 5; // DisplayType
        unsigned overflowX : 3; // Overflow
        unsigned overflowY : 3; // Overflow
        unsigned verticalAlign : 4; // VerticalAlign
        unsigned clear : 2; // Clear
        unsigned position : 3; // PositionType
        unsigned unicodeBidi : 3; // EUnicodeBidi
        unsigned floating : 2; // Float
        unsigned tableLayout : 1; // TableLayoutType

        unsigned hasExplicitlySetDirection : 1;
        unsigned hasExplicitlySetWritingMode : 1;
        unsigned hasExplicitlySetTextAlign : 1;
        unsigned hasViewportUnits : 1;
        unsigned hasExplicitlyInheritedProperties : 1; // Explicitly inherits a non-inherited property.
        unsigned isUnique : 1; // Style cannot be shared.
        unsigned emptyState : 1;
        unsigned firstChildState : 1;
        unsigned lastChildState : 1;
        unsigned affectedByHover : 1;
        unsigned affectedByActive : 1;
        unsigned affectedByDrag : 1;
        unsigned isLink : 1;

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
        unsigned textDecorations : TextDecorationBits;
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
    };

    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    DisplayType originalDisplay() const { return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay); }

    bool hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
    bool hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }

    void setContent(std::unique_ptr<ContentData>, bool add);

    LayoutBoxExtent getShadowInsetExtent(const ShadowData*) const;

    static bool isDisplayReplacedType(DisplayType);
    static bool isDisplayInlineType(DisplayType);
    static bool isDisplayFlexibleBox(DisplayType);
    static bool isDisplayGridBox(DisplayType);
    static bool isDisplayFlexibleOrGridBox(DisplayType);

    Color colorIncludingFallback(CSSPropertyID colorProperty, bool visitedLink) const;

    bool changeAffectsVisualOverflow(const RenderStyle&) const;
    bool changeRequiresLayout(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresPositionedLayoutOnly(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresLayerRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaintIfTextOrBorderOrOutline(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
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

#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_deletionHasBegun { false };
#endif
};

int adjustForAbsoluteZoom(int, const RenderStyle&);
float adjustFloatForAbsoluteZoom(float, const RenderStyle&);
LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderStyle&);

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
        && hasExplicitlySetDirection == other.hasExplicitlySetDirection
        && hasExplicitlySetWritingMode == other.hasExplicitlySetWritingMode
        && hasExplicitlySetTextAlign == other.hasExplicitlySetTextAlign
        && hasViewportUnits == other.hasViewportUnits
        && hasExplicitlyInheritedProperties == other.hasExplicitlyInheritedProperties
        && isUnique == other.isUnique
        && emptyState == other.emptyState
        && firstChildState == other.firstChildState
        && lastChildState == other.lastChildState
        && affectedByHover == other.affectedByHover
        && affectedByActive == other.affectedByActive
        && affectedByDrag == other.affectedByDrag
        && isLink == other.isLink
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
    hasViewportUnits = other.hasViewportUnits;
    hasExplicitlyInheritedProperties = other.hasExplicitlyInheritedProperties;
}

inline bool RenderStyle::NonInheritedFlags::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > PseudoId::None);
    ASSERT(pseudo < PseudoId::FirstInternalPseudoId);
    return pseudoBits & (1 << (static_cast<unsigned>(pseudo) - 1 /* PseudoId::None */));
}

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyle(PseudoId pseudo)
{
    ASSERT(pseudo > PseudoId::None);
    ASSERT(pseudo < PseudoId::FirstInternalPseudoId);
    pseudoBits |= 1 << (static_cast<unsigned>(pseudo) - 1 /* PseudoId::None */);
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
        && textDecorations == other.textDecorations
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
    return value / style.effectiveZoom();
}

inline BorderStyle collapsedBorderStyle(BorderStyle style)
{
    if (style == BorderStyle::Outset)
        return BorderStyle::Groove;
    if (style == BorderStyle::Inset)
        return BorderStyle::Ridge;
    return style;
}

inline const CSSCustomPropertyValue* RenderStyle::getCustomProperty(const AtomicString& name) const
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
    return ws != WhiteSpace::Pre && ws != WhiteSpace::PreWrap;
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
    return whiteSpace() == WhiteSpace::PreWrap || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool RenderStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::Break;
}

inline bool RenderStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || isHorizontalWritingMode() == (axis == ColumnAxis::Horizontal);
}

inline ImageOrientationEnum RenderStyle::imageOrientation() const
{
#if ENABLE(CSS_IMAGE_ORIENTATION)
    return static_cast<ImageOrientationEnum>(m_rareInheritedData->imageOrientation);
#else
    return DefaultImageOrientation;
#endif
}

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
    setBorderRadius(LengthSize { { size.width(), Fixed }, { size.height(), Fixed } });
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
    float clampedShapeImageThreshold = clampTo<float>(shapeImageThreshold, 0, 1);
    SET_VAR(m_rareNonInheritedData, shapeImageThreshold, clampedShapeImageThreshold);
}

inline void RenderStyle::setClipPath(RefPtr<ClipPathOperation>&& operation)
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

inline bool RenderStyle::setWritingMode(WritingMode v)
{
    if (v == writingMode())
        return false;
    m_inheritedFlags.writingMode = v;
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

inline bool RenderStyle::isDisplayReplacedType(DisplayType display)
{
    return display == DisplayType::InlineBlock || display == DisplayType::InlineBox || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid || display == DisplayType::InlineTable;
}

inline bool RenderStyle::isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::Inline || isDisplayReplacedType(display);
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

inline bool RenderStyle::hasAnyPublicPseudoStyles() const
{
    return m_nonInheritedFlags.hasAnyPublicPseudoStyles();
}

inline bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    return m_nonInheritedFlags.hasPseudoStyle(pseudo);
}

inline void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    m_nonInheritedFlags.setHasPseudoStyle(pseudo);
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

} // namespace WebCore

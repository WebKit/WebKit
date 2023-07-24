/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
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
 */

#pragma once

#include "AnimationList.h"
#include "CSSLineBoxContainValue.h"
#include "Element.h"
#include "FontCascadeDescription.h"
#include "GraphicsTypes.h"
#include "ImageOrientation.h"
#include "RenderStyle.h"
#include "ScrollTypes.h"
#include "ScrollbarColor.h"
#include "StyleAppearance.h"
#include "StyleBackgroundData.h"
#include "StyleBoxData.h"
#include "StyleDeprecatedFlexibleBoxData.h"
#include "StyleFilterData.h"
#include "StyleFlexibleBoxData.h"
#include "StyleGridData.h"
#include "StyleGridItemData.h"
#include "StyleMarqueeData.h"
#include "StyleMiscNonInheritedData.h"
#include "StyleMultiColData.h"
#include "StyleNonInheritedData.h"
#include "StyleRareInheritedData.h"
#include "StyleRareNonInheritedData.h"
#include "StyleSurroundData.h"
#include "StyleTransformData.h"
#include "StyleVisitedLinkColorData.h"
#include "UnicodeBidi.h"

#if ENABLE(APPLE_PAY)
#include "ApplePayButtonPart.h"
#endif

namespace WebCore {

inline const StyleColor& RenderStyle::accentColor() const { return m_rareInheritedData->accentColor; }
inline bool RenderStyle::affectsTransform() const { return hasTransform() || offsetPath() || rotate() || scale() || translate(); }
inline const StyleContentAlignmentData& RenderStyle::alignContent() const { return m_nonInheritedData->miscData->alignContent; }
inline const StyleSelfAlignmentData& RenderStyle::alignItems() const { return m_nonInheritedData->miscData->alignItems; }
inline const StyleSelfAlignmentData& RenderStyle::alignSelf() const { return m_nonInheritedData->miscData->alignSelf; }
inline const Vector<StyleContentAlignmentData>& RenderStyle::alignTracks() const { return m_nonInheritedData->rareData->grid->alignTracks; }
constexpr auto RenderStyle::allTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::TransformOrigin, TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }
inline const AnimationList* RenderStyle::animations() const { return m_nonInheritedData->miscData->animations.get(); }
inline AnimationList* RenderStyle::animations() { return m_nonInheritedData->miscData->animations.get(); }
inline StyleAppearance RenderStyle::appearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->appearance); }
inline const FilterOperations& RenderStyle::appleColorFilter() const { return m_rareInheritedData->appleColorFilter->operations; }
inline double RenderStyle::aspectRatioHeight() const { return m_nonInheritedData->miscData->aspectRatioHeight; }
inline double RenderStyle::aspectRatioLogicalHeight() const { return isHorizontalWritingMode() ? aspectRatioHeight() : aspectRatioWidth(); }
inline double RenderStyle::aspectRatioLogicalWidth() const { return isHorizontalWritingMode() ? aspectRatioWidth() : aspectRatioHeight(); }
inline AspectRatioType RenderStyle::aspectRatioType() const { return static_cast<AspectRatioType>(m_nonInheritedData->miscData->aspectRatioType); }
inline double RenderStyle::aspectRatioWidth() const { return m_nonInheritedData->miscData->aspectRatioWidth; }
inline const NamedGridLinesMap& RenderStyle::autoRepeatNamedGridColumnLines() const { return m_nonInheritedData->rareData->grid->autoRepeatNamedGridColumnLines(); }
inline const NamedGridLinesMap& RenderStyle::autoRepeatNamedGridRowLines() const { return m_nonInheritedData->rareData->grid->autoRepeatNamedGridRowLines(); }
inline const OrderedNamedGridLinesMap& RenderStyle::autoRepeatOrderedNamedGridColumnLines() const { return m_nonInheritedData->rareData->grid->autoRepeatOrderedNamedGridColumnLines(); }
inline const OrderedNamedGridLinesMap& RenderStyle::autoRepeatOrderedNamedGridRowLines() const { return m_nonInheritedData->rareData->grid->autoRepeatOrderedNamedGridRowLines(); }
inline bool RenderStyle::autoWrap() const { return autoWrap(whiteSpace()); }
inline BackfaceVisibility RenderStyle::backfaceVisibility() const { return static_cast<BackfaceVisibility>(m_nonInheritedData->rareData->backfaceVisibility); }
inline FillAttachment RenderStyle::backgroundAttachment() const { return backgroundLayers().attachment(); }
inline BlendMode RenderStyle::backgroundBlendMode() const { return backgroundLayers().blendMode(); }
inline FillBox RenderStyle::backgroundClip() const { return backgroundLayers().clip(); }
inline const StyleColor& RenderStyle::backgroundColor() const { return m_nonInheritedData->backgroundData->color; }
inline const FillLayer& RenderStyle::backgroundLayers() const { return m_nonInheritedData->backgroundData->background; }
inline FillBox RenderStyle::backgroundOrigin() const { return backgroundLayers().origin(); }
inline FillRepeatXY RenderStyle::backgroundRepeat() const { return backgroundLayers().repeat(); }
inline const LengthSize& RenderStyle::backgroundSizeLength() const { return backgroundLayers().sizeLength(); }
inline FillSizeType RenderStyle::backgroundSizeType() const { return backgroundLayers().sizeType(); }
inline const Length& RenderStyle::backgroundXPosition() const { return backgroundLayers().xPosition(); }
inline const Length& RenderStyle::backgroundYPosition() const { return backgroundLayers().yPosition(); }
inline BlockStepInsert RenderStyle::blockStepInsert() const { return static_cast<BlockStepInsert>(m_nonInheritedData->rareData->blockStepInsert); }
inline std::optional<Length> RenderStyle::blockStepSize() const { return m_nonInheritedData->rareData->blockStepSize; }
inline const BorderData& RenderStyle::border() const { return m_nonInheritedData->surroundData->border; }
inline const BorderValue& RenderStyle::borderBottom() const { return border().bottom(); }
inline const StyleColor& RenderStyle::borderBottomColor() const { return border().bottom().color(); }
inline bool RenderStyle::borderBottomIsTransparent() const { return border().bottom().isTransparent(); }
inline const LengthSize& RenderStyle::borderBottomLeftRadius() const { return border().bottomLeftRadius(); }
inline const LengthSize& RenderStyle::borderBottomRightRadius() const { return border().bottomRightRadius(); }
inline BorderStyle RenderStyle::borderBottomStyle() const { return border().bottom().style(); }
inline float RenderStyle::borderBottomWidth() const { return border().borderBottomWidth(); }
inline const NinePieceImage& RenderStyle::borderImage() const { return border().image(); }
inline NinePieceImageRule RenderStyle::borderImageHorizontalRule() const { return border().image().horizontalRule(); }
inline const LengthBox& RenderStyle::borderImageOutset() const { return border().image().outset(); }
inline LayoutBoxExtent RenderStyle::borderImageOutsets() const { return imageOutsets(borderImage()); }
inline const LengthBox& RenderStyle::borderImageSlices() const { return border().image().imageSlices(); }
inline StyleImage* RenderStyle::borderImageSource() const { return border().image().image(); }
inline NinePieceImageRule RenderStyle::borderImageVerticalRule() const { return border().image().verticalRule(); }
inline const LengthBox& RenderStyle::borderImageWidth() const { return border().image().borderSlices(); }
inline const BorderValue& RenderStyle::borderLeft() const { return border().left(); }
inline const StyleColor& RenderStyle::borderLeftColor() const { return border().left().color(); }
inline bool RenderStyle::borderLeftIsTransparent() const { return border().left().isTransparent(); }
inline BorderStyle RenderStyle::borderLeftStyle() const { return border().left().style(); }
inline float RenderStyle::borderLeftWidth() const { return border().borderLeftWidth(); }
inline const BorderDataRadii& RenderStyle::borderRadii() const { return border().m_radii; }
inline const BorderValue& RenderStyle::borderRight() const { return border().right(); }
inline const StyleColor& RenderStyle::borderRightColor() const { return border().right().color(); }
inline bool RenderStyle::borderRightIsTransparent() const { return border().right().isTransparent(); }
inline BorderStyle RenderStyle::borderRightStyle() const { return border().right().style(); }
inline float RenderStyle::borderRightWidth() const { return border().borderRightWidth(); }
inline const BorderValue& RenderStyle::borderTop() const { return border().top(); }
inline const StyleColor& RenderStyle::borderTopColor() const { return border().top().color(); }
inline bool RenderStyle::borderTopIsTransparent() const { return border().top().isTransparent(); }
inline const LengthSize& RenderStyle::borderTopLeftRadius() const { return border().topLeftRadius(); }
inline const LengthSize& RenderStyle::borderTopRightRadius() const { return border().topRightRadius(); }
inline BorderStyle RenderStyle::borderTopStyle() const { return border().top().style(); }
inline float RenderStyle::borderTopWidth() const { return border().borderTopWidth(); }
inline FloatBoxExtent RenderStyle::borderWidth() const { return border().borderWidth(); }
inline const Length& RenderStyle::bottom() const { return m_nonInheritedData->surroundData->offset.bottom(); }
inline BoxAlignment RenderStyle::boxAlign() const { return static_cast<BoxAlignment>(m_nonInheritedData->miscData->deprecatedFlexibleBox->align); }
inline float RenderStyle::boxFlex() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->flex; }
inline unsigned RenderStyle::boxFlexGroup() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->flexGroup; }
inline BoxLines RenderStyle::boxLines() const { return static_cast<BoxLines>(m_nonInheritedData->miscData->deprecatedFlexibleBox->lines); }
inline unsigned RenderStyle::boxOrdinalGroup() const { return m_nonInheritedData->miscData->deprecatedFlexibleBox->ordinalGroup; }
inline BoxOrient RenderStyle::boxOrient() const { return static_cast<BoxOrient>(m_nonInheritedData->miscData->deprecatedFlexibleBox->orient); }
inline BoxPack RenderStyle::boxPack() const { return static_cast<BoxPack>(m_nonInheritedData->miscData->deprecatedFlexibleBox->pack); }
inline StyleReflection* RenderStyle::boxReflect() const { return m_nonInheritedData->rareData->boxReflect.get(); }
inline const ShadowData* RenderStyle::boxShadow() const { return m_nonInheritedData->miscData->boxShadow.get(); }
inline LayoutBoxExtent RenderStyle::boxShadowExtent() const { return shadowExtent(boxShadow()); }
inline LayoutBoxExtent RenderStyle::boxShadowInsetExtent() const { return shadowInsetExtent(boxShadow()); }
inline BoxSizing RenderStyle::boxSizing() const { return m_nonInheritedData->boxData->boxSizing(); }
inline BoxSizing RenderStyle::boxSizingForAspectRatio() const { return aspectRatioType() == AspectRatioType::AutoAndRatio ? BoxSizing::ContentBox : boxSizing(); }
inline BreakBetween RenderStyle::breakAfter() const { return static_cast<BreakBetween>(m_nonInheritedData->rareData->breakAfter); }
inline BreakBetween RenderStyle::breakBefore() const { return static_cast<BreakBetween>(m_nonInheritedData->rareData->breakBefore); }
inline BreakInside RenderStyle::breakInside() const { return static_cast<BreakInside>(m_nonInheritedData->rareData->breakInside); }
inline LineCap RenderStyle::capStyle() const { return static_cast<LineCap>(m_rareInheritedData->capStyle); }
inline const StyleColor& RenderStyle::caretColor() const { return m_rareInheritedData->caretColor; }
inline const LengthBox& RenderStyle::clip() const { return m_nonInheritedData->rareData->clip; }
inline const Length& RenderStyle::clipBottom() const { return m_nonInheritedData->rareData->clip.bottom(); }
inline const Length& RenderStyle::clipLeft() const { return m_nonInheritedData->rareData->clip.left(); }
inline PathOperation* RenderStyle::clipPath() const { return m_nonInheritedData->rareData->clipPath.get(); }
inline const Length& RenderStyle::clipRight() const { return m_nonInheritedData->rareData->clip.right(); }
inline const Length& RenderStyle::clipTop() const { return m_nonInheritedData->rareData->clip.top(); }
inline bool RenderStyle::collapseWhiteSpace() const { return collapseWhiteSpace(whiteSpace()); }
inline ColumnAxis RenderStyle::columnAxis() const { return static_cast<ColumnAxis>(m_nonInheritedData->miscData->multiCol->axis); }
inline unsigned short RenderStyle::columnCount() const { return m_nonInheritedData->miscData->multiCol->count; }
inline ColumnFill RenderStyle::columnFill() const { return static_cast<ColumnFill>(m_nonInheritedData->miscData->multiCol->fill); }
inline const GapLength& RenderStyle::columnGap() const { return m_nonInheritedData->rareData->columnGap; }
inline ColumnProgression RenderStyle::columnProgression() const { return static_cast<ColumnProgression>(m_nonInheritedData->miscData->multiCol->progression); }
inline const StyleColor& RenderStyle::columnRuleColor() const { return m_nonInheritedData->miscData->multiCol->rule.color(); }
inline bool RenderStyle::columnRuleIsTransparent() const { return m_nonInheritedData->miscData->multiCol->rule.isTransparent(); }
inline BorderStyle RenderStyle::columnRuleStyle() const { return m_nonInheritedData->miscData->multiCol->rule.style(); }
inline unsigned short RenderStyle::columnRuleWidth() const { return m_nonInheritedData->miscData->multiCol->ruleWidth(); }
inline ColumnSpan RenderStyle::columnSpan() const { return static_cast<ColumnSpan>(m_nonInheritedData->miscData->multiCol->columnSpan); }
inline float RenderStyle::columnWidth() const { return m_nonInheritedData->miscData->multiCol->width; }
inline const AtomString& RenderStyle::computedLocale() const { return fontDescription().computedLocale(); }
inline OptionSet<Containment> RenderStyle::contain() const { return m_nonInheritedData->rareData->contain; }
inline std::optional<Length> RenderStyle::containIntrinsicHeight() const { return m_nonInheritedData->rareData->containIntrinsicHeight; }
inline ContainIntrinsicSizeType RenderStyle::containIntrinsicHeightType() const { return static_cast<ContainIntrinsicSizeType>(m_nonInheritedData->rareData->containIntrinsicHeightType); }
inline bool RenderStyle::containIntrinsicHeightHasAuto() const { return containIntrinsicHeightType() == ContainIntrinsicSizeType::AutoAndLength || containIntrinsicHeightType() == ContainIntrinsicSizeType::AutoAndNone; }
inline bool RenderStyle::containIntrinsicLogicalHeightHasAuto() const { return isHorizontalWritingMode() ? containIntrinsicHeightHasAuto() : containIntrinsicWidthHasAuto(); }
inline ContainIntrinsicSizeType RenderStyle::containIntrinsicLogicalHeightType() const { return isHorizontalWritingMode() ? containIntrinsicHeightType() : containIntrinsicWidthType(); }
inline ContainIntrinsicSizeType RenderStyle::containIntrinsicLogicalWidthType() const { return isHorizontalWritingMode() ? containIntrinsicWidthType() : containIntrinsicHeightType(); }
inline bool RenderStyle::containIntrinsicWidthHasAuto() const { return containIntrinsicWidthType() == ContainIntrinsicSizeType::AutoAndLength || containIntrinsicWidthType() == ContainIntrinsicSizeType::AutoAndNone; }
inline bool RenderStyle::containIntrinsicLogicalWidthHasAuto() const { return isHorizontalWritingMode() ? containIntrinsicWidthHasAuto() : containIntrinsicHeightHasAuto(); }
inline std::optional<Length> RenderStyle::containIntrinsicWidth() const { return m_nonInheritedData->rareData->containIntrinsicWidth; }
inline ContainIntrinsicSizeType RenderStyle::containIntrinsicWidthType() const { return static_cast<ContainIntrinsicSizeType>(m_nonInheritedData->rareData->containIntrinsicWidthType); }
inline const Vector<AtomString>& RenderStyle::containerNames() const { return m_nonInheritedData->rareData->containerNames; }
inline ContainerType RenderStyle::containerType() const { return static_cast<ContainerType>(m_nonInheritedData->rareData->containerType); }
inline bool RenderStyle::containsInlineSize() const { return effectiveContainment().contains(Containment::InlineSize); }
inline bool RenderStyle::containsLayout() const { return effectiveContainment().contains(Containment::Layout); }
inline bool RenderStyle::containsLayoutOrPaint() const { return effectiveContainment().containsAny({ Containment::Layout, Containment::Paint }); }
inline bool RenderStyle::containsPaint() const { return effectiveContainment().contains(Containment::Paint); }
inline bool RenderStyle::containsSize() const { return effectiveContainment().contains(Containment::Size); }
inline bool RenderStyle::containsSizeOrInlineSize() const { return effectiveContainment().containsAny({ Containment::Size, Containment::InlineSize }); }
inline bool RenderStyle::containsStyle() const { return effectiveContainment().contains(Containment::Style); }
constexpr OptionSet<Containment> RenderStyle::contentContainment() { return { Containment::Layout, Containment::Paint, Containment::Style }; }
inline const ContentData* RenderStyle::contentData() const { return m_nonInheritedData->miscData->content.get(); }
inline bool RenderStyle::contentDataEquivalent(const RenderStyle* otherStyle) const { return m_nonInheritedData->miscData->contentDataEquivalent(*otherStyle->m_nonInheritedData->miscData); }
inline ContentVisibility RenderStyle::contentVisibility() const { return static_cast<ContentVisibility>(m_nonInheritedData->rareData->contentVisibility); }
inline CursorList* RenderStyle::cursors() const { return m_rareInheritedData->cursorData.get(); }
inline StyleAppearance RenderStyle::effectiveAppearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->effectiveAppearance); }
inline OptionSet<Containment> RenderStyle::effectiveContainment() const { return m_nonInheritedData->rareData->effectiveContainment(); }
inline bool RenderStyle::effectiveInert() const { return m_rareInheritedData->effectiveInert; }
inline PointerEvents RenderStyle::effectivePointerEvents() const { return effectiveInert() ? PointerEvents::None : pointerEvents(); }
inline bool RenderStyle::effectiveSkippedContent() const { return m_rareInheritedData->effectiveSkippedContent; }
inline CSSPropertyID RenderStyle::effectiveStrokeColorProperty() const { return hasExplicitlySetStrokeColor() ? CSSPropertyStrokeColor : CSSPropertyWebkitTextStrokeColor; }
inline OptionSet<TouchAction> RenderStyle::effectiveTouchActions() const { return m_rareInheritedData->effectiveTouchActions; }
inline UserModify RenderStyle::effectiveUserModify() const { return effectiveInert() ? UserModify::ReadOnly : userModify(); }
inline float RenderStyle::effectiveZoom() const { return m_rareInheritedData->effectiveZoom; }
inline OptionSet<EventListenerRegionType> RenderStyle::eventListenerRegionTypes() const { return m_rareInheritedData->eventListenerRegionTypes; }
inline const FilterOperations& RenderStyle::filter() const { return m_nonInheritedData->miscData->filter->operations; }
inline IntOutsets RenderStyle::filterOutsets() const { return hasFilter() ? filter().outsets() : IntOutsets(); }
inline const Length& RenderStyle::flexBasis() const { return m_nonInheritedData->miscData->flexibleBox->flexBasis; }
inline FlexDirection RenderStyle::flexDirection() const { return static_cast<FlexDirection>(m_nonInheritedData->miscData->flexibleBox->flexDirection); }
inline float RenderStyle::flexGrow() const { return m_nonInheritedData->miscData->flexibleBox->flexGrow; }
inline float RenderStyle::flexShrink() const { return m_nonInheritedData->miscData->flexibleBox->flexShrink; }
inline FlexWrap RenderStyle::flexWrap() const { return static_cast<FlexWrap>(m_nonInheritedData->miscData->flexibleBox->flexWrap); }
inline std::optional<FontSelectionValue> RenderStyle::fontItalic() const { return fontDescription().italic(); }
inline FontPalette RenderStyle::fontPalette() const { return fontDescription().fontPalette(); }
inline FontSizeAdjust RenderStyle::fontSizeAdjust() const { return fontDescription().fontSizeAdjust(); }
inline FontSelectionValue RenderStyle::fontStretch() const { return fontDescription().stretch(); }
inline FontVariationSettings RenderStyle::fontVariationSettings() const { return fontDescription().variationSettings(); }
inline FontSelectionValue RenderStyle::fontWeight() const { return fontDescription().weight(); }
inline void RenderStyle::getBoxShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const { getShadowBlockDirectionExtent(boxShadow(), logicalTop, logicalBottom); }
inline void RenderStyle::getBoxShadowHorizontalExtent(LayoutUnit& left, LayoutUnit& right) const { getShadowHorizontalExtent(boxShadow(), left, right); }
inline void RenderStyle::getBoxShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const { getShadowInlineDirectionExtent(boxShadow(), logicalLeft, logicalRight); }
inline void RenderStyle::getBoxShadowVerticalExtent(LayoutUnit& top, LayoutUnit& bottom) const { getShadowVerticalExtent(boxShadow(), top, bottom); }
inline void RenderStyle::getTextShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const { getShadowBlockDirectionExtent(textShadow(), logicalTop, logicalBottom); }
inline void RenderStyle::getTextShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const { getShadowInlineDirectionExtent(textShadow(), logicalLeft, logicalRight); }
inline const Vector<GridTrackSize>& RenderStyle::gridAutoColumns() const { return m_nonInheritedData->rareData->grid->gridAutoColumns; }
inline GridAutoFlow RenderStyle::gridAutoFlow() const { return static_cast<GridAutoFlow>(m_nonInheritedData->rareData->grid->gridAutoFlow); }
inline const Vector<GridTrackSize>& RenderStyle::gridAutoRepeatColumns() const { return m_nonInheritedData->rareData->grid->gridAutoRepeatColumns(); }
inline unsigned RenderStyle::gridAutoRepeatColumnsInsertionPoint() const { return m_nonInheritedData->rareData->grid->autoRepeatColumnsInsertionPoint(); }
inline AutoRepeatType RenderStyle::gridAutoRepeatColumnsType() const  { return m_nonInheritedData->rareData->grid->autoRepeatColumnsType(); }
inline const Vector<GridTrackSize>& RenderStyle::gridAutoRepeatRows() const { return m_nonInheritedData->rareData->grid->gridAutoRepeatRows(); }
inline unsigned RenderStyle::gridAutoRepeatRowsInsertionPoint() const { return m_nonInheritedData->rareData->grid->autoRepeatRowsInsertionPoint(); }
inline AutoRepeatType RenderStyle::gridAutoRepeatRowsType() const  { return m_nonInheritedData->rareData->grid->autoRepeatRowsType(); }
inline const Vector<GridTrackSize>& RenderStyle::gridAutoRows() const { return m_nonInheritedData->rareData->grid->gridAutoRows; }
inline const GridTrackList& RenderStyle::gridColumnList() const { return m_nonInheritedData->rareData->grid->columns(); }
inline const Vector<GridTrackSize>& RenderStyle::gridColumnTrackSizes() const { return m_nonInheritedData->rareData->grid->gridColumnTrackSizes(); }
inline const GridPosition& RenderStyle::gridItemColumnEnd() const { return m_nonInheritedData->rareData->gridItem->gridColumnEnd; }
inline const GridPosition& RenderStyle::gridItemColumnStart() const { return m_nonInheritedData->rareData->gridItem->gridColumnStart; }
inline const GridPosition& RenderStyle::gridItemRowEnd() const { return m_nonInheritedData->rareData->gridItem->gridRowEnd; }
inline const GridPosition& RenderStyle::gridItemRowStart() const { return m_nonInheritedData->rareData->gridItem->gridRowStart; }
inline bool RenderStyle::gridMasonryColumns() const { return m_nonInheritedData->rareData->grid->masonryColumns(); }
inline bool RenderStyle::gridMasonryRows() const { return m_nonInheritedData->rareData->grid->masonryRows(); }
inline const GridTrackList& RenderStyle::gridRowList() const { return m_nonInheritedData->rareData->grid->rows(); }
inline const Vector<GridTrackSize>& RenderStyle::gridRowTrackSizes() const { return m_nonInheritedData->rareData->grid->gridRowTrackSizes(); }
inline bool RenderStyle::gridSubgridColumns() const { return m_nonInheritedData->rareData->grid->subgridColumns(); }
inline bool RenderStyle::gridSubgridRows() const { return m_nonInheritedData->rareData->grid->subgridRows(); }
inline OptionSet<HangingPunctuation> RenderStyle::hangingPunctuation() const { return OptionSet<HangingPunctuation>::fromRaw(m_rareInheritedData->hangingPunctuation); }
inline bool RenderStyle::hasAnimations() const { return animations() && animations()->size(); }
inline bool RenderStyle::hasAnimationsOrTransitions() const { return hasAnimations() || hasTransitions(); }
inline bool RenderStyle::hasAnyFixedBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::FixedBackground); }
inline bool RenderStyle::hasAnyLocalBackground() const { return backgroundLayers().hasImageWithAttachment(FillAttachment::LocalBackground); }
inline bool RenderStyle::hasAnyPublicPseudoStyles() const { return m_nonInheritedFlags.hasAnyPublicPseudoStyles(); }
inline bool RenderStyle::hasAppearance() const { return appearance() != StyleAppearance::None; }
inline bool RenderStyle::hasAppleColorFilter() const { return !appleColorFilter().operations().isEmpty(); }
inline bool RenderStyle::hasAspectRatio() const { return aspectRatioType() == AspectRatioType::Ratio || aspectRatioType() == AspectRatioType::AutoAndRatio; }
inline bool RenderStyle::hasAttrContent() const { return m_nonInheritedData->miscData->hasAttrContent; }
inline bool RenderStyle::hasAutoAccentColor() const { return m_rareInheritedData->hasAutoAccentColor; }
inline bool RenderStyle::hasAutoCaretColor() const { return m_rareInheritedData->hasAutoCaretColor; }
inline bool RenderStyle::hasAutoColumnCount() const { return m_nonInheritedData->miscData->multiCol->autoCount; }
inline bool RenderStyle::hasAutoColumnWidth() const { return m_nonInheritedData->miscData->multiCol->autoWidth; }
inline bool RenderStyle::hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
inline bool RenderStyle::hasAutoLengthContainIntrinsicSize() const { return containIntrinsicWidthHasAuto() || containIntrinsicHeightHasAuto(); }
inline bool RenderStyle::hasAutoOrphans() const { return m_rareInheritedData->hasAutoOrphans; }
inline bool RenderStyle::hasAutoSpecifiedZIndex() const { return m_nonInheritedData->boxData->hasAutoSpecifiedZIndex(); }
inline bool RenderStyle::hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }
inline bool RenderStyle::hasAutoUsedZIndex() const { return m_nonInheritedData->boxData->hasAutoUsedZIndex(); }
inline bool RenderStyle::hasAutoWidows() const { return m_rareInheritedData->hasAutoWidows; }
inline bool RenderStyle::hasBackground() const { return visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || hasBackgroundImage(); }
inline bool RenderStyle::hasBackgroundImage() const { return backgroundLayers().hasImage(); }
inline bool RenderStyle::hasBlendMode() const { return blendMode() != BlendMode::Normal; }
inline bool RenderStyle::hasBorder() const { return border().hasBorder(); }
inline bool RenderStyle::hasBorderImage() const { return border().hasBorderImage(); }
inline bool RenderStyle::hasBorderImageOutsets() const { return borderImage().hasImage() && !borderImage().outset().isZero(); }
inline bool RenderStyle::hasBorderRadius() const { return border().hasBorderRadius(); }
inline bool RenderStyle::hasClip() const { return m_nonInheritedData->rareData->hasClip; }
inline bool RenderStyle::hasContent() const { return contentData(); }
inline bool RenderStyle::hasEffectiveAppearance() const { return effectiveAppearance() != StyleAppearance::None; }
inline bool RenderStyle::hasEffectiveContentNone() const { return !contentData() && (m_nonInheritedFlags.hasContentNone || styleType() == PseudoId::Before || styleType() == PseudoId::After); }
inline bool RenderStyle::hasExplicitlySetBorderBottomLeftRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderBottomLeftRadius; }
inline bool RenderStyle::hasExplicitlySetBorderBottomRightRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderBottomRightRadius; }
inline bool RenderStyle::hasExplicitlySetBorderRadius() const { return hasExplicitlySetBorderBottomLeftRadius() || hasExplicitlySetBorderBottomRightRadius() || hasExplicitlySetBorderTopLeftRadius() || hasExplicitlySetBorderTopRightRadius(); }
inline bool RenderStyle::hasExplicitlySetBorderTopLeftRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderTopLeftRadius; }
inline bool RenderStyle::hasExplicitlySetBorderTopRightRadius() const { return m_nonInheritedData->surroundData->hasExplicitlySetBorderTopRightRadius; }
inline bool RenderStyle::hasExplicitlySetStrokeColor() const { return m_rareInheritedData->hasSetStrokeColor; }
inline bool RenderStyle::hasFilter() const { return !filter().operations().isEmpty(); }
inline bool RenderStyle::hasInFlowPosition() const { return position() == PositionType::Relative || position() == PositionType::Sticky; }
inline bool RenderStyle::hasIsolation() const { return isolation() != Isolation::Auto; }
inline bool RenderStyle::hasMargin() const { return !m_nonInheritedData->surroundData->margin.isZero(); }
inline bool RenderStyle::hasMask() const { return maskLayers().hasImage() || maskBoxImage().hasImage(); }
inline bool RenderStyle::hasOffset() const { return !m_nonInheritedData->surroundData->offset.isZero(); }
inline bool RenderStyle::hasOpacity() const { return m_nonInheritedData->miscData->hasOpacity(); }
inline bool RenderStyle::hasOutOfFlowPosition() const { return position() == PositionType::Absolute || position() == PositionType::Fixed; }
inline bool RenderStyle::hasOutline() const { return outlineStyle() > BorderStyle::Hidden && outlineWidth() > 0; }
inline bool RenderStyle::hasOutlineInVisualOverflow() const { return hasOutline() && outlineSize() > 0; }
inline bool RenderStyle::hasPadding() const { return !paddingBox().isZero(); }
inline bool RenderStyle::hasPerspective() const { return perspective() != initialPerspective(); }
inline bool RenderStyle::hasPositionedMask() const { return maskLayers().hasImage(); }
inline bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const { return m_nonInheritedFlags.hasPseudoStyle(pseudo); }
inline bool RenderStyle::hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }
inline bool RenderStyle::hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
inline bool RenderStyle::hasTextCombine() const { return textCombine() != TextCombine::None; }
inline bool RenderStyle::hasTransform() const { return !transform().operations().isEmpty() || offsetPath(); }
inline bool RenderStyle::hasTransformRelatedProperty() const { return hasTransform() || translate() || scale() || rotate() || transformStyle3D() == TransformStyle3D::Preserve3D || hasPerspective(); }
inline bool RenderStyle::hasTransitions() const { return transitions() && transitions()->size(); }
inline bool RenderStyle::hasViewportConstrainedPosition() const { return position() == PositionType::Fixed || position() == PositionType::Sticky; }
inline bool RenderStyle::hasVisibleBorder() const { return border().hasVisibleBorder(); }
inline bool RenderStyle::hasVisibleBorderDecoration() const { return hasVisibleBorder() || hasBorderImage(); }
inline bool RenderStyle::hasVisitedLinkAutoCaretColor() const { return m_rareInheritedData->hasVisitedLinkAutoCaretColor; }
inline const Length& RenderStyle::height() const { return m_nonInheritedData->boxData->height(); }
inline short RenderStyle::hyphenationLimitAfter() const { return m_rareInheritedData->hyphenationLimitAfter; }
inline short RenderStyle::hyphenationLimitBefore() const { return m_rareInheritedData->hyphenationLimitBefore; }
inline short RenderStyle::hyphenationLimitLines() const { return m_rareInheritedData->hyphenationLimitLines; }
inline const AtomString& RenderStyle::hyphenationString() const { return m_rareInheritedData->hyphenationString; }
inline Hyphens RenderStyle::hyphens() const { return static_cast<Hyphens>(m_rareInheritedData->hyphens); }
inline ImageOrientation RenderStyle::imageOrientation() const { return static_cast<ImageOrientation::Orientation>(m_rareInheritedData->imageOrientation); }
inline ImageRendering RenderStyle::imageRendering() const { return static_cast<ImageRendering>(m_rareInheritedData->imageRendering); }
inline const NamedGridLinesMap& RenderStyle::implicitNamedGridColumnLines() const { return m_nonInheritedData->rareData->grid->implicitNamedGridColumnLines; }
inline const NamedGridLinesMap& RenderStyle::implicitNamedGridRowLines() const { return m_nonInheritedData->rareData->grid->implicitNamedGridRowLines; }
constexpr auto RenderStyle::individualTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }
inline const StyleCustomPropertyData& RenderStyle::inheritedCustomProperties() const { return m_rareInheritedData->customProperties.get(); }
inline Vector<StyleContentAlignmentData> RenderStyle::initialAlignTracks() { return { initialContentAlignment() }; }
constexpr StyleAppearance RenderStyle::initialAppearance() { return StyleAppearance::None; }
inline FilterOperations RenderStyle::initialAppleColorFilter() { return { }; }
constexpr AspectRatioType RenderStyle::initialAspectRatioType() { return AspectRatioType::Auto; }
constexpr BackfaceVisibility RenderStyle::initialBackfaceVisibility() { return BackfaceVisibility::Visible; }
inline StyleColor RenderStyle::initialBackgroundColor() { return Color::transparentBlack; }
constexpr BlockStepInsert RenderStyle::initialBlockStepInsert() { return BlockStepInsert::Margin; }
inline std::optional<Length> RenderStyle::initialBlockStepSize() { return std::nullopt; }
constexpr BorderCollapse RenderStyle::initialBorderCollapse() { return BorderCollapse::Separate; }
inline LengthSize RenderStyle::initialBorderRadius() { return { zeroLength(), zeroLength() }; }
constexpr BorderStyle RenderStyle::initialBorderStyle() { return BorderStyle::None; }
constexpr BoxAlignment RenderStyle::initialBoxAlign() { return BoxAlignment::Stretch; }
constexpr BoxDecorationBreak RenderStyle::initialBoxDecorationBreak() { return BoxDecorationBreak::Slice; }
constexpr BoxDirection RenderStyle::initialBoxDirection() { return BoxDirection::Normal; }
constexpr BoxLines RenderStyle::initialBoxLines() { return BoxLines::Single; }
constexpr BoxOrient RenderStyle::initialBoxOrient() { return BoxOrient::Horizontal; }
constexpr BoxPack RenderStyle::initialBoxPack() { return BoxPack::Start; }
constexpr BoxSizing RenderStyle::initialBoxSizing() { return BoxSizing::ContentBox; }
constexpr BreakBetween RenderStyle::initialBreakBetween() { return BreakBetween::Auto; }
constexpr BreakInside RenderStyle::initialBreakInside() { return BreakInside::Auto; }
constexpr LineCap RenderStyle::initialCapStyle() { return LineCap::Butt; }
constexpr CaptionSide RenderStyle::initialCaptionSide() { return CaptionSide::Top; }
constexpr Clear RenderStyle::initialClear() { return Clear::None; }
inline Color RenderStyle::initialColor() { return Color::black; }
constexpr ColumnAxis RenderStyle::initialColumnAxis() { return ColumnAxis::Auto; }
constexpr ColumnFill RenderStyle::initialColumnFill() { return ColumnFill::Balance; }
inline GapLength RenderStyle::initialColumnGap() { return { }; }
constexpr ColumnProgression RenderStyle::initialColumnProgression() { return ColumnProgression::Normal; }
constexpr ColumnSpan RenderStyle::initialColumnSpan() { return ColumnSpan::None; }
inline std::optional<Length> RenderStyle::initialContainIntrinsicHeight() { return std::nullopt; }
constexpr ContainIntrinsicSizeType RenderStyle::initialContainIntrinsicHeightType() { return ContainIntrinsicSizeType::None; }
inline std::optional<Length> RenderStyle::initialContainIntrinsicWidth() { return std::nullopt; }
constexpr ContainIntrinsicSizeType RenderStyle::initialContainIntrinsicWidthType() { return ContainIntrinsicSizeType::None; }
inline Vector<AtomString> RenderStyle::initialContainerNames() { return { }; }
constexpr ContainerType RenderStyle::initialContainerType() { return ContainerType::Normal; }
constexpr OptionSet<Containment> RenderStyle::initialContainment() { return { }; }
constexpr StyleContentAlignmentData RenderStyle::initialContentAlignment() { return { }; }
inline const AtomString& RenderStyle::initialContentAltText() { return emptyAtom(); }
constexpr ContentVisibility RenderStyle::initialContentVisibility() { return ContentVisibility::Visible; }
constexpr CursorType RenderStyle::initialCursor() { return CursorType::Auto; }
constexpr StyleSelfAlignmentData RenderStyle::initialDefaultAlignment() { return { ItemPosition::Normal, OverflowAlignment::Default }; }
constexpr TextDirection RenderStyle::initialDirection() { return TextDirection::LTR; }
constexpr DisplayType RenderStyle::initialDisplay() { return DisplayType::Inline; }
constexpr EmptyCell RenderStyle::initialEmptyCells() { return EmptyCell::Show; }
inline FilterOperations RenderStyle::initialFilter() { return { }; }
inline Length RenderStyle::initialFlexBasis() { return LengthType::Auto; }
constexpr FlexDirection RenderStyle::initialFlexDirection() { return FlexDirection::Row; }
constexpr FlexWrap RenderStyle::initialFlexWrap() { return FlexWrap::NoWrap; }
constexpr Float RenderStyle::initialFloating() { return Float::None; }
inline Vector<GridTrackSize> RenderStyle::initialGridAutoColumns() { return { GridTrackSize { Length { } } }; }
constexpr GridAutoFlow RenderStyle::initialGridAutoFlow() { return AutoFlowRow; }
inline Vector<GridTrackSize> RenderStyle::initialGridAutoRepeatTracks() { return { }; }
constexpr AutoRepeatType RenderStyle::initialGridAutoRepeatType() { return AutoRepeatType::None; }
inline Vector<GridTrackSize> RenderStyle::initialGridAutoRows() { return { GridTrackSize { Length { } } }; }
inline Vector<GridTrackSize> RenderStyle::initialGridColumnTrackSizes() { return { }; }
inline GridPosition RenderStyle::initialGridItemColumnEnd() { return { }; }
inline GridPosition RenderStyle::initialGridItemColumnStart() { return { }; }
inline GridPosition RenderStyle::initialGridItemRowEnd() { return { }; }
inline GridPosition RenderStyle::initialGridItemRowStart() { return { }; }
inline Vector<GridTrackSize> RenderStyle::initialGridRowTrackSizes() { return { }; }
constexpr OptionSet<HangingPunctuation> RenderStyle::initialHangingPunctuation() { return { }; }
inline const AtomString& RenderStyle::initialHyphenationString() { return nullAtom(); }
constexpr Hyphens RenderStyle::initialHyphens() { return Hyphens::Manual; }
constexpr ImageOrientation RenderStyle::initialImageOrientation() { return ImageOrientation::Orientation::FromImage; }
constexpr ImageRendering RenderStyle::initialImageRendering() { return ImageRendering::Auto; }
constexpr ImageResolutionSnap RenderStyle::initialImageResolutionSnap() { return ImageResolutionSnap::None; }
constexpr ImageResolutionSource RenderStyle::initialImageResolutionSource() { return ImageResolutionSource::Specified; }
constexpr IntSize RenderStyle::initialInitialLetter() { return { }; }
constexpr InputSecurity RenderStyle::initialInputSecurity() { return InputSecurity::Auto; }
constexpr LineJoin RenderStyle::initialJoinStyle() { return LineJoin::Miter; }
constexpr StyleSelfAlignmentData RenderStyle::initialJustifyItems() { return { ItemPosition::Legacy }; }
inline Vector<StyleContentAlignmentData> RenderStyle::initialJustifyTracks() { return { initialContentAlignment() }; }
inline const IntSize& RenderStyle::initialLetter() const { return m_nonInheritedData->rareData->initialLetter; }
inline int RenderStyle::initialLetterDrop() const { return initialLetter().width(); }
inline int RenderStyle::initialLetterHeight() const { return initialLetter().height(); }
constexpr LineAlign RenderStyle::initialLineAlign() { return LineAlign::None; }
constexpr OptionSet<LineBoxContain> RenderStyle::initialLineBoxContain() { return { LineBoxContain::Block, LineBoxContain::Inline, LineBoxContain::Replaced }; }
constexpr LineBreak RenderStyle::initialLineBreak() { return LineBreak::Auto; }
constexpr LineClampValue RenderStyle::initialLineClamp() { return { }; }
inline const AtomString& RenderStyle::initialLineGrid() { return nullAtom(); }
constexpr LineSnap RenderStyle::initialLineSnap() { return LineSnap::None; }
constexpr ListStylePosition RenderStyle::initialListStylePosition() { return ListStylePosition::Outside; }
inline ListStyleType RenderStyle::initialListStyleType() { return { ListStyleType::Type::CounterStyle, nameString(CSSValueDisc) }; }
inline Length RenderStyle::initialMargin() { return zeroLength(); }
constexpr OptionSet<MarginTrimType> RenderStyle::initialMarginTrim() { return { }; }
constexpr MarqueeBehavior RenderStyle::initialMarqueeBehavior() { return MarqueeBehavior::Scroll; }
constexpr MarqueeDirection RenderStyle::initialMarqueeDirection() { return MarqueeDirection::Auto; }
inline Length RenderStyle::initialMarqueeIncrement() { return { 6, LengthType::Fixed }; }
constexpr MasonryAutoFlow RenderStyle::initialMasonryAutoFlow() { return { MasonryAutoFlowPlacementAlgorithm::Pack, MasonryAutoFlowPlacementOrder::DefiniteFirst }; }
constexpr MathStyle RenderStyle::initialMathStyle() { return MathStyle::Normal; }
inline Length RenderStyle::initialMaxSize() { return LengthType::Undefined; }
inline Length RenderStyle::initialMinSize() { return LengthType::Auto; }
constexpr NBSPMode RenderStyle::initialNBSPMode() { return NBSPMode::Normal; }
inline NamedGridAreaMap RenderStyle::initialNamedGridArea() { return { }; }
inline NamedGridLinesMap RenderStyle::initialNamedGridColumnLines() { return { }; }
inline NamedGridLinesMap RenderStyle::initialNamedGridRowLines() { return { }; }
constexpr ObjectFit RenderStyle::initialObjectFit() { return ObjectFit::Fill; }
inline LengthPoint RenderStyle::initialObjectPosition() { return { { 50.0f, LengthType::Percent }, { 50.0f, LengthType::Percent } }; }
inline Length RenderStyle::initialOffset() { return LengthType::Auto; }
inline LengthPoint RenderStyle::initialOffsetAnchor() { return { }; }
inline Length RenderStyle::initialOffsetDistance() { return zeroLength(); }
inline LengthPoint RenderStyle::initialOffsetPosition() { return { }; }
constexpr OffsetRotation RenderStyle::initialOffsetRotate() { return { true }; }
inline OrderedNamedGridLinesMap RenderStyle::initialOrderedNamedGridColumnLines() { return { }; }
inline OrderedNamedGridLinesMap RenderStyle::initialOrderedNamedGridRowLines() { return { }; }
constexpr OutlineIsAuto RenderStyle::initialOutlineStyleIsAuto() { return OutlineIsAuto::Off; }
constexpr OverflowAnchor RenderStyle::initialOverflowAnchor() { return OverflowAnchor::Auto; }
constexpr OverflowWrap RenderStyle::initialOverflowWrap() { return OverflowWrap::Normal; }
constexpr Overflow RenderStyle::initialOverflowX() { return Overflow::Visible; }
constexpr Overflow RenderStyle::initialOverflowY() { return Overflow::Visible; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorX() { return OverscrollBehavior::Auto; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorY() { return OverscrollBehavior::Auto; }
inline Length RenderStyle::initialPadding() { return zeroLength(); }
constexpr PaintOrder RenderStyle::initialPaintOrder() { return PaintOrder::Normal; }
inline Length RenderStyle::initialPerspectiveOriginX() { return { 50.0f, LengthType::Percent }; }
inline Length RenderStyle::initialPerspectiveOriginY() { return { 50.0f, LengthType::Percent }; }
constexpr PointerEvents RenderStyle::initialPointerEvents() { return PointerEvents::Auto; }
constexpr PositionType RenderStyle::initialPosition() { return PositionType::Static; }
constexpr PrintColorAdjust RenderStyle::initialPrintColorAdjust() { return PrintColorAdjust::Economy; }
constexpr Order RenderStyle::initialRTLOrdering() { return Order::Logical; }
inline Length RenderStyle::initialRadius() { return LengthType::Auto; }
constexpr Resize RenderStyle::initialResize() { return Resize::None; }
inline GapLength RenderStyle::initialRowGap() { return { }; }
constexpr RubyPosition RenderStyle::initialRubyPosition() { return RubyPosition::Before; }
inline Length RenderStyle::initialScrollMargin() { return zeroLength(); }
inline Length RenderStyle::initialScrollPadding() { return { }; }
inline std::optional<ScrollbarColor> RenderStyle::initialScrollbarColor() { return std::nullopt; }
constexpr StyleSelfAlignmentData RenderStyle::initialSelfAlignment() { return { ItemPosition::Auto, OverflowAlignment::Default }; }
inline Length RenderStyle::initialShapeMargin() { return zeroLength(); }
inline Length RenderStyle::initialSize() { return LengthType::Auto; }
constexpr OptionSet<SpeakAs> RenderStyle::initialSpeakAs() { return { }; }
inline StyleColor RenderStyle::initialStrokeColor() { return { Color::transparentBlack }; }
constexpr float RenderStyle::initialStrokeMiterLimit() { return defaultMiterLimit; }
inline Length RenderStyle::initialStrokeWidth() { return oneLength(); }
constexpr TabSize RenderStyle::initialTabSize() { return 8; }
constexpr TableLayoutType RenderStyle::initialTableLayout() { return TableLayoutType::Auto; }
constexpr TextAlignMode RenderStyle::initialTextAlign() { return TextAlignMode::Start; }
constexpr TextAlignLast RenderStyle::initialTextAlignLast() { return TextAlignLast::Auto; }
constexpr TextAutospace RenderStyle::initialTextAutospace() { return { }; }
constexpr TextBoxTrim RenderStyle::initialTextBoxTrim() { return TextBoxTrim::None; }
constexpr TextCombine RenderStyle::initialTextCombine() { return TextCombine::None; }
inline StyleColor RenderStyle::initialTextDecorationColor() { return StyleColor::currentColor(); }
constexpr OptionSet<TextDecorationLine> RenderStyle::initialTextDecorationLine() { return { }; }
constexpr TextDecorationSkipInk RenderStyle::initialTextDecorationSkipInk() { return TextDecorationSkipInk::Auto; }
constexpr TextDecorationStyle RenderStyle::initialTextDecorationStyle() { return TextDecorationStyle::Solid; }
constexpr TextDecorationThickness RenderStyle::initialTextDecorationThickness() { return TextDecorationThickness::createWithAuto(); }
inline StyleColor RenderStyle::initialTextEmphasisColor() { return StyleColor::currentColor(); }
inline const AtomString& RenderStyle::initialTextEmphasisCustomMark() { return nullAtom(); }
constexpr TextEmphasisFill RenderStyle::initialTextEmphasisFill() { return TextEmphasisFill::Filled; }
constexpr TextEmphasisMark RenderStyle::initialTextEmphasisMark() { return TextEmphasisMark::None; }
constexpr OptionSet<TextEmphasisPosition> RenderStyle::initialTextEmphasisPosition() { return { TextEmphasisPosition::Over, TextEmphasisPosition::Right }; }
inline StyleColor RenderStyle::initialTextFillColor() { return StyleColor::currentColor(); }
inline bool RenderStyle::hasExplicitlySetColor() const { return m_inheritedFlags.hasExplicitlySetColor; }
constexpr TextGroupAlign RenderStyle::initialTextGroupAlign() { return TextGroupAlign::None; }
inline Length RenderStyle::initialTextIndent() { return zeroLength(); }
constexpr TextIndentLine RenderStyle::initialTextIndentLine() { return TextIndentLine::FirstLine; }
constexpr TextIndentType RenderStyle::initialTextIndentType() { return TextIndentType::Normal; }
constexpr TextJustify RenderStyle::initialTextJustify() { return TextJustify::Auto; }
constexpr TextOrientation RenderStyle::initialTextOrientation() { return TextOrientation::Mixed; }
constexpr TextOverflow RenderStyle::initialTextOverflow() { return TextOverflow::Clip; }
constexpr TextSecurity RenderStyle::initialTextSecurity() { return TextSecurity::None; }
constexpr TextSpacingTrim RenderStyle::initialTextSpacingTrim() { return { }; }
inline StyleColor RenderStyle::initialTextStrokeColor() { return StyleColor::currentColor(); }
constexpr OptionSet<TextTransform> RenderStyle::initialTextTransform() { return { }; }
constexpr TextUnderlineOffset RenderStyle::initialTextUnderlineOffset() { return TextUnderlineOffset::createWithAuto(); }
constexpr TextUnderlinePosition RenderStyle::initialTextUnderlinePosition() { return TextUnderlinePosition::Auto; }
constexpr TextWrap RenderStyle::initialTextWrap() { return TextWrap::Wrap; }
constexpr TextZoom RenderStyle::initialTextZoom() { return TextZoom::Normal; }
constexpr TouchAction RenderStyle::initialTouchActions() { return TouchAction::Auto; }
inline TransformOperations RenderStyle::initialTransform() { return { }; }
constexpr TransformBox RenderStyle::initialTransformBox() { return TransformBox::ViewBox; }
inline Length RenderStyle::initialTransformOriginX() { return { 50.0f, LengthType::Percent }; }
inline Length RenderStyle::initialTransformOriginY() { return { 50.0f, LengthType::Percent }; }
constexpr TransformStyle3D RenderStyle::initialTransformStyle3D() { return TransformStyle3D::Flat; }
constexpr UnicodeBidi RenderStyle::initialUnicodeBidi() { return UnicodeBidi::Normal; }
constexpr UserDrag RenderStyle::initialUserDrag() { return UserDrag::Auto; }
constexpr UserModify RenderStyle::initialUserModify() { return UserModify::ReadOnly; }
constexpr UserSelect RenderStyle::initialUserSelect() { return UserSelect::Text; }
constexpr VerticalAlign RenderStyle::initialVerticalAlign() { return VerticalAlign::Baseline; }
constexpr Visibility RenderStyle::initialVisibility() { return Visibility::Visible; }
constexpr WhiteSpace RenderStyle::initialWhiteSpace() { return WhiteSpace::Normal; }
constexpr WhiteSpaceCollapse RenderStyle::initialWhiteSpaceCollapse() { return WhiteSpaceCollapse::Collapse; }
constexpr WordBreak RenderStyle::initialWordBreak() { return WordBreak::Normal; }
inline Length RenderStyle::initialWordSpacing() { return zeroLength(); }
constexpr WritingMode RenderStyle::initialWritingMode() { return WritingMode::TopToBottom; }
inline InputSecurity RenderStyle::inputSecurity() const { return static_cast<InputSecurity>(m_nonInheritedData->rareData->inputSecurity); }
inline bool RenderStyle::isColumnFlexDirection() const { return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse; }
constexpr bool RenderStyle::isDisplayBlockLevel() const { return isDisplayBlockType(display()); }
constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox(DisplayType display) { return display == DisplayType::Box || display == DisplayType::InlineBox; }
constexpr bool RenderStyle::isDisplayFlexibleBox(DisplayType display) { return display == DisplayType::Flex || display == DisplayType::InlineFlex; }
constexpr bool RenderStyle::isDisplayFlexibleBoxIncludingDeprecatedOrGridBox() const { return isDisplayFlexibleOrGridBox() || isDisplayDeprecatedFlexibleBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridBox() const { return isDisplayFlexibleOrGridBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridBox(DisplayType display) { return isDisplayFlexibleBox(display) || isDisplayGridBox(display); }
constexpr bool RenderStyle::isDisplayGridBox(DisplayType display) { return display == DisplayType::Grid || display == DisplayType::InlineGrid; }
constexpr bool RenderStyle::isDisplayInlineType() const { return isDisplayInlineType(display()); }
constexpr bool RenderStyle::isDisplayListItemType(DisplayType display) { return display == DisplayType::ListItem; }
constexpr bool RenderStyle::isDisplayTableOrTablePart() const { return isDisplayTableOrTablePart(display()); }
inline bool RenderStyle::isFixedTableLayout() const { return tableLayout() == TableLayoutType::Fixed && (logicalWidth().isSpecified() || logicalWidth().isFitContent() || logicalWidth().isMinContent()); }
inline bool RenderStyle::isFlippedBlocksWritingMode() const { return WebCore::isFlippedWritingMode(writingMode()); }
inline bool RenderStyle::isFlippedLinesWritingMode() const { return WebCore::isFlippedLinesWritingMode(writingMode()); }
inline bool RenderStyle::isFloating() const { return floating() != Float::None; }
inline bool RenderStyle::isGridAutoFlowAlgorithmDense() const { return m_nonInheritedData->rareData->grid->gridAutoFlow & InternalAutoFlowAlgorithmDense; }
inline bool RenderStyle::isGridAutoFlowAlgorithmSparse() const { return m_nonInheritedData->rareData->grid->gridAutoFlow & InternalAutoFlowAlgorithmSparse; }
inline bool RenderStyle::isGridAutoFlowDirectionColumn() const { return m_nonInheritedData->rareData->grid->gridAutoFlow & InternalAutoFlowDirectionColumn; }
inline bool RenderStyle::isGridAutoFlowDirectionRow() const { return m_nonInheritedData->rareData->grid->gridAutoFlow & InternalAutoFlowDirectionRow; }
inline bool RenderStyle::isHorizontalWritingMode() const { return WebCore::isHorizontalWritingMode(writingMode()); }
inline bool RenderStyle::isLeftToRightDirection() const { return direction() == TextDirection::LTR; }
constexpr bool RenderStyle::isOriginalDisplayBlockType() const { return isDisplayBlockType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayListItemType() const { return isDisplayListItemType(originalDisplay()); }
inline bool RenderStyle::isOverflowVisible() const { return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible; }
inline bool RenderStyle::isReverseFlexDirection() const { return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse; }
inline bool RenderStyle::isVerticalWritingMode() const { return WebCore::isVerticalWritingMode(writingMode()); }
inline LineJoin RenderStyle::joinStyle() const { return static_cast<LineJoin>(m_rareInheritedData->joinStyle); }
inline const StyleContentAlignmentData& RenderStyle::justifyContent() const { return m_nonInheritedData->miscData->justifyContent; }
inline const StyleSelfAlignmentData& RenderStyle::justifyItems() const { return m_nonInheritedData->miscData->justifyItems; }
inline const StyleSelfAlignmentData& RenderStyle::justifySelf() const { return m_nonInheritedData->miscData->justifySelf; }
inline const Vector<StyleContentAlignmentData>& RenderStyle::justifyTracks() const { return m_nonInheritedData->rareData->grid->justifyTracks; }
inline const Length& RenderStyle::left() const { return m_nonInheritedData->surroundData->offset.left(); }
inline LineAlign RenderStyle::lineAlign() const { return static_cast<LineAlign>(m_rareInheritedData->lineAlign); }
inline OptionSet<LineBoxContain> RenderStyle::lineBoxContain() const { return OptionSet<LineBoxContain>::fromRaw(m_rareInheritedData->lineBoxContain); }
inline LineBreak RenderStyle::lineBreak() const { return static_cast<LineBreak>(m_rareInheritedData->lineBreak); }
inline const LineClampValue& RenderStyle::lineClamp() const { return m_nonInheritedData->rareData->lineClamp; }
inline const AtomString& RenderStyle::lineGrid() const { return m_rareInheritedData->lineGrid; }
inline LineSnap RenderStyle::lineSnap() const { return static_cast<LineSnap>(m_rareInheritedData->lineSnap); }
inline ListStyleType RenderStyle::listStyleType() const { return m_rareInheritedData->listStyleType; }
inline const Length& RenderStyle::logicalBottom() const { return m_nonInheritedData->surroundData->offset.after(writingMode()); }
inline const Length& RenderStyle::logicalHeight() const { return isHorizontalWritingMode() ? height() : width(); }
inline const Length& RenderStyle::logicalLeft() const { return m_nonInheritedData->surroundData->offset.start(writingMode()); }
inline const Length& RenderStyle::logicalMaxHeight() const { return isHorizontalWritingMode() ? maxHeight() : maxWidth(); }
inline const Length& RenderStyle::logicalMaxWidth() const { return isHorizontalWritingMode() ? maxWidth() : maxHeight(); }
inline const Length& RenderStyle::logicalMinHeight() const { return isHorizontalWritingMode() ? minHeight() : minWidth(); }
inline const Length& RenderStyle::logicalMinWidth() const { return isHorizontalWritingMode() ? minWidth() : minHeight(); }
inline const Length& RenderStyle::logicalRight() const { return m_nonInheritedData->surroundData->offset.end(writingMode()); }
inline const Length& RenderStyle::logicalTop() const { return m_nonInheritedData->surroundData->offset.before(writingMode()); }
inline const Length& RenderStyle::logicalWidth() const { return isHorizontalWritingMode() ? width() : height(); }
inline const Length& RenderStyle::marginAfter() const { return m_nonInheritedData->surroundData->margin.after(writingMode()); }
inline const Length& RenderStyle::marginAfterUsing(const RenderStyle* otherStyle) const { return m_nonInheritedData->surroundData->margin.after(otherStyle->writingMode()); }
inline const Length& RenderStyle::marginBefore() const { return m_nonInheritedData->surroundData->margin.before(writingMode()); }
inline const Length& RenderStyle::marginBeforeUsing(const RenderStyle* otherStyle) const { return m_nonInheritedData->surroundData->margin.before(otherStyle->writingMode()); }
inline const Length& RenderStyle::marginBottom() const { return m_nonInheritedData->surroundData->margin.bottom(); }
inline const Length& RenderStyle::marginEnd() const { return m_nonInheritedData->surroundData->margin.end(writingMode(), direction()); }
inline const Length& RenderStyle::marginEndUsing(const RenderStyle* otherStyle) const { return m_nonInheritedData->surroundData->margin.end(otherStyle->writingMode(), otherStyle->direction()); }
inline const Length& RenderStyle::marginLeft() const { return m_nonInheritedData->surroundData->margin.left(); }
inline const Length& RenderStyle::marginRight() const { return m_nonInheritedData->surroundData->margin.right(); }
inline const Length& RenderStyle::marginStart() const { return m_nonInheritedData->surroundData->margin.start(writingMode(), direction()); }
inline const Length& RenderStyle::marginStartUsing(const RenderStyle* otherStyle) const { return m_nonInheritedData->surroundData->margin.start(otherStyle->writingMode(), otherStyle->direction()); }
inline const Length& RenderStyle::marginTop() const { return m_nonInheritedData->surroundData->margin.top(); }
inline OptionSet<MarginTrimType> RenderStyle::marginTrim() const { return m_nonInheritedData->rareData->marginTrim; }
inline MarqueeBehavior RenderStyle::marqueeBehavior() const { return static_cast<MarqueeBehavior>(m_nonInheritedData->rareData->marquee->behavior); }
inline MarqueeDirection RenderStyle::marqueeDirection() const { return static_cast<MarqueeDirection>(m_nonInheritedData->rareData->marquee->direction); }
inline const Length& RenderStyle::marqueeIncrement() const { return m_nonInheritedData->rareData->marquee->increment; }
inline int RenderStyle::marqueeLoopCount() const { return m_nonInheritedData->rareData->marquee->loops; }
inline int RenderStyle::marqueeSpeed() const { return m_nonInheritedData->rareData->marquee->speed; }
inline const NinePieceImage& RenderStyle::maskBoxImage() const { return m_nonInheritedData->rareData->maskBoxImage; }
inline LayoutBoxExtent RenderStyle::maskBoxImageOutsets() const { return imageOutsets(maskBoxImage()); }
inline StyleImage* RenderStyle::maskBoxImageSource() const { return maskBoxImage().image(); }
inline FillBox RenderStyle::maskClip() const { return maskLayers().clip(); }
inline CompositeOperator RenderStyle::maskComposite() const { return maskLayers().composite(); }
inline StyleImage* RenderStyle::maskImage() const { return maskLayers().image(); }
inline const FillLayer& RenderStyle::maskLayers() const { return m_nonInheritedData->miscData->mask; }
inline FillBox RenderStyle::maskOrigin() const { return maskLayers().origin(); }
inline FillRepeatXY RenderStyle::maskRepeat() const { return maskLayers().repeat(); }
inline const LengthSize& RenderStyle::maskSizeLength() const { return maskLayers().sizeLength(); }
inline FillSizeType RenderStyle::maskSizeType() const { return maskLayers().sizeType(); }
inline const Length& RenderStyle::maskXPosition() const { return maskLayers().xPosition(); }
inline const Length& RenderStyle::maskYPosition() const { return maskLayers().yPosition(); }
inline MasonryAutoFlow RenderStyle::masonryAutoFlow() const { return m_nonInheritedData->rareData->grid->masonryAutoFlow; }
inline MathStyle RenderStyle::mathStyle() const { return static_cast<MathStyle>(m_rareInheritedData->mathStyle); }
inline const Length& RenderStyle::maxHeight() const { return m_nonInheritedData->boxData->maxHeight(); }
inline const Length& RenderStyle::maxWidth() const { return m_nonInheritedData->boxData->maxWidth(); }
inline const Length& RenderStyle::minHeight() const { return m_nonInheritedData->boxData->minHeight(); }
inline const Length& RenderStyle::minWidth() const { return m_nonInheritedData->boxData->minWidth(); }
inline const NamedGridAreaMap& RenderStyle::namedGridArea() const { return m_nonInheritedData->rareData->grid->namedGridArea; }
inline size_t RenderStyle::namedGridAreaColumnCount() const { return m_nonInheritedData->rareData->grid->namedGridAreaColumnCount; }
inline size_t RenderStyle::namedGridAreaRowCount() const { return m_nonInheritedData->rareData->grid->namedGridAreaRowCount; }
inline const NamedGridLinesMap& RenderStyle::namedGridColumnLines() const { return m_nonInheritedData->rareData->grid->namedGridColumnLines(); }
inline const NamedGridLinesMap& RenderStyle::namedGridRowLines() const { return m_nonInheritedData->rareData->grid->namedGridRowLines(); }
inline NBSPMode RenderStyle::nbspMode() const { return static_cast<NBSPMode>(m_rareInheritedData->nbspMode); }
inline const StyleCustomPropertyData& RenderStyle::nonInheritedCustomProperties() const { return m_nonInheritedData->rareData->customProperties.get(); }
inline ObjectFit RenderStyle::objectFit() const { return static_cast<ObjectFit>(m_nonInheritedData->miscData->objectFit); }
inline const LengthPoint& RenderStyle::objectPosition() const { return m_nonInheritedData->miscData->objectPosition; }
inline const LengthPoint& RenderStyle::offsetAnchor() const { return m_nonInheritedData->rareData->offsetAnchor; }
inline const Length& RenderStyle::offsetDistance() const { return m_nonInheritedData->rareData->offsetDistance; }
inline PathOperation* RenderStyle::offsetPath() const { return m_nonInheritedData->rareData->offsetPath.get(); }
inline const LengthPoint& RenderStyle::offsetPosition() const { return m_nonInheritedData->rareData->offsetPosition; }
inline OffsetRotation RenderStyle::offsetRotate() const { return m_nonInheritedData->rareData->offsetRotate; }
inline Length RenderStyle::oneLength() { return { 1, LengthType::Fixed }; }
inline float RenderStyle::opacity() const { return m_nonInheritedData->miscData->opacity; }
inline int RenderStyle::order() const { return m_nonInheritedData->miscData->order; }
inline const OrderedNamedGridLinesMap& RenderStyle::orderedNamedGridColumnLines() const { return m_nonInheritedData->rareData->grid->orderedNamedGridColumnLines(); }
inline const OrderedNamedGridLinesMap& RenderStyle::orderedNamedGridRowLines() const { return m_nonInheritedData->rareData->grid->orderedNamedGridRowLines(); }
inline unsigned short RenderStyle::orphans() const { return m_rareInheritedData->orphans; }
inline const StyleColor& RenderStyle::outlineColor() const { return m_nonInheritedData->backgroundData->outline.color(); }
inline BorderStyle RenderStyle::outlineStyle() const { return m_nonInheritedData->backgroundData->outline.style(); }
inline OutlineIsAuto RenderStyle::outlineStyleIsAuto() const { return static_cast<OutlineIsAuto>(m_nonInheritedData->backgroundData->outline.isAuto()); }
inline OverflowAnchor RenderStyle::overflowAnchor() const { return static_cast<OverflowAnchor>(m_nonInheritedData->rareData->overflowAnchor); }
inline OverflowWrap RenderStyle::overflowWrap() const { return static_cast<OverflowWrap>(m_rareInheritedData->overflowWrap); }
inline OverscrollBehavior RenderStyle::overscrollBehaviorX() const { return static_cast<OverscrollBehavior>(m_nonInheritedData->rareData->overscrollBehaviorX); }
inline OverscrollBehavior RenderStyle::overscrollBehaviorY() const { return static_cast<OverscrollBehavior>(m_nonInheritedData->rareData->overscrollBehaviorY); }
inline const Length& RenderStyle::paddingAfter() const { return paddingBox().after(writingMode()); }
inline const Length& RenderStyle::paddingBefore() const { return paddingBox().before(writingMode()); }
inline const Length& RenderStyle::paddingBottom() const { return paddingBox().bottom(); }
inline const LengthBox& RenderStyle::paddingBox() const { return m_nonInheritedData->surroundData->padding; }
inline const Length& RenderStyle::paddingEnd() const { return paddingBox().end(writingMode(), direction()); }
inline const Length& RenderStyle::paddingLeft() const { return paddingBox().left(); }
inline const Length& RenderStyle::paddingRight() const { return paddingBox().right(); }
inline const Length& RenderStyle::paddingStart() const { return paddingBox().start(writingMode(), direction()); }
inline const Length& RenderStyle::paddingTop() const { return paddingBox().top(); }
inline const LengthSize& RenderStyle::pageSize() const { return m_nonInheritedData->rareData->pageSize; }
inline PageSizeType RenderStyle::pageSizeType() const { return static_cast<PageSizeType>(m_nonInheritedData->rareData->pageSizeType); }
inline PaintOrder RenderStyle::paintOrder() const { return static_cast<PaintOrder>(m_rareInheritedData->paintOrder); }
inline float RenderStyle::perspective() const { return m_nonInheritedData->rareData->perspective; }
inline LengthPoint RenderStyle::perspectiveOrigin() const { return m_nonInheritedData->rareData->perspectiveOrigin(); }
inline const Length& RenderStyle::perspectiveOriginX() const { return m_nonInheritedData->rareData->perspectiveOriginX; }
inline const Length& RenderStyle::perspectiveOriginY() const { return m_nonInheritedData->rareData->perspectiveOriginY; }
inline bool RenderStyle::preserveNewline() const { return preserveNewline(whiteSpace()); }
inline bool RenderStyle::preserves3D() const { return usedTransformStyle3D() == TransformStyle3D::Preserve3D; }
inline QuotesData* RenderStyle::quotes() const { return m_rareInheritedData->quotes.get(); }
inline Resize RenderStyle::resize() const { return static_cast<Resize>(m_nonInheritedData->miscData->resize); }
inline const Length& RenderStyle::right() const { return m_nonInheritedData->surroundData->offset.right(); }
inline RotateTransformOperation* RenderStyle::rotate() const { return m_nonInheritedData->rareData->rotate.get(); }
inline const GapLength& RenderStyle::rowGap() const { return m_nonInheritedData->rareData->rowGap; }
inline RubyPosition RenderStyle::rubyPosition() const { return static_cast<RubyPosition>(m_rareInheritedData->rubyPosition); }
inline ScaleTransformOperation* RenderStyle::scale() const { return m_nonInheritedData->rareData->scale.get(); }
inline std::optional<ScrollbarColor> RenderStyle::scrollbarColor() const { return m_rareInheritedData->scrollbarColor.asOptional(); }
inline const StyleColor& RenderStyle::scrollbarThumbColor() const { return m_rareInheritedData->scrollbarColor->thumbColor; }
inline const StyleColor& RenderStyle::scrollbarTrackColor() const { return m_rareInheritedData->scrollbarColor->trackColor; }
inline float RenderStyle::shapeImageThreshold() const { return m_nonInheritedData->rareData->shapeImageThreshold; }
inline const Length& RenderStyle::shapeMargin() const { return m_nonInheritedData->rareData->shapeMargin; }
inline ShapeValue* RenderStyle::shapeOutside() const { return m_nonInheritedData->rareData->shapeOutside.get(); }
inline OptionSet<SpeakAs> RenderStyle::speakAs() const { return OptionSet<SpeakAs>::fromRaw(m_rareInheritedData->speakAs); }
inline const AtomString& RenderStyle::specifiedLocale() const { return fontDescription().specifiedLocale(); }
inline int RenderStyle::specifiedZIndex() const { return m_nonInheritedData->boxData->specifiedZIndex(); }
inline bool RenderStyle::specifiesColumns() const { return !hasAutoColumnCount() || !hasAutoColumnWidth() || !hasInlineColumnAxis(); }
constexpr OptionSet<Containment> RenderStyle::strictContainment() { return { Containment::Size, Containment::Layout, Containment::Paint, Containment::Style }; }
inline StyleColor RenderStyle::strokeColor() const { return m_rareInheritedData->strokeColor; }
inline float RenderStyle::strokeMiterLimit() const { return m_rareInheritedData->miterLimit; }
inline const TabSize& RenderStyle::tabSize() const { return m_rareInheritedData->tabSize; }
inline TextAlignLast RenderStyle::textAlignLast() const { return static_cast<TextAlignLast>(m_rareInheritedData->textAlignLast); }
inline TextBoxTrim RenderStyle::textBoxTrim() const { return static_cast<TextBoxTrim>(m_nonInheritedData->rareData->textBoxTrim); }
inline TextCombine RenderStyle::textCombine() const { return static_cast<TextCombine>(m_rareInheritedData->textCombine); }
inline const StyleColor& RenderStyle::textDecorationColor() const { return m_nonInheritedData->rareData->textDecorationColor; }
inline OptionSet<TextDecorationLine> RenderStyle::textDecorationLine() const { return OptionSet<TextDecorationLine>::fromRaw(m_nonInheritedFlags.textDecorationLine); }
inline TextDecorationSkipInk RenderStyle::textDecorationSkipInk() const { return static_cast<TextDecorationSkipInk>(m_rareInheritedData->textDecorationSkipInk); }
inline TextDecorationStyle RenderStyle::textDecorationStyle() const { return static_cast<TextDecorationStyle>(m_nonInheritedData->rareData->textDecorationStyle); }
inline TextDecorationThickness RenderStyle::textDecorationThickness() const { return m_nonInheritedData->rareData->textDecorationThickness; }
inline OptionSet<TextDecorationLine> RenderStyle::textDecorationsInEffect() const { return OptionSet<TextDecorationLine>::fromRaw(m_inheritedFlags.textDecorationLines); }
inline const StyleColor& RenderStyle::textEmphasisColor() const { return m_rareInheritedData->textEmphasisColor; }
inline const AtomString& RenderStyle::textEmphasisCustomMark() const { return m_rareInheritedData->textEmphasisCustomMark; }
inline TextEmphasisFill RenderStyle::textEmphasisFill() const { return static_cast<TextEmphasisFill>(m_rareInheritedData->textEmphasisFill); }
inline OptionSet<TextEmphasisPosition> RenderStyle::textEmphasisPosition() const { return OptionSet<TextEmphasisPosition>::fromRaw(m_rareInheritedData->textEmphasisPosition); }
inline const StyleColor& RenderStyle::textFillColor() const { return m_rareInheritedData->textFillColor; }
inline TextGroupAlign RenderStyle::textGroupAlign() const { return static_cast<TextGroupAlign>(m_nonInheritedData->rareData->textGroupAlign); }
inline const Length& RenderStyle::textIndent() const { return m_rareInheritedData->indent; }
inline TextIndentLine RenderStyle::textIndentLine() const { return static_cast<TextIndentLine>(m_rareInheritedData->textIndentLine); }
inline TextIndentType RenderStyle::textIndentType() const { return static_cast<TextIndentType>(m_rareInheritedData->textIndentType); }
inline TextJustify RenderStyle::textJustify() const { return static_cast<TextJustify>(m_rareInheritedData->textJustify); }
inline TextOrientation RenderStyle::textOrientation() const { return static_cast<TextOrientation>(m_rareInheritedData->textOrientation); }
inline TextOverflow RenderStyle::textOverflow() const { return static_cast<TextOverflow>(m_nonInheritedData->miscData->textOverflow); }
inline TextSecurity RenderStyle::textSecurity() const { return static_cast<TextSecurity>(m_rareInheritedData->textSecurity); }
inline const ShadowData* RenderStyle::textShadow() const { return m_rareInheritedData->textShadow.get(); }
inline LayoutBoxExtent RenderStyle::textShadowExtent() const { return shadowExtent(textShadow()); }
inline const StyleColor& RenderStyle::textStrokeColor() const { return m_rareInheritedData->textStrokeColor; }
inline float RenderStyle::textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
inline OptionSet<TextTransform> RenderStyle::textTransform() const { return OptionSet<TextTransform>::fromRaw(m_inheritedFlags.textTransform); }
inline TextUnderlineOffset RenderStyle::textUnderlineOffset() const { return m_rareInheritedData->textUnderlineOffset; }
inline TextUnderlinePosition RenderStyle::textUnderlinePosition() const { return static_cast<TextUnderlinePosition>(m_rareInheritedData->textUnderlinePosition); }
inline TextZoom RenderStyle::textZoom() const { return static_cast<TextZoom>(m_rareInheritedData->textZoom); }
inline const Length& RenderStyle::top() const { return m_nonInheritedData->surroundData->offset.top(); }
inline OptionSet<TouchAction> RenderStyle::touchActions() const { return m_nonInheritedData->rareData->touchActions; }
inline const TransformOperations& RenderStyle::transform() const { return m_nonInheritedData->miscData->transform->operations; }
inline TransformBox RenderStyle::transformBox() const { return m_nonInheritedData->miscData->transform->transformBox; }
inline const Length& RenderStyle::transformOriginX() const { return m_nonInheritedData->miscData->transform->x; }
inline LengthPoint RenderStyle::transformOriginXY() const { return m_nonInheritedData->miscData->transform->originXY(); }
inline const Length& RenderStyle::transformOriginY() const { return m_nonInheritedData->miscData->transform->y; }
inline float RenderStyle::transformOriginZ() const { return m_nonInheritedData->miscData->transform->z; }
inline TransformStyle3D RenderStyle::transformStyle3D() const { return static_cast<TransformStyle3D>(m_nonInheritedData->rareData->transformStyle3D); }
inline const AnimationList* RenderStyle::transitions() const { return m_nonInheritedData->miscData->transitions.get(); }
inline AnimationList* RenderStyle::transitions() { return m_nonInheritedData->miscData->transitions.get(); }
inline TranslateTransformOperation* RenderStyle::translate() const { return m_nonInheritedData->rareData->translate.get(); }
inline bool RenderStyle::useSmoothScrolling() const { return m_nonInheritedData->rareData->useSmoothScrolling; }
inline float RenderStyle::usedPerspective() const { return std::max(1.0f, perspective()); }
inline TransformStyle3D RenderStyle::usedTransformStyle3D() const { return static_cast<bool>(m_nonInheritedData->rareData->transformStyleForcedToFlat) ? TransformStyle3D::Flat : transformStyle3D(); }
inline int RenderStyle::usedZIndex() const { return m_nonInheritedData->boxData->usedZIndex(); }
inline UserDrag RenderStyle::userDrag() const { return static_cast<UserDrag>(m_nonInheritedData->miscData->userDrag); }
inline UserModify RenderStyle::userModify() const { return static_cast<UserModify>(m_rareInheritedData->userModify); }
inline UserSelect RenderStyle::userSelect() const { return static_cast<UserSelect>(m_rareInheritedData->userSelect); }
inline const Length& RenderStyle::verticalAlignLength() const { return m_nonInheritedData->boxData->verticalAlign(); }
inline const StyleColor& RenderStyle::visitedLinkBackgroundColor() const { return m_nonInheritedData->miscData->visitedLinkColor->background; }
inline const StyleColor& RenderStyle::visitedLinkBorderBottomColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderBottom; }
inline const StyleColor& RenderStyle::visitedLinkBorderLeftColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderLeft; }
inline const StyleColor& RenderStyle::visitedLinkBorderRightColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderRight; }
inline const StyleColor& RenderStyle::visitedLinkBorderTopColor() const { return m_nonInheritedData->miscData->visitedLinkColor->borderTop; }
inline const StyleColor& RenderStyle::visitedLinkCaretColor() const { return m_rareInheritedData->visitedLinkCaretColor; }
inline const StyleColor& RenderStyle::visitedLinkColumnRuleColor() const { return m_nonInheritedData->miscData->multiCol->visitedLinkColumnRuleColor; }
inline const StyleColor& RenderStyle::visitedLinkOutlineColor() const { return m_nonInheritedData->miscData->visitedLinkColor->outline; }
inline const StyleColor& RenderStyle::visitedLinkStrokeColor() const { return m_rareInheritedData->visitedLinkStrokeColor; }
inline const StyleColor& RenderStyle::visitedLinkTextDecorationColor() const { return m_nonInheritedData->miscData->visitedLinkColor->textDecoration; }
inline const StyleColor& RenderStyle::visitedLinkTextEmphasisColor() const { return m_rareInheritedData->visitedLinkTextEmphasisColor; }
inline const StyleColor& RenderStyle::visitedLinkTextFillColor() const { return m_rareInheritedData->visitedLinkTextFillColor; }
inline const StyleColor& RenderStyle::visitedLinkTextStrokeColor() const { return m_rareInheritedData->visitedLinkTextStrokeColor; }
inline unsigned short RenderStyle::widows() const { return m_rareInheritedData->widows; }
inline const Length& RenderStyle::width() const { return m_nonInheritedData->boxData->width(); }
inline WillChangeData* RenderStyle::willChange() const { return m_nonInheritedData->rareData->willChange.get(); }
inline bool RenderStyle::willChangeCreatesStackingContext() const { return willChange() && willChange()->canCreateStackingContext(); }
inline WordBreak RenderStyle::wordBreak() const { return static_cast<WordBreak>(m_rareInheritedData->wordBreak); }
constexpr LengthType RenderStyle::zeroLength() { return LengthType::Fixed; }
inline float RenderStyle::zoom() const { return m_nonInheritedData->rareData->zoom; }

// ignore non-standard ::-webkit-scrollbar when standard properties are in use
inline bool RenderStyle::usesStandardScrollbarStyle() const { return scrollbarWidth() != ScrollbarWidth::Auto || scrollbarColor().has_value(); }
inline bool RenderStyle::usesLegacyScrollbarStyle() const { return hasPseudoStyle(PseudoId::Scrollbar) && !usesStandardScrollbarStyle(); }

#if ENABLE(APPLE_PAY)
inline ApplePayButtonStyle RenderStyle::applePayButtonStyle() const { return static_cast<ApplePayButtonStyle>(m_nonInheritedData->rareData->applePayButtonStyle); }
inline ApplePayButtonType RenderStyle::applePayButtonType() const { return static_cast<ApplePayButtonType>(m_nonInheritedData->rareData->applePayButtonType); }
constexpr ApplePayButtonStyle RenderStyle::initialApplePayButtonStyle() { return ApplePayButtonStyle::Black; }
constexpr ApplePayButtonType RenderStyle::initialApplePayButtonType() { return ApplePayButtonType::Plain; }
#endif

#if ENABLE(CSS_BOX_DECORATION_BREAK)
inline BoxDecorationBreak RenderStyle::boxDecorationBreak() const { return m_nonInheritedData->boxData->boxDecorationBreak(); }
#endif

#if ENABLE(CSS_COMPOSITING)
inline BlendMode RenderStyle::blendMode() const { return static_cast<BlendMode>(m_nonInheritedData->rareData->effectiveBlendMode); }
constexpr BlendMode RenderStyle::initialBlendMode() { return BlendMode::Normal; }
constexpr Isolation RenderStyle::initialIsolation() { return Isolation::Auto; }
inline bool RenderStyle::isInSubtreeWithBlendMode() const { return m_rareInheritedData->isInSubtreeWithBlendMode; }
inline Isolation RenderStyle::isolation() const { return static_cast<Isolation>(m_nonInheritedData->rareData->isolation); }
#else
inline BlendMode RenderStyle::blendMode() const { return BlendMode::Normal; }
inline Isolation RenderStyle::isolation() const { return Isolation::Auto; }
#endif

#if ENABLE(CURSOR_VISIBILITY)
constexpr CursorVisibility RenderStyle::initialCursorVisibility() { return CursorVisibility::Auto; }
#endif

#if ENABLE(DARK_MODE_CSS)
inline StyleColorScheme RenderStyle::colorScheme() const { return m_rareInheritedData->colorScheme; }
constexpr StyleColorScheme RenderStyle::initialColorScheme() { return { }; }
#endif

#if ENABLE(FILTERS_LEVEL_2)
inline const FilterOperations& RenderStyle::backdropFilter() const { return m_nonInheritedData->rareData->backdropFilter->operations; }
inline bool RenderStyle::hasBackdropFilter() const { return !backdropFilter().operations().isEmpty(); }
inline FilterOperations RenderStyle::initialBackdropFilter() { return { }; }
#endif

#if PLATFORM(IOS_FAMILY)
inline bool RenderStyle::touchCalloutEnabled() const { return m_rareInheritedData->touchCalloutEnabled; }
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline bool RenderStyle::useTouchOverflowScrolling() const { return m_rareInheritedData->useTouchOverflowScrolling; }
#endif

#if ENABLE(TEXT_AUTOSIZING)
inline Length RenderStyle::initialSpecifiedLineHeight() { return { -100.0f, LengthType::Percent }; }
constexpr TextSizeAdjustment RenderStyle::initialTextSizeAdjust() { return { }; }
inline TextSizeAdjustment RenderStyle::textSizeAdjust() const { return m_rareInheritedData->textSizeAdjust; }
#endif

#if ENABLE(TOUCH_EVENTS)
inline StyleColor RenderStyle::tapHighlightColor() const { return m_rareInheritedData->tapHighlightColor; }
#endif

inline bool RenderStyle::InheritedFlags::operator==(const InheritedFlags& other) const
{
    return emptyCells == other.emptyCells
        && captionSide == other.captionSide
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
        && whiteSpaceCollapse == other.whiteSpaceCollapse
        && textWrap == other.textWrap
        && borderCollapse == other.borderCollapse
        && boxDirection == other.boxDirection
        && rtlOrdering == other.rtlOrdering
        && printColorAdjust == other.printColorAdjust
        && pointerEvents == other.pointerEvents
        && insideLink == other.insideLink
        && insideDefaultButton == other.insideDefaultButton
        && writingMode == other.writingMode
        && hasExplicitlySetColor == other.hasExplicitlySetColor;
}

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
        && textDecorationLine == other.textDecorationLine
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

inline bool RenderStyle::NonInheritedFlags::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > PseudoId::None);
    ASSERT(pseudo < PseudoId::FirstInternalPseudoId);
    return pseudoBits & (1 << (static_cast<unsigned>(pseudo) - 1 /* PseudoId::None */));
}

inline bool RenderStyle::NonInheritedFlags::hasAnyPublicPseudoStyles() const
{
    return static_cast<unsigned>(PseudoId::PublicPseudoIdMask) & pseudoBits;
}

constexpr bool RenderStyle::autoWrap(WhiteSpace mode)
{
    // Nowrap and pre don't automatically wrap.
    return mode != WhiteSpace::NoWrap && mode != WhiteSpace::Pre;
}

inline bool RenderStyle::breakOnlyAfterWhiteSpace() const
{
    return whiteSpace() == WhiteSpace::PreWrap || whiteSpace() == WhiteSpace::BreakSpaces || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool RenderStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::BreakWord || overflowWrap() == OverflowWrap::Anywhere;
}

constexpr bool RenderStyle::collapseWhiteSpace(WhiteSpace mode)
{
    // Pre and prewrap do not collapse whitespace.
    return mode != WhiteSpace::Pre && mode != WhiteSpace::PreWrap && mode != WhiteSpace::BreakSpaces;
}

inline void RenderStyle::getShadowInlineDirectionExtent(const ShadowData* shadow, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const
{
    return isHorizontalWritingMode() ? getShadowHorizontalExtent(shadow, logicalLeft, logicalRight) : getShadowVerticalExtent(shadow, logicalLeft, logicalRight);
}

inline void RenderStyle::getShadowBlockDirectionExtent(const ShadowData* shadow, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const
{
    return isHorizontalWritingMode() ? getShadowVerticalExtent(shadow, logicalTop, logicalBottom) : getShadowHorizontalExtent(shadow, logicalTop, logicalBottom);
}

inline bool RenderStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || isHorizontalWritingMode() == (axis == ColumnAxis::Horizontal);
}

inline Length RenderStyle::initialLineHeight()
{
    // Returning -100% here means the line-height is not set.
    return { -100.0f, LengthType::Percent };
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

constexpr bool RenderStyle::isDisplayBlockType(DisplayType display)
{
    return display == DisplayType::Block
        || display == DisplayType::Box
        || display == DisplayType::Flex
        || display == DisplayType::FlowRoot
        || display == DisplayType::Grid
        || display == DisplayType::ListItem
        || display == DisplayType::Table;
}

constexpr bool RenderStyle::isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::Inline
        || display == DisplayType::InlineBlock
        || display == DisplayType::InlineBox
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || display == DisplayType::InlineTable;
}

constexpr bool RenderStyle::isDisplayRegionType() const
{
    return display() == DisplayType::Block
        || display() == DisplayType::InlineBlock
        || display() == DisplayType::TableCell
        || display() == DisplayType::TableCaption
        || display() == DisplayType::ListItem;
}

constexpr bool RenderStyle::isDisplayTableOrTablePart(DisplayType display)
{
    return display == DisplayType::Table
        || display == DisplayType::InlineTable
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

inline double RenderStyle::logicalAspectRatio() const
{
    ASSERT(aspectRatioType() != AspectRatioType::Auto);
    if (isHorizontalWritingMode())
        return aspectRatioWidth() / aspectRatioHeight();
    return aspectRatioHeight() / aspectRatioWidth();
}

constexpr bool RenderStyle::preserveNewline(WhiteSpace mode)
{
    // Normal and nowrap do not preserve newlines.
    return mode != WhiteSpace::Normal && mode != WhiteSpace::NoWrap;
}

inline bool isSkippedContentRoot(const RenderStyle& style, const Element* element)
{
    switch (style.contentVisibility()) {
    case ContentVisibility::Visible:
        return false;
    case ContentVisibility::Hidden:
        return true;
    case ContentVisibility::Auto:
        return element && !element->isRelevantToUser();
    };

    ASSERT_NOT_REACHED();
    return false;
}

inline float adjustFloatForAbsoluteZoom(float value, const RenderStyle& style)
{
    return value / style.effectiveZoom();
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

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderStyle& style)
{
    auto zoom = style.effectiveZoom();
    return { size.width() / zoom, size.height() / zoom };
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderStyle& style)
{
    return LayoutUnit(value / style.effectiveZoom());
}

constexpr BorderStyle collapsedBorderStyle(BorderStyle style)
{
    if (style == BorderStyle::Outset)
        return BorderStyle::Groove;
    if (style == BorderStyle::Inset)
        return BorderStyle::Ridge;
    return style;
}

inline bool generatesBox(const RenderStyle& style)
{
    return style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

inline bool isNonVisibleOverflow(Overflow overflow)
{
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll || overflow == Overflow::Clip;
}

inline bool pseudoElementRendererIsNeeded(const RenderStyle* style)
{
    return style && style->display() != DisplayType::None && style->contentData();
}

} // namespace WebCore

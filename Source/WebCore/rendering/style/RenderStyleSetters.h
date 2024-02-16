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

#include "RenderStyleInlines.h"
#include "StyleReflection.h"

namespace WebCore {

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET_STYLE_PROPERTY_WITH_FUNCTIONS(read, write, value) SET_STYLE_PROPERTY_BASE(read(), value, write(value))

#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)

#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { auto& readable = *read; if (!compareEqual(readable.variable1, value1) || !compareEqual(readable.variable2, value2)) { auto& writable = write; writable.variable1 = value1; writable.variable2 = value2; } } while (0)

#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

#define SET_BORDER_COLOR(group, variable, value) SET_STYLE_PROPERTY_WITH_FUNCTIONS(group->variable.color, group.access().variable.setColor, value)
#define SET_NESTED_BORDER_COLOR(group, parentVariable, variable, value) SET_STYLE_PROPERTY_WITH_FUNCTIONS(group->parentVariable->variable.color, group.access().parentVariable.access().variable.setColor, value)

template<typename T, typename U> inline bool compareEqual(const T& a, const U& b) { return a == b; }

inline void RenderStyle::addToTextDecorationsInEffect(OptionSet<TextDecorationLine> value) { m_inheritedFlags.textDecorationLines |= static_cast<unsigned>(value.toRaw()); }
inline void RenderStyle::clearAnimations() { m_nonInheritedData.access().miscData.access().animations = nullptr; }
inline void RenderStyle::clearBackgroundLayers() { m_nonInheritedData.access().backgroundData.access().background = FillLayer::create(FillLayerType::Background); }
inline void RenderStyle::clearMaskLayers() { m_nonInheritedData.access().miscData.access().mask = FillLayer::create(FillLayerType::Mask); }
inline void RenderStyle::clearTransitions() { m_nonInheritedData.access().miscData.access().transitions = nullptr; }
inline FillLayer& RenderStyle::ensureBackgroundLayers() { return m_nonInheritedData.access().backgroundData.access().background.access(); }
inline FillLayer& RenderStyle::ensureMaskLayers() { return m_nonInheritedData.access().miscData.access().mask.access(); }
inline void RenderStyle::inheritBackgroundLayers(const FillLayer& parent) { m_nonInheritedData.access().backgroundData.access().background = FillLayer::create(parent); }
inline void RenderStyle::inheritColumnPropertiesFrom(const RenderStyle& parent) { m_nonInheritedData.access().miscData.access().multiCol = parent.m_nonInheritedData->miscData->multiCol; }
inline void RenderStyle::inheritMaskLayers(const FillLayer& parent) { m_nonInheritedData.access().miscData.access().mask = FillLayer::create(parent); }
inline FilterOperations& RenderStyle::mutableAppleColorFilter() { return m_rareInheritedData.access().appleColorFilter.access().operations; }
inline FilterOperations& RenderStyle::mutableFilter() { return m_nonInheritedData.access().miscData.access().filter.access().operations; }
inline void RenderStyle::resetBorderBottom() { SET_NESTED(m_nonInheritedData, surroundData, border.m_bottom, BorderValue()); }
inline void RenderStyle::resetBorderBottomLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomLeft, initialBorderRadius()); }
inline void RenderStyle::resetBorderBottomRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomRight, initialBorderRadius()); }
inline void RenderStyle::resetBorderImage() { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, NinePieceImage()); }
inline void RenderStyle::resetBorderLeft() { SET_NESTED(m_nonInheritedData, surroundData, border.m_left, BorderValue()); }
inline void RenderStyle::resetBorderRight() { SET_NESTED(m_nonInheritedData, surroundData, border.m_right, BorderValue()); }
inline void RenderStyle::resetBorderTop() { SET_NESTED(m_nonInheritedData, surroundData, border.m_top, BorderValue { }); }
inline void RenderStyle::resetBorderTopLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topLeft, initialBorderRadius()); }
inline void RenderStyle::resetBorderTopRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topRight, initialBorderRadius()); }
inline void RenderStyle::resetColumnRule() { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule, BorderValue()); }
inline void RenderStyle::resetMargin() { SET_NESTED(m_nonInheritedData, surroundData, margin, LengthBox(LengthType::Fixed)); }
inline void RenderStyle::resetPadding() { SET_NESTED(m_nonInheritedData, surroundData, padding, LengthBox { 0 }); }
inline void RenderStyle::resetPageSizeType() { SET_NESTED(m_nonInheritedData, rareData, pageSizeType, PAGE_SIZE_AUTO); }
inline void RenderStyle::setAccentColor(const StyleColor& color) { SET_PAIR(m_rareInheritedData, accentColor, color, hasAutoAccentColor, false); }
inline void RenderStyle::setAlignContent(const StyleContentAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignContent, data); }
inline void RenderStyle::setAlignItems(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignItems, data); }
inline void RenderStyle::setAlignItemsPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().alignItems.setPosition(position); }
inline void RenderStyle::setAlignSelf(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignSelf, data); }
inline void RenderStyle::setAlignSelfPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().alignSelf.setPosition(position); }
inline void RenderStyle::setAppearance(StyleAppearance appearance) { SET_NESTED_PAIR(m_nonInheritedData, miscData, appearance, static_cast<unsigned>(appearance), effectiveAppearance, static_cast<unsigned>(appearance)); }
inline void RenderStyle::setAppleColorFilter(const FilterOperations& ops) { SET_NESTED(m_rareInheritedData, appleColorFilter, operations, ops); }
inline void RenderStyle::setAspectRatio(double width, double height) { SET_NESTED_PAIR(m_nonInheritedData, miscData, aspectRatioWidth, width, aspectRatioHeight, height); }
inline void RenderStyle::setAspectRatioType(AspectRatioType aspectRatioType) { SET_NESTED(m_nonInheritedData, miscData, aspectRatioType, static_cast<unsigned>(aspectRatioType)); }
inline void RenderStyle::setBackfaceVisibility(BackfaceVisibility b) { SET_NESTED(m_nonInheritedData, rareData, backfaceVisibility, static_cast<unsigned>(b)); }
inline void RenderStyle::setBackgroundAttachment(FillAttachment attachment) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, backgroundData, background, m_attachment, static_cast<unsigned>(attachment), m_attachmentSet, true); }
inline void RenderStyle::setBackgroundBlendMode(BlendMode blendMode) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, backgroundData, background, m_blendMode, static_cast<unsigned>(blendMode), m_blendModeSet, true); }
inline void RenderStyle::setBackgroundClip(FillBox fillBox) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, backgroundData, background, m_clip, static_cast<unsigned>(fillBox), m_clipSet, true); }
inline void RenderStyle::setBackgroundColor(const StyleColor& value) { SET_NESTED(m_nonInheritedData, backgroundData, color, value); }
inline void RenderStyle::setBackgroundOrigin(FillBox fillBox) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, backgroundData, background, m_origin, static_cast<unsigned>(fillBox), m_originSet, true); }
inline void RenderStyle::setBackgroundRepeat(FillRepeatXY fillRepeat) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, backgroundData, background, m_repeat, fillRepeat, m_repeatSet, true); }
inline void RenderStyle::setBackgroundSize(FillSizeType b) { SET_DOUBLY_NESTED(m_nonInheritedData, backgroundData, background, m_sizeType, static_cast<unsigned>(b)); }
inline void RenderStyle::setBackgroundSizeLength(LengthSize&& size) { SET_DOUBLY_NESTED(m_nonInheritedData, backgroundData, background, m_sizeLength, WTFMove(size)); }
inline void RenderStyle::setBackgroundXPosition(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, backgroundData, background, m_xPosition, WTFMove(length)); }
inline void RenderStyle::setBackgroundYPosition(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, backgroundData, background, m_yPosition, WTFMove(length)); }
inline void RenderStyle::setBlockStepInsert(BlockStepInsert newBlockStepInsert) { SET_NESTED(m_nonInheritedData, rareData, blockStepInsert, static_cast<unsigned>(newBlockStepInsert)); }
inline void RenderStyle::setBlockStepSize(std::optional<Length> length) { SET_NESTED(m_nonInheritedData, rareData, blockStepSize, WTFMove(length)); }
inline void RenderStyle::setBorderBottomColor(const StyleColor& value) { SET_NESTED_BORDER_COLOR(m_nonInheritedData, surroundData, border.m_bottom, value); }
inline void RenderStyle::setBorderBottomLeftRadius(LengthSize&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomLeft, WTFMove(size)); }
inline void RenderStyle::setBorderBottomRightRadius(LengthSize&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomRight, WTFMove(size)); }
inline void RenderStyle::setBorderBottomStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_bottom.m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderBottomWidth(float value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_bottom.m_width, value); }
inline void RenderStyle::setBorderImage(const NinePieceImage& image) { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, image); }
inline void RenderStyle::setBorderLeftColor(const StyleColor& value) { SET_NESTED_BORDER_COLOR(m_nonInheritedData, surroundData, border.m_left, value); }
inline void RenderStyle::setBorderLeftStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_left.m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderLeftWidth(float value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_left.m_width, value); }
inline void RenderStyle::setBorderRadius(const IntSize& size) { setBorderRadius({ { size.width(), LengthType::Fixed }, { size.height(), LengthType::Fixed } }); }
inline void RenderStyle::setBorderRightColor(const StyleColor& value) { SET_NESTED_BORDER_COLOR(m_nonInheritedData, surroundData, border.m_right, value); }
inline void RenderStyle::setBorderRightStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_right.m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderRightWidth(float value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_right.m_width, value); }
inline void RenderStyle::setBorderTopColor(const StyleColor& value) { SET_NESTED_BORDER_COLOR(m_nonInheritedData, surroundData, border.m_top, value); }
inline void RenderStyle::setBorderTopLeftRadius(LengthSize&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topLeft, WTFMove(size)); }
inline void RenderStyle::setBorderTopRightRadius(LengthSize&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topRight, WTFMove(size)); }
inline void RenderStyle::setBorderTopStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_top.m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderTopWidth(float value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_top.m_width, value); }
inline void RenderStyle::setBottom(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, offset.bottom(), WTFMove(length)); }
inline void RenderStyle::setBoxAlign(BoxAlignment alignment) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, align, static_cast<unsigned>(alignment)); }
inline void RenderStyle::setBoxFlex(float flex) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, flex, flex); }
inline void RenderStyle::setBoxFlexGroup(unsigned group) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, flexGroup, group); }
inline void RenderStyle::setBoxLines(BoxLines lines) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, lines, static_cast<unsigned>(lines)); }
inline void RenderStyle::setBoxOrdinalGroup(unsigned group) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, ordinalGroup, group); }
inline void RenderStyle::setBoxOrient(BoxOrient orientation) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, orient, static_cast<unsigned>(orientation)); }
inline void RenderStyle::setBoxPack(BoxPack packing) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, pack, static_cast<unsigned>(packing)); }
inline void RenderStyle::setBoxReflect(RefPtr<StyleReflection>&& reflection) { SET_NESTED(m_nonInheritedData, rareData, boxReflect, WTFMove(reflection)); }
inline void RenderStyle::setBoxSizing(BoxSizing sizing) { SET_NESTED(m_nonInheritedData, boxData, m_boxSizing, static_cast<unsigned>(sizing)); }
inline void RenderStyle::setBreakAfter(BreakBetween behavior) { SET_NESTED(m_nonInheritedData, rareData, breakAfter, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setBreakBefore(BreakBetween behavior) { SET_NESTED(m_nonInheritedData, rareData, breakBefore, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setBreakInside(BreakInside behavior) { SET_NESTED(m_nonInheritedData, rareData, breakInside, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setCapStyle(LineCap style) { SET(m_rareInheritedData, capStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setCaretColor(const StyleColor& color) { SET_PAIR(m_rareInheritedData, caretColor, color, hasAutoCaretColor, false); }
inline void RenderStyle::setClip(LengthBox&& box) { SET_NESTED(m_nonInheritedData, rareData, clip, WTFMove(box)); }
inline void RenderStyle::setClipBottom(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, clip.bottom(), WTFMove(length)); }
inline void RenderStyle::setClipLeft(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, clip.left(), WTFMove(length)); }
inline void RenderStyle::setClipRight(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, clip.right(), WTFMove(length)); }
inline void RenderStyle::setClipTop(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, clip.top(), WTFMove(length)); }
inline void RenderStyle::setColumnAxis(ColumnAxis axis) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, axis, static_cast<unsigned>(axis)); }
inline void RenderStyle::setColumnFill(ColumnFill fill) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, fill, static_cast<unsigned>(fill)); }
inline void RenderStyle::setColumnGap(GapLength&& length) { SET_NESTED(m_nonInheritedData, rareData, columnGap, WTFMove(length)); }
inline void RenderStyle::setColumnProgression(ColumnProgression progression) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, progression, static_cast<unsigned>(progression)); }
inline void RenderStyle::setColumnRuleColor(const StyleColor& c) { SET_BORDER_COLOR(m_nonInheritedData.access().miscData.access().multiCol, rule, c); }
inline void RenderStyle::setColumnRuleStyle(BorderStyle b) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule.m_style, static_cast<unsigned>(b)); }
inline void RenderStyle::setColumnRuleWidth(unsigned short w) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule.m_width, w); }
inline void RenderStyle::setColumnSpan(ColumnSpan span) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, columnSpan, static_cast<unsigned>(span)); }
inline void RenderStyle::setColumnWidth(float width) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, miscData, multiCol, width, width, autoWidth, false); }
inline void RenderStyle::setContain(OptionSet<Containment> containment) { SET_NESTED(m_nonInheritedData, rareData, contain, containment); }
inline void RenderStyle::setContainIntrinsicHeight(std::optional<Length> height) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicHeight, height); }
inline void RenderStyle::setContainIntrinsicHeightType(ContainIntrinsicSizeType containIntrinsicHeightType) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicHeightType, static_cast<unsigned>(containIntrinsicHeightType)); }
inline void RenderStyle::setContainIntrinsicWidth(std::optional<Length> width) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicWidth, width); }
inline void RenderStyle::setContainIntrinsicWidthType(ContainIntrinsicSizeType containIntrinsicWidthType) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicWidthType, static_cast<unsigned>(containIntrinsicWidthType)); }
inline void RenderStyle::setContainerNames(const Vector<Style::ScopedName>& names) { SET_NESTED(m_nonInheritedData, rareData, containerNames, names); }
inline void RenderStyle::setContainerType(ContainerType type) { SET_NESTED(m_nonInheritedData, rareData, containerType, static_cast<unsigned>(type)); }
inline void RenderStyle::setContentVisibility(ContentVisibility value) { SET_NESTED(m_nonInheritedData, rareData, contentVisibility, static_cast<unsigned>(value)); }
inline void RenderStyle::setEffectiveAppearance(StyleAppearance a) { SET_NESTED(m_nonInheritedData, miscData, effectiveAppearance, static_cast<unsigned>(a)); }
inline void RenderStyle::setEffectiveInert(bool effectiveInert) { SET(m_rareInheritedData, effectiveInert, effectiveInert); }
inline void RenderStyle::setEffectiveTouchActions(OptionSet<TouchAction> touchActions) { SET(m_rareInheritedData, effectiveTouchActions, touchActions); }
inline void RenderStyle::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes) { SET(m_rareInheritedData, eventListenerRegionTypes, eventListenerTypes); }
inline void RenderStyle::setFilter(const FilterOperations& ops) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, filter, operations, ops); }
inline void RenderStyle::setFlexBasis(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexBasis, WTFMove(length)); }
inline void RenderStyle::setFlexDirection(FlexDirection direction) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexDirection, static_cast<unsigned>(direction)); }
inline void RenderStyle::setFlexWrap(FlexWrap wrap) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexWrap, static_cast<unsigned>(wrap)); }
inline void RenderStyle::setGridAutoColumns(const Vector<GridTrackSize>& trackSizeList) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, gridAutoColumns, trackSizeList); }
inline void RenderStyle::setGridAutoFlow(GridAutoFlow flow) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, gridAutoFlow, flow); }
inline void RenderStyle::setGridAutoRows(const Vector<GridTrackSize>& trackSizeList) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, gridAutoRows, trackSizeList); }
inline void RenderStyle::setGridItemColumnEnd(const GridPosition& columnEndPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridColumnEnd, columnEndPosition); }
inline void RenderStyle::setGridItemColumnStart(const GridPosition& columnStartPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridColumnStart, columnStartPosition); }
inline void RenderStyle::setGridItemRowEnd(const GridPosition& rowEndPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridRowEnd, rowEndPosition); }
inline void RenderStyle::setGridItemRowStart(const GridPosition& rowStartPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridRowStart, rowStartPosition); }
inline void RenderStyle::setHangingPunctuation(OptionSet<HangingPunctuation> punctuation) { SET(m_rareInheritedData, hangingPunctuation, punctuation.toRaw()); }
inline void RenderStyle::setHasAutoAccentColor() { SET_PAIR(m_rareInheritedData, hasAutoAccentColor, true, accentColor, StyleColor::currentColor()); }
inline void RenderStyle::setHasAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasAutoCaretColor, true, caretColor, StyleColor::currentColor()); }
inline void RenderStyle::setHasAutoColumnCount() { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, miscData, multiCol, autoCount, true, count, initialColumnCount()); }
inline void RenderStyle::setHasAutoColumnWidth() { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, miscData, multiCol, autoWidth, true, width, 0); }
inline void RenderStyle::setHasAutoOrphans() { SET_PAIR(m_rareInheritedData, hasAutoOrphans, true, orphans, initialOrphans()); }
inline void RenderStyle::setHasAutoSpecifiedZIndex() { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_hasAutoSpecifiedZIndex, true, m_specifiedZIndex, 0); }
inline void RenderStyle::setHasAutoUsedZIndex() { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_hasAutoUsedZIndex, true, m_usedZIndex, 0); }
inline void RenderStyle::setHasAutoWidows() { SET_PAIR(m_rareInheritedData, hasAutoWidows, true, widows, initialWidows()); }
inline void RenderStyle::setHasClip(bool hasClip) { SET_NESTED(m_nonInheritedData, rareData, hasClip, hasClip); }
inline void RenderStyle::setHasContentNone(bool value) { m_nonInheritedFlags.hasContentNone = value; }
inline void RenderStyle::setHasExplicitlySetBorderBottomLeftRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderBottomLeftRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderBottomRightRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderBottomRightRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderTopLeftRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderTopLeftRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderTopRightRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderTopRightRadius, value); }
inline void RenderStyle::setHasExplicitlySetStrokeColor(bool value) { SET(m_rareInheritedData, hasSetStrokeColor, static_cast<unsigned>(value)); }
inline void RenderStyle::setHasExplicitlySetStrokeWidth(bool value) { SET(m_rareInheritedData, hasSetStrokeWidth, static_cast<unsigned>(value)); }
inline void RenderStyle::setHasPseudoStyles(PseudoIdSet set) { m_nonInheritedFlags.setHasPseudoStyles(set); }
inline void RenderStyle::setHasVisitedLinkAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, true, visitedLinkCaretColor, StyleColor::currentColor()); }
inline void RenderStyle::setHeight(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_height, WTFMove(length)); }
inline void RenderStyle::setHyphenationLimitAfter(short limit) { SET(m_rareInheritedData, hyphenationLimitAfter, limit); }
inline void RenderStyle::setHyphenationLimitBefore(short limit) { SET(m_rareInheritedData, hyphenationLimitBefore, limit); }
inline void RenderStyle::setHyphenationLimitLines(short limit) { SET(m_rareInheritedData, hyphenationLimitLines, limit); }
inline void RenderStyle::setHyphenationString(const AtomString& h) { SET(m_rareInheritedData, hyphenationString, h); }
inline void RenderStyle::setHyphens(Hyphens hyphens) { SET(m_rareInheritedData, hyphens, static_cast<unsigned>(hyphens)); }
inline void RenderStyle::setImageOrientation(ImageOrientation value) { SET(m_rareInheritedData, imageOrientation, static_cast<unsigned>(value)); }
inline void RenderStyle::setImageRendering(ImageRendering value) { SET(m_rareInheritedData, imageRendering, static_cast<unsigned>(value)); }
inline void RenderStyle::setImplicitNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, implicitNamedGridColumnLines, namedGridColumnLines); }
inline void RenderStyle::setImplicitNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, implicitNamedGridRowLines, namedGridRowLines); }
inline void RenderStyle::setInitialLetter(const IntSize& size) { SET_NESTED(m_nonInheritedData, rareData, initialLetter, size); }
inline void RenderStyle::setInputSecurity(InputSecurity security) { SET_NESTED(m_nonInheritedData, rareData, inputSecurity, static_cast<unsigned>(security)); }
inline void RenderStyle::setJoinStyle(LineJoin style) { SET(m_rareInheritedData, joinStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setJustifyContent(const StyleContentAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifyContent, data); }
inline void RenderStyle::setJustifyContentPosition(ContentPosition position) { m_nonInheritedData.access().miscData.access().justifyContent.setPosition(position); }
inline void RenderStyle::setJustifyItems(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifyItems, data); }
inline void RenderStyle::setJustifySelf(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifySelf, data); }
inline void RenderStyle::setJustifySelfPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().justifySelf.setPosition(position); }
inline void RenderStyle::setLeft(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, offset.left(), WTFMove(length)); }
inline void RenderStyle::setLineAlign(LineAlign alignment) { SET(m_rareInheritedData, lineAlign, static_cast<unsigned>(alignment)); }
inline void RenderStyle::setLineBoxContain(OptionSet<LineBoxContain> c) { SET(m_rareInheritedData, lineBoxContain, c.toRaw()); }
inline void RenderStyle::setLineBreak(LineBreak rule) { SET(m_rareInheritedData, lineBreak, static_cast<unsigned>(rule)); }
inline void RenderStyle::setLineClamp(LineClampValue value) { SET_NESTED(m_nonInheritedData, rareData, lineClamp, value); }
inline void RenderStyle::setLineGrid(const AtomString& name) { SET(m_rareInheritedData, lineGrid, name); }
inline void RenderStyle::setLineSnap(LineSnap snap) { SET(m_rareInheritedData, lineSnap, static_cast<unsigned>(snap)); }
inline void RenderStyle::setListStyleType(ListStyleType value) { SET(m_rareInheritedData, listStyleType, value); }
inline void RenderStyle::setMarginBottom(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, margin.bottom(), WTFMove(length)); }
inline void RenderStyle::setMarginLeft(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, margin.left(), WTFMove(length)); }
inline void RenderStyle::setMarginRight(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, margin.right(), WTFMove(length)); }
inline void RenderStyle::setMarginTop(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, margin.top(), WTFMove(length)); }
inline void RenderStyle::setMarginTrim(OptionSet<MarginTrimType> value) { SET_NESTED(m_nonInheritedData, rareData, marginTrim, value); }
inline void RenderStyle::setMarqueeBehavior(MarqueeBehavior b) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, behavior, static_cast<unsigned>(b)); }
inline void RenderStyle::setMarqueeDirection(MarqueeDirection d) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, direction, static_cast<unsigned>(d)); }
inline void RenderStyle::setMarqueeIncrement(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, increment, WTFMove(length)); }
inline void RenderStyle::setMarqueeLoopCount(int i) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, loops, i); }
inline void RenderStyle::setMarqueeSpeed(int speed) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, speed, speed); }
inline void RenderStyle::setMaskBorder(const NinePieceImage& image) { SET_NESTED(m_nonInheritedData, rareData, maskBorder, image); }
inline void RenderStyle::setMaskBorderSource(RefPtr<StyleImage>&& value) { m_nonInheritedData.access().rareData.access().maskBorder.setImage(WTFMove(value)); }
inline void RenderStyle::setMaskImage(RefPtr<StyleImage>&& image) { m_nonInheritedData.access().miscData.access().mask.access().setImage(WTFMove(image)); }
inline void RenderStyle::setMaskRepeat(FillRepeatXY repeat) { SET_DOUBLY_NESTED_PAIR(m_nonInheritedData, miscData, mask, m_repeat, repeat, m_repeatSet, true); }
inline void RenderStyle::setMaskSize(LengthSize size) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, mask, m_sizeLength, WTFMove(size)); }
inline void RenderStyle::setMaskXPosition(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, mask, m_xPosition, WTFMove(length)); }
inline void RenderStyle::setMaskYPosition(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, mask, m_yPosition, WTFMove(length)); }
inline void RenderStyle::setMasonryAutoFlow(MasonryAutoFlow flow) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, masonryAutoFlow, flow); }
inline void RenderStyle::setMathStyle(const MathStyle& style) { SET(m_rareInheritedData, mathStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setMaxHeight(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_maxHeight, WTFMove(length)); }
inline void RenderStyle::setMaxWidth(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_maxWidth, WTFMove(length)); }
inline void RenderStyle::setMinHeight(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_minHeight, WTFMove(length)); }
inline void RenderStyle::setMinWidth(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_minWidth, WTFMove(length)); }
inline void RenderStyle::setNBSPMode(NBSPMode mode) { SET(m_rareInheritedData, nbspMode, static_cast<unsigned>(mode)); }
inline void RenderStyle::setNamedGridArea(const NamedGridAreaMap& map) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, namedGridArea, map); }
inline void RenderStyle::setNamedGridAreaColumnCount(size_t columnCount) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, namedGridAreaColumnCount, columnCount); }
inline void RenderStyle::setNamedGridAreaRowCount(size_t rowCount) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, namedGridAreaRowCount, rowCount); }
inline void RenderStyle::setObjectFit(ObjectFit fit) { SET_NESTED(m_nonInheritedData, miscData, objectFit, static_cast<unsigned>(fit)); }
inline void RenderStyle::setObjectPosition(LengthPoint position) { SET_NESTED(m_nonInheritedData, miscData, objectPosition, WTFMove(position)); }
inline void RenderStyle::setOffsetAnchor(LengthPoint position) { SET_NESTED(m_nonInheritedData, rareData, offsetAnchor, WTFMove(position)); }
inline void RenderStyle::setOffsetDistance(Length&& distance) { SET_NESTED(m_nonInheritedData, rareData, offsetDistance, WTFMove(distance)); }
inline void RenderStyle::setOffsetPath(RefPtr<PathOperation>&& path) { SET_NESTED(m_nonInheritedData, rareData, offsetPath, WTFMove(path)); }
inline void RenderStyle::setOffsetPosition(LengthPoint position) { SET_NESTED(m_nonInheritedData, rareData, offsetPosition, WTFMove(position)); }
inline void RenderStyle::setOffsetRotate(OffsetRotation&& rotation) { SET_NESTED(m_nonInheritedData, rareData, offsetRotate, WTFMove(rotation)); }
inline void RenderStyle::setOrder(int o) { SET_NESTED(m_nonInheritedData, miscData, order, o); }
inline void RenderStyle::setOutlineColor(const StyleColor& color) { SET_NESTED_BORDER_COLOR(m_nonInheritedData, backgroundData, outline, color); }
inline void RenderStyle::setOutlineOffset(float offset) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_offset, offset); }
inline void RenderStyle::setOutlineStyle(BorderStyle style) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_style, static_cast<unsigned>(style)); }
inline void RenderStyle::setOutlineStyleIsAuto(OutlineIsAuto isAuto) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_isAuto, static_cast<unsigned>(isAuto)); }
inline void RenderStyle::setOutlineWidth(float width) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_width, width); }
inline void RenderStyle::setOverflowAnchor(OverflowAnchor a) { SET_NESTED(m_nonInheritedData, rareData, overflowAnchor, static_cast<unsigned>(a)); }
inline void RenderStyle::setOverflowWrap(OverflowWrap rule) { SET(m_rareInheritedData, overflowWrap, static_cast<unsigned>(rule)); }
inline void RenderStyle::setOverscrollBehaviorX(OverscrollBehavior value) { SET_NESTED(m_nonInheritedData, rareData, overscrollBehaviorX, static_cast<unsigned>(value)); }
inline void RenderStyle::setOverscrollBehaviorY(OverscrollBehavior value) { SET_NESTED(m_nonInheritedData, rareData, overscrollBehaviorY, static_cast<unsigned>(value)); }
inline void RenderStyle::setPaddingBottom(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, padding.bottom(), WTFMove(length)); }
inline void RenderStyle::setPaddingBox(LengthBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, padding, WTFMove(box)); }
inline void RenderStyle::setPaddingLeft(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, padding.left(), WTFMove(length)); }
inline void RenderStyle::setPaddingRight(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, padding.right(), WTFMove(length)); }
inline void RenderStyle::setPaddingTop(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, padding.top(), WTFMove(length)); }
inline void RenderStyle::setPageSize(LengthSize size) { SET_NESTED(m_nonInheritedData, rareData, pageSize, WTFMove(size)); }
inline void RenderStyle::setPageSizeType(PageSizeType type) { SET_NESTED(m_nonInheritedData, rareData, pageSizeType, type); }
inline void RenderStyle::setPaintOrder(PaintOrder order) { SET(m_rareInheritedData, paintOrder, static_cast<unsigned>(order)); }
inline void RenderStyle::setPerspective(float perspective) { SET_NESTED(m_nonInheritedData, rareData, perspective, perspective); }
inline void RenderStyle::setPerspectiveOriginX(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOriginX, WTFMove(length)); }
inline void RenderStyle::setPerspectiveOriginY(Length&& length) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOriginY, WTFMove(length)); }
inline void RenderStyle::setResize(Resize r) { SET_NESTED(m_nonInheritedData, miscData, resize, static_cast<unsigned>(r)); }
inline void RenderStyle::setRight(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, offset.right(), WTFMove(length)); }
inline void RenderStyle::setRowGap(GapLength&& gapLength) { SET_NESTED(m_nonInheritedData, rareData, rowGap, WTFMove(gapLength)); }
inline void RenderStyle::setRubyPosition(RubyPosition position) { SET(m_rareInheritedData, rubyPosition, static_cast<unsigned>(position)); }
inline void RenderStyle::setScrollTimelineAxes(const Vector<ScrollAxis>& axes) { SET_NESTED(m_nonInheritedData, rareData, scrollTimelineAxes, axes); }
inline void RenderStyle::setScrollTimelineNames(const Vector<AtomString>& names) { SET_NESTED(m_nonInheritedData, rareData, scrollTimelineNames, names); }
inline void RenderStyle::setViewTimelineAxes(const Vector<ScrollAxis>& axes) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineAxes, axes); }
inline void RenderStyle::setViewTimelineInsets(const Vector<ViewTimelineInsets>& insets) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineInsets, insets); }
inline void RenderStyle::setViewTimelineNames(const Vector<AtomString>& names) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineNames, names); }
inline void RenderStyle::setScrollbarColor(const std::optional<ScrollbarColor>& color) { SET(m_rareInheritedData, scrollbarColor, color); }
inline void RenderStyle::setScrollbarThumbColor(const StyleColor& color) { m_rareInheritedData.access().scrollbarColor->thumbColor = color; }
inline void RenderStyle::setScrollbarTrackColor(const StyleColor& color) { m_rareInheritedData.access().scrollbarColor->trackColor = color; }
inline void RenderStyle::setShapeMargin(Length&& margin) { SET_NESTED(m_nonInheritedData, rareData, shapeMargin, WTFMove(margin)); }
inline void RenderStyle::setEffectiveContentVisibility(ContentVisibility effectiveContentVisibility) { SET(m_rareInheritedData, effectiveContentVisibility, static_cast<unsigned>(effectiveContentVisibility)); }
inline void RenderStyle::setSpeakAs(OptionSet<SpeakAs> style) { SET(m_rareInheritedData, speakAs, style.toRaw()); }
inline void RenderStyle::setSpecifiedZIndex(int value) { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_hasAutoSpecifiedZIndex, false, m_specifiedZIndex, value); }
inline void RenderStyle::setStrokeColor(const StyleColor& color) { SET(m_rareInheritedData, strokeColor, color); }
inline void RenderStyle::setStrokeMiterLimit(float value) { SET(m_rareInheritedData, miterLimit, value); }
inline void RenderStyle::setStrokeWidth(Length&& width) { SET(m_rareInheritedData, strokeWidth, WTFMove(width)); }
inline void RenderStyle::setTabSize(const TabSize& size) { SET(m_rareInheritedData, tabSize, size); }
inline void RenderStyle::setTextAlignLast(TextAlignLast value) { SET(m_rareInheritedData, textAlignLast, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextBoxTrim(TextBoxTrim value) { SET_NESTED(m_nonInheritedData, rareData, textBoxTrim, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextCombine(TextCombine value) { SET(m_rareInheritedData, textCombine, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextDecorationColor(const StyleColor& color) { SET_NESTED(m_nonInheritedData, rareData, textDecorationColor, color); }
inline void RenderStyle::setTextDecorationLine(OptionSet<TextDecorationLine> value) { m_nonInheritedFlags.textDecorationLine = value.toRaw(); }
inline void RenderStyle::setTextDecorationSkipInk(TextDecorationSkipInk skipInk) { SET(m_rareInheritedData, textDecorationSkipInk, static_cast<unsigned>(skipInk)); }
inline void RenderStyle::setTextDecorationStyle(TextDecorationStyle value) { SET_NESTED(m_nonInheritedData, rareData, textDecorationStyle, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextDecorationThickness(TextDecorationThickness textDecorationThickness) { SET_NESTED(m_nonInheritedData, rareData, textDecorationThickness, textDecorationThickness); }
inline void RenderStyle::setTextDecorationsInEffect(OptionSet<TextDecorationLine> value) { m_inheritedFlags.textDecorationLines = value.toRaw(); }
inline void RenderStyle::setTextEmphasisColor(const StyleColor& c) { SET(m_rareInheritedData, textEmphasisColor, c); }
inline void RenderStyle::setTextEmphasisCustomMark(const AtomString& mark) { SET(m_rareInheritedData, textEmphasisCustomMark, mark); }
inline void RenderStyle::setTextEmphasisFill(TextEmphasisFill fill) { SET(m_rareInheritedData, textEmphasisFill, static_cast<unsigned>(fill)); }
inline void RenderStyle::setTextEmphasisMark(TextEmphasisMark mark) { SET(m_rareInheritedData, textEmphasisMark, static_cast<unsigned>(mark)); }
inline void RenderStyle::setTextEmphasisPosition(OptionSet<TextEmphasisPosition> position) { SET(m_rareInheritedData, textEmphasisPosition, static_cast<unsigned>(position.toRaw())); }
inline void RenderStyle::setTextFillColor(const StyleColor& color) { SET(m_rareInheritedData, textFillColor, color); }
inline void RenderStyle::setHasExplicitlySetColor(bool value) { m_inheritedFlags.hasExplicitlySetColor = value; }
inline void RenderStyle::setTextGroupAlign(TextGroupAlign value) { SET_NESTED(m_nonInheritedData, rareData, textGroupAlign, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextIndent(Length&& length) { SET(m_rareInheritedData, indent, WTFMove(length)); }
inline void RenderStyle::setTextIndentLine(TextIndentLine value) { SET(m_rareInheritedData, textIndentLine, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextIndentType(TextIndentType value) { SET(m_rareInheritedData, textIndentType, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextJustify(TextJustify value) { SET(m_rareInheritedData, textJustify, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextOverflow(TextOverflow overflow) { SET_NESTED(m_nonInheritedData, miscData, textOverflow, static_cast<unsigned>(overflow)); }
inline void RenderStyle::setTextSecurity(TextSecurity security) { SET(m_rareInheritedData, textSecurity, static_cast<unsigned>(security)); }
inline void RenderStyle::setTextStrokeColor(const StyleColor& c) { SET(m_rareInheritedData, textStrokeColor, c); }
inline void RenderStyle::setTextStrokeWidth(float value) { SET(m_rareInheritedData, textStrokeWidth, value); }
inline void RenderStyle::setTextTransform(OptionSet<TextTransform> value) { m_inheritedFlags.textTransform = value.toRaw(); }
inline void RenderStyle::setTextUnderlineOffset(TextUnderlineOffset textUnderlineOffset) { SET(m_rareInheritedData, textUnderlineOffset, textUnderlineOffset); }
inline void RenderStyle::setTextUnderlinePosition(TextUnderlinePosition position) { SET(m_rareInheritedData, textUnderlinePosition, static_cast<unsigned>(position)); }
inline void RenderStyle::setTextZoom(TextZoom zoom) { SET(m_rareInheritedData, textZoom, static_cast<unsigned>(zoom)); }
inline void RenderStyle::setTop(Length&& length) { SET_NESTED(m_nonInheritedData, surroundData, offset.top(), WTFMove(length)); }
inline void RenderStyle::setTouchActions(OptionSet<TouchAction> actions) { SET_NESTED(m_nonInheritedData, rareData, touchActions, actions); }
inline void RenderStyle::setTransform(const TransformOperations& operations) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, operations, operations); }
inline void RenderStyle::setTransformBox(TransformBox box) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, transformBox, box); }
inline void RenderStyle::setTransformOriginX(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, x, WTFMove(length)); }
inline void RenderStyle::setTransformOriginY(Length&& length) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, y, WTFMove(length)); }
inline void RenderStyle::setTransformOriginZ(float value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, z, value); }
inline void RenderStyle::setTransformStyle3D(TransformStyle3D b) { SET_NESTED(m_nonInheritedData, rareData, transformStyle3D, static_cast<unsigned>(b)); }
inline void RenderStyle::setTransformStyleForcedToFlat(bool b) { SET_NESTED(m_nonInheritedData, rareData, transformStyleForcedToFlat, static_cast<unsigned>(b)); }
inline void RenderStyle::setUseSmoothScrolling(bool value) { SET_NESTED(m_nonInheritedData, rareData, useSmoothScrolling, value); }
inline void RenderStyle::setUsedZIndex(int index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_usedZIndex, index, m_hasAutoUsedZIndex, false); }
inline void RenderStyle::setUserDrag(UserDrag value) { SET_NESTED(m_nonInheritedData, miscData, userDrag, static_cast<unsigned>(value)); }
inline void RenderStyle::setUserModify(UserModify value) { SET(m_rareInheritedData, userModify, static_cast<unsigned>(value)); }
inline void RenderStyle::setUserSelect(UserSelect value) { SET(m_rareInheritedData, userSelect, static_cast<unsigned>(value)); }
inline void RenderStyle::setViewTransitionName(std::optional<Style::ScopedName> value) { SET_NESTED(m_nonInheritedData, rareData, viewTransitionName, value); }
inline void RenderStyle::setVisitedLinkBackgroundColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, background, value); }
inline void RenderStyle::setVisitedLinkBorderBottomColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderBottom, value); }
inline void RenderStyle::setVisitedLinkBorderLeftColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderLeft, value); }
inline void RenderStyle::setVisitedLinkBorderRightColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderRight, value); }
inline void RenderStyle::setVisitedLinkBorderTopColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderTop, value); }
inline void RenderStyle::setVisitedLinkCaretColor(const StyleColor& value) { SET_PAIR(m_rareInheritedData, visitedLinkCaretColor, value, hasVisitedLinkAutoCaretColor, false); }
inline void RenderStyle::setVisitedLinkColumnRuleColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, visitedLinkColumnRuleColor, value); }
inline void RenderStyle::setVisitedLinkOutlineColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, outline, value); }
inline void RenderStyle::setVisitedLinkStrokeColor(const StyleColor& value) { SET(m_rareInheritedData, visitedLinkStrokeColor, value); }
inline void RenderStyle::setVisitedLinkTextDecorationColor(const StyleColor& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, textDecoration, value); }
inline void RenderStyle::setVisitedLinkTextEmphasisColor(const StyleColor& value) { SET(m_rareInheritedData, visitedLinkTextEmphasisColor, value); }
inline void RenderStyle::setVisitedLinkTextFillColor(const StyleColor& value) { SET(m_rareInheritedData, visitedLinkTextFillColor, value); }
inline void RenderStyle::setVisitedLinkTextStrokeColor(const StyleColor& value) { SET(m_rareInheritedData, visitedLinkTextStrokeColor, value); }
inline void RenderStyle::setWidth(Length&& length) { SET_NESTED(m_nonInheritedData, boxData, m_width, WTFMove(length)); }
inline void RenderStyle::setWordBreak(WordBreak rule) { SET(m_rareInheritedData, wordBreak, static_cast<unsigned>(rule)); }

#if ENABLE(APPLE_PAY)
inline void RenderStyle::setApplePayButtonStyle(ApplePayButtonStyle style) { SET_NESTED(m_nonInheritedData, rareData, applePayButtonStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setApplePayButtonType(ApplePayButtonType type) { SET_NESTED(m_nonInheritedData, rareData, applePayButtonType, static_cast<unsigned>(type)); }
#endif

inline void RenderStyle::setBoxDecorationBreak(BoxDecorationBreak value) { SET_NESTED(m_nonInheritedData, boxData, m_boxDecorationBreak, static_cast<unsigned>(value)); }

#if ENABLE(DARK_MODE_CSS)
inline void RenderStyle::setColorScheme(StyleColorScheme scheme) { SET(m_rareInheritedData, colorScheme, scheme); }
inline void RenderStyle::setHasExplicitlySetColorScheme() { SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetColorScheme, true); }
#endif

inline FilterOperations& RenderStyle::mutableBackdropFilter() { return m_nonInheritedData.access().rareData.access().backdropFilter.access().operations; }
inline void RenderStyle::setBackdropFilter(const FilterOperations& ops) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, backdropFilter, operations, ops); }

#if PLATFORM(IOS_FAMILY)
inline void RenderStyle::setTouchCalloutEnabled(bool value) { SET(m_rareInheritedData, touchCalloutEnabled, value); }
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline void RenderStyle::setUseTouchOverflowScrolling(bool value) { SET(m_rareInheritedData, useTouchOverflowScrolling, value); }
#endif

#if ENABLE(TEXT_AUTOSIZING)
inline void RenderStyle::setTextSizeAdjust(TextSizeAdjustment adjustment) { SET(m_rareInheritedData, textSizeAdjust, adjustment); }
#endif

#if ENABLE(TOUCH_EVENTS)
inline void RenderStyle::setTapHighlightColor(const StyleColor& color) { SET(m_rareInheritedData, tapHighlightColor, color); }
#endif

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyles(PseudoIdSet pseudoIdSet)
{
    ASSERT(pseudoIdSet);
    ASSERT((pseudoIdSet.data() & static_cast<unsigned>(PseudoId::PublicPseudoIdMask)) == pseudoIdSet.data());
    pseudoBits |= pseudoIdSet.data() >> 1; // Shift down as we do not store a bit for PseudoId::None.
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
        auto& maskLayers = ensureMaskLayers();
        maskLayers.cullEmptyLayers();
        maskLayers.fillUnsetProperties();
    }
}

inline void RenderStyle::resetBorder()
{
    resetBorderExceptRadius();
    resetBorderRadius();
}

inline void RenderStyle::resetBorderExceptRadius()
{
    resetBorderImage();
    resetBorderTop();
    resetBorderRight();
    resetBorderBottom();
    resetBorderLeft();
}

inline void RenderStyle::resetBorderRadius()
{
    resetBorderTopLeftRadius();
    resetBorderTopRightRadius();
    resetBorderBottomLeftRadius();
    resetBorderBottomRightRadius();
}

inline void RenderStyle::setBorderRadius(LengthSize&& size)
{
    setBorderTopLeftRadius(LengthSize { size });
    setBorderTopRightRadius(LengthSize { size });
    setBorderBottomLeftRadius(LengthSize { size });
    setBorderBottomRightRadius(WTFMove(size));
}

inline void RenderStyle::setClipPath(RefPtr<PathOperation>&& operation)
{
    if (m_nonInheritedData->rareData->clipPath != operation)
        m_nonInheritedData.access().rareData.access().clipPath = WTFMove(operation);
}

inline void RenderStyle::setColumnCount(unsigned short count)
{
    unsigned short clampedCount = std::max<unsigned short>(count, 1);
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, count, clampedCount);
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, autoCount, false);
}

inline bool RenderStyle::setEffectiveZoom(float zoomLevel)
{
    if (compareEqual(m_rareInheritedData->effectiveZoom, zoomLevel))
        return false;
    m_rareInheritedData.access().effectiveZoom = zoomLevel;
    return true;
}

inline void RenderStyle::setFlexGrow(float grow)
{
    float clampedGrow = std::max(grow, 0.f);
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexGrow, clampedGrow);
}

inline void RenderStyle::setFlexShrink(float shrink)
{
    float clampedShrink = std::max(shrink, 0.f);
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexShrink, clampedShrink);
}

inline void RenderStyle::setPseudoElementNameArgument(const AtomString& identifier)
{
    ASSERT(pseudoElementType() == PseudoId::ViewTransitionGroup
        || pseudoElementType() == PseudoId::ViewTransitionImagePair
        || pseudoElementType() == PseudoId::ViewTransitionNew
        || pseudoElementType() == PseudoId::ViewTransitionOld
        || pseudoElementType() == PseudoId::Highlight);
    SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, identifier);
}

inline void RenderStyle::setGridColumnList(const GridTrackList& list)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->columns(), list))
        m_nonInheritedData.access().rareData.access().grid.access().setColumns(list);
}

inline void RenderStyle::setGridRowList(const GridTrackList& list)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->rows(), list))
        m_nonInheritedData.access().rareData.access().grid.access().setRows(list);
}

inline void RenderStyle::setHasExplicitlySetDirection()
{
    SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetDirection, true);
}

inline void RenderStyle::setHasExplicitlySetWritingMode()
{
    SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetWritingMode, true);
}

inline void RenderStyle::setLogicalHeight(Length&& height)
{
    if (isHorizontalWritingMode())
        setHeight(WTFMove(height));
    else
        setWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalWidth(Length&& width)
{
    if (isHorizontalWritingMode())
        setWidth(WTFMove(width));
    else
        setHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinWidth(Length&& width)
{
    if (isHorizontalWritingMode())
        setMinWidth(WTFMove(width));
    else
        setMinHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMaxWidth(Length&& width)
{
    if (isHorizontalWritingMode())
        setMaxWidth(WTFMove(width));
    else
        setMaxHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinHeight(Length&& height)
{
    if (isHorizontalWritingMode())
        setMinHeight(WTFMove(height));
    else
        setMinWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalMaxHeight(Length&& height)
{
    if (isHorizontalWritingMode())
        setMaxHeight(WTFMove(height));
    else
        setMaxWidth(WTFMove(height));
}

inline void RenderStyle::setOpacity(float opacity)
{
    float clampedOpacity = clampTo(opacity, 0.f, 1.f);
    SET_NESTED(m_nonInheritedData, miscData, opacity, clampedOpacity);
}

inline void RenderStyle::setOrphans(unsigned short count)
{
    unsigned short clampedCount = std::max<unsigned short>(count, 1);
    SET_PAIR(m_rareInheritedData, orphans, clampedCount, hasAutoOrphans, false);
}

inline void RenderStyle::setShapeImageThreshold(float shapeImageThreshold)
{
    float clampedShapeImageThreshold = clampTo<float>(shapeImageThreshold, 0.f, 1.f);
    SET_NESTED(m_nonInheritedData, rareData, shapeImageThreshold, clampedShapeImageThreshold);
}

inline void RenderStyle::setShapeOutside(RefPtr<ShapeValue>&& value)
{
    if (m_nonInheritedData->rareData->shapeOutside == value)
        return;
    m_nonInheritedData.access().rareData.access().shapeOutside = WTFMove(value);
}

inline bool RenderStyle::setTextOrientation(TextOrientation textOrientation)
{
    if (compareEqual(static_cast<TextOrientation>(m_rareInheritedData->textOrientation), textOrientation))
        return false;
    m_rareInheritedData.access().textOrientation = static_cast<unsigned>(textOrientation);
    return true;
}

inline void RenderStyle::setVerticalAlign(VerticalAlign align)
{
    SET_NESTED(m_nonInheritedData, boxData, m_verticalAlign, static_cast<unsigned>(align));
}

inline void RenderStyle::setVerticalAlignLength(Length&& length)
{
    setVerticalAlign(VerticalAlign::Length);
    SET_NESTED(m_nonInheritedData, boxData, m_verticalAlignLength, WTFMove(length));
}

inline void RenderStyle::setWidows(unsigned short count)
{
    auto clampedCount = std::max<unsigned short>(count, 1);
    SET_PAIR(m_rareInheritedData, widows, clampedCount, hasAutoWidows, false);
}

inline bool RenderStyle::setWritingMode(WritingMode mode)
{
    if (mode == writingMode())
        return false;
    m_inheritedFlags.writingMode = static_cast<unsigned>(mode);
    return true;
}

inline bool RenderStyle::setZoom(float zoomLevel)
{
    setEffectiveZoom(effectiveZoom() * zoomLevel);
    if (compareEqual(m_nonInheritedData->rareData->zoom, zoomLevel))
        return false;
    m_nonInheritedData.access().rareData.access().zoom = zoomLevel;
    return true;
}

inline void RenderStyle::containIntrinsicWidthAddAuto()
{
    if (containIntrinsicWidthType() == ContainIntrinsicSizeType::None)
        setContainIntrinsicWidthType(ContainIntrinsicSizeType::AutoAndNone);
    else if (containIntrinsicWidthType() == ContainIntrinsicSizeType::Length)
        setContainIntrinsicWidthType(ContainIntrinsicSizeType::AutoAndLength);
}

inline void RenderStyle::containIntrinsicHeightAddAuto()
{
    if (containIntrinsicHeightType() == ContainIntrinsicSizeType::None)
        setContainIntrinsicHeightType(ContainIntrinsicSizeType::AutoAndNone);
    else if (containIntrinsicHeightType() == ContainIntrinsicSizeType::Length)
        setContainIntrinsicHeightType(ContainIntrinsicSizeType::AutoAndLength);
}

inline void RenderStyle::setBlendMode(BlendMode mode)
{
    SET_NESTED(m_nonInheritedData, rareData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET(m_rareInheritedData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}

inline void RenderStyle::setIsolation(Isolation isolation) { SET_NESTED(m_nonInheritedData, rareData, isolation, static_cast<unsigned>(isolation)); }

#undef SET
#undef SET_BORDER_COLOR
#undef SET_DOUBLY_NESTED
#undef SET_DOUBLY_NESTED_PAIR
#undef SET_NESTED
#undef SET_NESTED_BORDER_COLOR
#undef SET_NESTED_BORDER_VALUE_COLOR
#undef SET_NESTED_PAIR
#undef SET_PAIR
#undef SET_STYLE_PROPERTY
#undef SET_STYLE_PROPERTY_BASE
#undef SET_STYLE_PROPERTY_PAIR
#undef SET_STYLE_PROPERTY_WITH_FUNCTIONS

} // namespace WebCore

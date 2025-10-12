/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "PathOperation.h"
#include "RenderStyleInlines.h"

namespace WebCore {

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)

#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)

#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { auto& readable = *read; if (!compareEqual(readable.variable1, value1) || !compareEqual(readable.variable2, value2)) { auto& writable = write; writable.variable1 = value1; writable.variable2 = value2; } } while (0)

#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

template<typename T, typename U> inline bool compareEqual(const T& a, const U& b) { return a == b; }

inline void RenderStyle::addToTextDecorationLineInEffect(const Style::TextDecorationLine& value) { m_inheritedFlags.textDecorationLineInEffect = textDecorationLineInEffect().addOrReplaceIfNotNone(value); }
inline void RenderStyle::clearAnimations() { m_nonInheritedData.access().miscData.access().animations = CSS::Keyword::None { }; }
inline void RenderStyle::clearTransitions() { m_nonInheritedData.access().miscData.access().transitions = CSS::Keyword::None { }; }
inline Style::Animations& RenderStyle::ensureAnimations() { return m_nonInheritedData.access().miscData.access().animations.access(); }
inline Style::Transitions& RenderStyle::ensureTransitions() { return m_nonInheritedData.access().miscData.access().transitions.access(); }
inline Style::BackgroundLayers& RenderStyle::ensureBackgroundLayers() { return m_nonInheritedData.access().backgroundData.access().background.access(); }
inline Style::MaskLayers& RenderStyle::ensureMaskLayers() { return m_nonInheritedData.access().miscData.access().mask.access(); }
inline void RenderStyle::inheritColumnPropertiesFrom(const RenderStyle& parent) { m_nonInheritedData.access().miscData.access().multiCol = parent.m_nonInheritedData->miscData->multiCol; }
inline void RenderStyle::resetBorderBottom() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.bottom(), BorderValue()); }
inline void RenderStyle::resetBorderBottomLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderBottomRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomRight(), initialBorderRadius()); }
inline void RenderStyle::resetBorderImage() { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, Style::BorderImage()); }
inline void RenderStyle::resetBorderLeft() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.left(), BorderValue()); }
inline void RenderStyle::resetBorderRight() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.right(), BorderValue()); }
inline void RenderStyle::resetBorderTop() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.top(), BorderValue { }); }
inline void RenderStyle::resetBorderTopLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderTopRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topRight(), initialBorderRadius()); }
inline void RenderStyle::resetColumnRule() { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule, BorderValue()); }
inline void RenderStyle::resetMargin() { SET_NESTED(m_nonInheritedData, surroundData, margin, Style::MarginBox { 0_css_px }); }
inline void RenderStyle::resetPadding() { SET_NESTED(m_nonInheritedData, surroundData, padding, Style::PaddingBox { 0_css_px }); }
inline void RenderStyle::resetPageSize() { SET_NESTED(m_nonInheritedData, rareData, pageSize, Style::PageSize { CSS::Keyword::Auto { } }); }
inline void RenderStyle::setAccentColor(Style::AccentColor&& color) { SET(m_rareInheritedData, accentColor, WTFMove(color)); }
inline void RenderStyle::setAlignContent(const StyleContentAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignContent, data); }
inline void RenderStyle::setAlignItems(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignItems, data); }
inline void RenderStyle::setAlignItemsPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().alignItems.setPosition(position); }
inline void RenderStyle::setAlignSelf(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, alignSelf, data); }
inline void RenderStyle::setAlignSelfPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().alignSelf.setPosition(position); }
inline void RenderStyle::setAnchorNames(Style::AnchorNames&& names) { SET_NESTED(m_nonInheritedData, rareData, anchorNames, WTFMove(names)); }
inline void RenderStyle::setAnchorScope(const NameScope& scope) { SET_NESTED(m_nonInheritedData, rareData, anchorScope, scope); }
inline void RenderStyle::setAppearance(StyleAppearance appearance) { SET_NESTED_PAIR(m_nonInheritedData, miscData, appearance, static_cast<unsigned>(appearance), usedAppearance, static_cast<unsigned>(appearance)); }
inline void RenderStyle::setAppleColorFilter(Style::AppleColorFilter&& appleColorFilter) { SET_NESTED(m_rareInheritedData, appleColorFilter, appleColorFilter, WTFMove(appleColorFilter)); }
#if HAVE(CORE_MATERIAL)
inline void RenderStyle::setAppleVisualEffect(AppleVisualEffect effect) { SET_NESTED(m_nonInheritedData, rareData, appleVisualEffect, static_cast<unsigned>(effect)); }
inline void RenderStyle::setUsedAppleVisualEffectForSubtree(AppleVisualEffect effect) { SET(m_rareInheritedData, usedAppleVisualEffectForSubtree, static_cast<unsigned>(effect)); }
#endif
inline void RenderStyle::setAspectRatio(Style::AspectRatio&& ratio) { SET_NESTED(m_nonInheritedData, miscData, aspectRatio, WTFMove(ratio)); }
inline void RenderStyle::setBackfaceVisibility(BackfaceVisibility b) { SET_NESTED(m_nonInheritedData, rareData, backfaceVisibility, static_cast<unsigned>(b)); }
inline void RenderStyle::setBackgroundColor(Style::Color&& value) { SET_NESTED(m_nonInheritedData, backgroundData, color, WTFMove(value)); }
inline void RenderStyle::setBackgroundLayers(Style::BackgroundLayers&& layers) { SET_NESTED(m_nonInheritedData, backgroundData, background, WTFMove(layers)); }
inline void RenderStyle::setBlockEllipsis(Style::BlockEllipsis&& value) { SET(m_rareInheritedData, blockEllipsis, WTFMove(value)); }
inline void RenderStyle::setBlockStepAlign(BlockStepAlign value) { SET_NESTED(m_nonInheritedData, rareData, blockStepAlign, static_cast<unsigned>(value)); }
inline void RenderStyle::setBlockStepInsert(BlockStepInsert value) { SET_NESTED(m_nonInheritedData, rareData, blockStepInsert, static_cast<unsigned>(value)); }
inline void RenderStyle::setBlockStepRound(BlockStepRound value) { SET_NESTED(m_nonInheritedData, rareData, blockStepRound, static_cast<unsigned>(value)); }
inline void RenderStyle::setBlockStepSize(Style::BlockStepSize&& size) { SET_NESTED(m_nonInheritedData, rareData, blockStepSize, WTFMove(size)); }
inline void RenderStyle::setBorderBottomColor(Style::Color&& value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.bottom().m_color, WTFMove(value)); }
inline void RenderStyle::setBorderBottomLeftRadius(Style::BorderRadiusValue&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomLeft(), WTFMove(size)); }
inline void RenderStyle::setBorderBottomRightRadius(Style::BorderRadiusValue&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomRight(), WTFMove(size)); }
inline void RenderStyle::setBorderBottomStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.bottom().m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderBottomWidth(Style::LineWidth value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.bottom().m_width, value); }
inline void RenderStyle::setBorderImage(Style::BorderImage&& image) { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, WTFMove(image)); }
inline void RenderStyle::setBorderLeftColor(Style::Color&& value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.left().m_color, WTFMove(value)); }
inline void RenderStyle::setBorderLeftStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.left().m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderLeftWidth(Style::LineWidth value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.left().m_width, value); }
inline void RenderStyle::setBorderRightColor(Style::Color&& value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.right().m_color, WTFMove(value)); }
inline void RenderStyle::setBorderRightStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.right().m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderRightWidth(Style::LineWidth value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.right().m_width, value); }
inline void RenderStyle::setBorderTopColor(Style::Color&& value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.top().m_color, WTFMove(value)); }
inline void RenderStyle::setBorderTopLeftRadius(Style::BorderRadiusValue&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topLeft(), WTFMove(size)); }
inline void RenderStyle::setBorderTopRightRadius(Style::BorderRadiusValue&& size) { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topRight(), WTFMove(size)); }
inline void RenderStyle::setBorderTopStyle(BorderStyle value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.top().m_style, static_cast<unsigned>(value)); }
inline void RenderStyle::setBorderTopWidth(Style::LineWidth value) { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.top().m_width, value); }
inline void RenderStyle::setBorderHorizontalSpacing(Style::WebkitBorderSpacing borderSpacing) { SET(m_inheritedData, borderHorizontalSpacing, borderSpacing); }
inline void RenderStyle::setBorderVerticalSpacing(Style::WebkitBorderSpacing borderSpacing) { SET(m_inheritedData, borderVerticalSpacing, borderSpacing); }
inline void RenderStyle::setBottom(Style::InsetEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, inset.bottom(), WTFMove(edge)); }
inline void RenderStyle::setBoxAlign(BoxAlignment alignment) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, align, static_cast<unsigned>(alignment)); }
inline void RenderStyle::setBoxFlex(Style::WebkitBoxFlex flex) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, flex, flex); }
inline void RenderStyle::setBoxFlexGroup(Style::WebkitBoxFlexGroup group) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, flexGroup, group); }
inline void RenderStyle::setBoxLines(BoxLines lines) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, lines, static_cast<unsigned>(lines)); }
inline void RenderStyle::setBoxOrdinalGroup(Style::WebkitBoxOrdinalGroup group) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, ordinalGroup, group); }
inline void RenderStyle::setBoxOrient(BoxOrient orientation) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, orient, static_cast<unsigned>(orientation)); }
inline void RenderStyle::setBoxPack(BoxPack packing) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, deprecatedFlexibleBox, pack, static_cast<unsigned>(packing)); }
inline void RenderStyle::setBoxShadow(Style::BoxShadows&& boxShadow) { SET_NESTED(m_nonInheritedData, miscData, boxShadow, WTFMove(boxShadow)); }
inline void RenderStyle::setBoxReflect(Style::WebkitBoxReflect&& boxReflect) { SET_NESTED(m_nonInheritedData, rareData, boxReflect, WTFMove(boxReflect)); }
inline void RenderStyle::setBoxSizing(BoxSizing sizing) { SET_NESTED(m_nonInheritedData, boxData, m_boxSizing, static_cast<uint8_t>(sizing)); }
inline void RenderStyle::setBreakAfter(BreakBetween behavior) { SET_NESTED(m_nonInheritedData, rareData, breakAfter, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setBreakBefore(BreakBetween behavior) { SET_NESTED(m_nonInheritedData, rareData, breakBefore, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setBreakInside(BreakInside behavior) { SET_NESTED(m_nonInheritedData, rareData, breakInside, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setCapStyle(LineCap style) { SET(m_rareInheritedData, capStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setCaretColor(Style::Color&& color) { SET_PAIR(m_rareInheritedData, caretColor, WTFMove(color), hasAutoCaretColor, false); }
inline void RenderStyle::setClip(Style::Clip&& clip) { SET_NESTED(m_nonInheritedData, rareData, clip, WTFMove(clip)); }
inline void RenderStyle::setColumnAxis(ColumnAxis axis) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, axis, static_cast<unsigned>(axis)); }
inline void RenderStyle::setColumnCount(Style::ColumnCount count) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, count, count); }
inline void RenderStyle::setColumnFill(ColumnFill fill) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, fill, static_cast<unsigned>(fill)); }
inline void RenderStyle::setColumnGap(Style::GapGutter&& gap) { SET_NESTED(m_nonInheritedData, rareData, columnGap, WTFMove(gap)); }
inline void RenderStyle::setColumnProgression(ColumnProgression progression) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, progression, static_cast<unsigned>(progression)); }
inline void RenderStyle::setColumnRuleColor(Style::Color&& c) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule.m_color, c); }
inline void RenderStyle::setColumnRuleStyle(BorderStyle b) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule.m_style, static_cast<unsigned>(b)); }
inline void RenderStyle::setColumnRuleWidth(Style::LineWidth width) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, rule.m_width, width); }
inline void RenderStyle::setColumnSpan(ColumnSpan span) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, columnSpan, static_cast<unsigned>(span)); }
inline void RenderStyle::setColumnWidth(Style::ColumnWidth width) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, width, width); }
inline void RenderStyle::setContain(OptionSet<Containment> containment) { SET_NESTED(m_nonInheritedData, rareData, contain, containment); }
inline void RenderStyle::setContainIntrinsicHeight(Style::ContainIntrinsicSize&& height) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicHeight, WTFMove(height)); }
inline void RenderStyle::setContainIntrinsicWidth(Style::ContainIntrinsicSize&& width) { SET_NESTED(m_nonInheritedData, rareData, containIntrinsicWidth, WTFMove(width)); }
inline void RenderStyle::setContainerNames(Style::ContainerNames&& names) { SET_NESTED(m_nonInheritedData, rareData, containerNames, WTFMove(names)); }
inline void RenderStyle::setContainerType(ContainerType type) { SET_NESTED(m_nonInheritedData, rareData, containerType, static_cast<unsigned>(type)); }
inline void RenderStyle::setContent(Style::Content&& value) { SET_NESTED(m_nonInheritedData, miscData, content, WTFMove(value)); }
inline void RenderStyle::setContentVisibility(ContentVisibility value) { SET_NESTED(m_nonInheritedData, rareData, contentVisibility, static_cast<unsigned>(value)); }
inline void RenderStyle::setUsedAppearance(StyleAppearance a) { SET_NESTED(m_nonInheritedData, miscData, usedAppearance, static_cast<unsigned>(a)); }
inline void RenderStyle::setEffectiveInert(bool effectiveInert) { SET(m_rareInheritedData, effectiveInert, effectiveInert); }
inline void RenderStyle::setIsEffectivelyTransparent(bool effectivelyTransparent) { SET(m_rareInheritedData, effectivelyTransparent, effectivelyTransparent); }
inline void RenderStyle::setUsedTouchActions(OptionSet<TouchAction> touchActions) { SET(m_rareInheritedData, usedTouchActions, touchActions); }
inline void RenderStyle::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes) { SET(m_rareInheritedData, eventListenerRegionTypes, eventListenerTypes); }
inline void RenderStyle::setFieldSizing(FieldSizing value) { SET_NESTED(m_nonInheritedData, rareData, fieldSizing, static_cast<unsigned>(value)); }
inline void RenderStyle::setFilter(Style::Filter&& filter) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, filter, filter, WTFMove(filter)); }
inline void RenderStyle::setFlexBasis(Style::FlexBasis&& basis) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexBasis, WTFMove(basis)); }
inline void RenderStyle::setFlexDirection(FlexDirection direction) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexDirection, static_cast<unsigned>(direction)); }
inline void RenderStyle::setFlexGrow(Style::FlexGrow grow) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexGrow, grow); }
inline void RenderStyle::setFlexShrink(Style::FlexShrink shrink) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexShrink, shrink); }
inline void RenderStyle::setFlexWrap(FlexWrap wrap) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, flexibleBox, flexWrap, static_cast<unsigned>(wrap)); }
inline void RenderStyle::setGridAutoColumns(Style::GridTrackSizes&& trackSizeList) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, m_gridAutoColumns, WTFMove(trackSizeList)); }
inline void RenderStyle::setGridAutoFlow(GridAutoFlow flow) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, m_gridAutoFlow, flow); }
inline void RenderStyle::setGridAutoRows(Style::GridTrackSizes&& trackSizeList) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, m_gridAutoRows, WTFMove(trackSizeList)); }
inline void RenderStyle::setGridItemColumnEnd(Style::GridPosition&& columnEndPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridColumnEnd, WTFMove(columnEndPosition)); }
inline void RenderStyle::setGridItemColumnStart(Style::GridPosition&& columnStartPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridColumnStart, WTFMove(columnStartPosition)); }
inline void RenderStyle::setGridItemRowEnd(Style::GridPosition&& rowEndPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridRowEnd, WTFMove(rowEndPosition)); }
inline void RenderStyle::setGridItemRowStart(Style::GridPosition&& rowStartPosition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, gridItem, gridRowStart, WTFMove(rowStartPosition)); }
inline void RenderStyle::setGridTemplateAreas(Style::GridTemplateAreas&& areas) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, grid, m_gridTemplateAreas, WTFMove(areas)); }
inline void RenderStyle::setHangingPunctuation(OptionSet<HangingPunctuation> punctuation) { SET(m_rareInheritedData, hangingPunctuation, punctuation.toRaw()); }
inline void RenderStyle::setHasAttrContent() { SET_NESTED(m_nonInheritedData, miscData, hasAttrContent, true); }
inline void RenderStyle::setHasAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasAutoCaretColor, true, caretColor, Style::Color::currentColor()); }
inline void RenderStyle::setHasDisplayAffectedByAnimations() { SET_NESTED(m_nonInheritedData, miscData, hasDisplayAffectedByAnimations, true); }
inline void RenderStyle::setHasExplicitlySetBorderBottomLeftRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderBottomLeftRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderBottomRightRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderBottomRightRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderTopLeftRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderTopLeftRadius, value); }
inline void RenderStyle::setHasExplicitlySetBorderTopRightRadius(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetBorderTopRightRadius, value); }
inline void RenderStyle::setHasExplicitlySetPaddingBottom(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetPaddingBottom, value); }
inline void RenderStyle::setHasExplicitlySetPaddingLeft(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetPaddingLeft, value); }
inline void RenderStyle::setHasExplicitlySetPaddingRight(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetPaddingRight, value); }
inline void RenderStyle::setHasExplicitlySetPaddingTop(bool value) { SET_NESTED(m_nonInheritedData, surroundData, hasExplicitlySetPaddingTop, value); }
inline void RenderStyle::setHasExplicitlySetStrokeColor(bool value) { SET(m_rareInheritedData, hasSetStrokeColor, static_cast<unsigned>(value)); }
inline void RenderStyle::setHasExplicitlySetStrokeWidth(bool value) { SET(m_rareInheritedData, hasSetStrokeWidth, static_cast<unsigned>(value)); }
inline void RenderStyle::setHasPseudoStyles(PseudoIdSet set) { m_nonInheritedFlags.setHasPseudoStyles(set); }
inline void RenderStyle::setHasVisitedLinkAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, true, visitedLinkCaretColor, Style::Color::currentColor()); }
inline void RenderStyle::setHeight(Style::PreferredSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_height, WTFMove(length)); }
inline void RenderStyle::setHyphenateLimitAfter(Style::HyphenateLimitEdge limit) { SET(m_rareInheritedData, hyphenateLimitAfter, limit); }
inline void RenderStyle::setHyphenateLimitBefore(Style::HyphenateLimitEdge limit) { SET(m_rareInheritedData, hyphenateLimitBefore, limit); }
inline void RenderStyle::setHyphenateLimitLines(Style::HyphenateLimitLines limit) { SET(m_rareInheritedData, hyphenateLimitLines, limit); }
inline void RenderStyle::setHyphenateCharacter(Style::HyphenateCharacter&& hyphenateCharacter) { SET(m_rareInheritedData, hyphenateCharacter, WTFMove(hyphenateCharacter)); }
inline void RenderStyle::setHyphens(Hyphens hyphens) { SET(m_rareInheritedData, hyphens, static_cast<unsigned>(hyphens)); }
inline void RenderStyle::setImageOrientation(ImageOrientation value) { SET(m_rareInheritedData, imageOrientation, static_cast<unsigned>(value)); }
inline void RenderStyle::setImageRendering(ImageRendering value) { SET(m_rareInheritedData, imageRendering, static_cast<unsigned>(value)); }
inline void RenderStyle::setInsetBox(Style::InsetBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, inset, WTFMove(box)); }
inline void RenderStyle::setInitialLetter(Style::WebkitInitialLetter&& initialLetter) { SET_NESTED(m_nonInheritedData, rareData, initialLetter, WTFMove(initialLetter)); }
inline void RenderStyle::setInputSecurity(InputSecurity security) { SET_NESTED(m_nonInheritedData, rareData, inputSecurity, static_cast<unsigned>(security)); }
inline void RenderStyle::setJoinStyle(LineJoin style) { SET(m_rareInheritedData, joinStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setJustifyContent(const StyleContentAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifyContent, data); }
inline void RenderStyle::setJustifyContentPosition(ContentPosition position) { m_nonInheritedData.access().miscData.access().justifyContent.setPosition(position); }
inline void RenderStyle::setJustifyItems(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifyItems, data); }
inline void RenderStyle::setJustifySelf(const StyleSelfAlignmentData& data) { SET_NESTED(m_nonInheritedData, miscData, justifySelf, data); }
inline void RenderStyle::setJustifySelfPosition(ItemPosition position) { m_nonInheritedData.access().miscData.access().justifySelf.setPosition(position); }
inline void RenderStyle::setLeft(Style::InsetEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, inset.left(), WTFMove(edge)); }
inline void RenderStyle::setLetterSpacing(Style::LetterSpacing&& letterSpacing) { SET_NESTED(m_inheritedData, fontData, letterSpacing, WTFMove(letterSpacing)); }
inline void RenderStyle::setLineAlign(LineAlign alignment) { SET(m_rareInheritedData, lineAlign, static_cast<unsigned>(alignment)); }
inline void RenderStyle::setLineBoxContain(OptionSet<Style::LineBoxContain> c) { SET(m_rareInheritedData, lineBoxContain, c.toRaw()); }
inline void RenderStyle::setLineBreak(LineBreak rule) { SET(m_rareInheritedData, lineBreak, static_cast<unsigned>(rule)); }
inline void RenderStyle::setLineClamp(Style::WebkitLineClamp&& value) { SET_NESTED(m_nonInheritedData, rareData, lineClamp, WTFMove(value)); }
inline void RenderStyle::setLineGrid(Style::WebkitLineGrid&& lineGrid) { SET(m_rareInheritedData, lineGrid, WTFMove(lineGrid)); }
inline void RenderStyle::setLineSnap(LineSnap snap) { SET(m_rareInheritedData, lineSnap, static_cast<unsigned>(snap)); }
inline void RenderStyle::setListStyleImage(Style::ImageOrNone&& value) { SET(m_rareInheritedData, listStyleImage, WTFMove(value)); }
inline void RenderStyle::setListStyleType(Style::ListStyleType&& value) { SET(m_rareInheritedData, listStyleType, WTFMove(value)); }
inline void RenderStyle::setMarginBottom(Style::MarginEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, margin.bottom(), WTFMove(edge)); }
inline void RenderStyle::setMarginBox(Style::MarginBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, margin, WTFMove(box)); }
inline void RenderStyle::setMarginLeft(Style::MarginEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, margin.left(), WTFMove(edge)); }
inline void RenderStyle::setMarginRight(Style::MarginEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, margin.right(), WTFMove(edge)); }
inline void RenderStyle::setMarginTop(Style::MarginEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, margin.top(), WTFMove(edge)); }
inline void RenderStyle::setMarginTrim(OptionSet<MarginTrimType> value) { SET_NESTED(m_nonInheritedData, rareData, marginTrim, value); }
inline void RenderStyle::setMarqueeBehavior(MarqueeBehavior b) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, behavior, static_cast<unsigned>(b)); }
inline void RenderStyle::setMarqueeDirection(MarqueeDirection d) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, direction, static_cast<unsigned>(d)); }
inline void RenderStyle::setMarqueeIncrement(Style::WebkitMarqueeIncrement&& increment) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, increment, WTFMove(increment)); }
inline void RenderStyle::setMarqueeRepetition(Style::WebkitMarqueeRepetition repetition) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, repetition, repetition); }
inline void RenderStyle::setMarqueeSpeed(Style::WebkitMarqueeSpeed speed) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, marquee, speed, speed); }
inline void RenderStyle::setMaskBorder(Style::MaskBorder&& image) { SET_NESTED(m_nonInheritedData, rareData, maskBorder, WTFMove(image)); }
inline void RenderStyle::setMaskLayers(Style::MaskLayers&& layers) { SET_NESTED(m_nonInheritedData, miscData, mask, WTFMove(layers)); }
inline void RenderStyle::setMathShift(const MathShift& shift) { SET(m_rareInheritedData, mathShift, static_cast<unsigned>(shift)); }
inline void RenderStyle::setMathStyle(const MathStyle& style) { SET(m_rareInheritedData, mathStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setMaxHeight(Style::MaximumSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_maxHeight, WTFMove(length)); }
inline void RenderStyle::setMaxLines(Style::MaximumLines value) { SET_NESTED(m_nonInheritedData, rareData, maxLines, value); }
inline void RenderStyle::setMaxWidth(Style::MaximumSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_maxWidth, WTFMove(length)); }
inline void RenderStyle::setMinHeight(Style::MinimumSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_minHeight, WTFMove(length)); }
inline void RenderStyle::setMinWidth(Style::MinimumSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_minWidth, WTFMove(length)); }
inline void RenderStyle::setNBSPMode(NBSPMode mode) { SET(m_rareInheritedData, nbspMode, static_cast<unsigned>(mode)); }
inline void RenderStyle::setObjectFit(ObjectFit fit) { SET_NESTED(m_nonInheritedData, miscData, objectFit, static_cast<unsigned>(fit)); }
inline void RenderStyle::setObjectPosition(Style::ObjectPosition&& position) { SET_NESTED(m_nonInheritedData, miscData, objectPosition, WTFMove(position)); }
inline void RenderStyle::setOffsetAnchor(Style::OffsetAnchor&& anchor) { SET_NESTED(m_nonInheritedData, rareData, offsetAnchor, WTFMove(anchor)); }
inline void RenderStyle::setOffsetDistance(Style::OffsetDistance&& distance) { SET_NESTED(m_nonInheritedData, rareData, offsetDistance, WTFMove(distance)); }
inline void RenderStyle::setOffsetPath(Style::OffsetPath&& path) { SET_NESTED(m_nonInheritedData, rareData, offsetPath, WTFMove(path)); }
inline void RenderStyle::setOffsetPosition(Style::OffsetPosition&& position) { SET_NESTED(m_nonInheritedData, rareData, offsetPosition, WTFMove(position)); }
inline void RenderStyle::setOffsetRotate(Style::OffsetRotate&& rotate) { SET_NESTED(m_nonInheritedData, rareData, offsetRotate, WTFMove(rotate)); }
inline void RenderStyle::setOpacity(Style::Opacity opacity) { SET_NESTED(m_nonInheritedData, miscData, opacity, opacity); }
inline void RenderStyle::setOrder(Style::Order order) { SET_NESTED(m_nonInheritedData, miscData, order, order); }
inline void RenderStyle::setOrphans(Style::Orphans orphans) { SET(m_rareInheritedData, orphans, orphans); }
inline void RenderStyle::setOutlineColor(Style::Color&& color) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_color, WTFMove(color)); }
inline void RenderStyle::setOutlineOffset(Style::Length<> offset) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_offset, offset); }
inline void RenderStyle::setOutlineStyle(OutlineStyle style) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_style, static_cast<unsigned>(style)); }
inline void RenderStyle::setOutlineWidth(Style::LineWidth width) { SET_NESTED(m_nonInheritedData, backgroundData, outline.m_width, width); }
inline void RenderStyle::setOverflowAnchor(OverflowAnchor a) { SET_NESTED(m_nonInheritedData, rareData, overflowAnchor, static_cast<unsigned>(a)); }
inline void RenderStyle::setOverflowContinue(OverflowContinue value) { SET_NESTED(m_nonInheritedData, rareData, overflowContinue, value); }
inline void RenderStyle::setOverflowWrap(OverflowWrap rule) { SET(m_rareInheritedData, overflowWrap, static_cast<unsigned>(rule)); }
inline void RenderStyle::setOverscrollBehaviorX(OverscrollBehavior value) { SET_NESTED(m_nonInheritedData, rareData, overscrollBehaviorX, static_cast<unsigned>(value)); }
inline void RenderStyle::setOverscrollBehaviorY(OverscrollBehavior value) { SET_NESTED(m_nonInheritedData, rareData, overscrollBehaviorY, static_cast<unsigned>(value)); }
inline void RenderStyle::setPaddingBottom(Style::PaddingEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, padding.bottom(), WTFMove(edge)); }
inline void RenderStyle::setPaddingBox(Style::PaddingBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, padding, WTFMove(box)); }
inline void RenderStyle::setPaddingLeft(Style::PaddingEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, padding.left(), WTFMove(edge)); }
inline void RenderStyle::setPaddingRight(Style::PaddingEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, padding.right(), WTFMove(edge)); }
inline void RenderStyle::setPaddingTop(Style::PaddingEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, padding.top(), WTFMove(edge)); }
inline void RenderStyle::setPageSize(Style::PageSize&& pageSize) { SET_NESTED(m_nonInheritedData, rareData, pageSize, WTFMove(pageSize)); }
inline void RenderStyle::setPaintOrder(PaintOrder order) { SET(m_rareInheritedData, paintOrder, static_cast<unsigned>(order)); }
inline void RenderStyle::setPerspective(Style::Perspective&& perspective) { SET_NESTED(m_nonInheritedData, rareData, perspective, WTFMove(perspective)); }
inline void RenderStyle::setPerspectiveOrigin(Style::PerspectiveOrigin&& origin) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin, WTFMove(origin)); }
inline void RenderStyle::setPerspectiveOriginX(Style::PerspectiveOriginX&& originX) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin.x, WTFMove(originX)); }
inline void RenderStyle::setPerspectiveOriginY(Style::PerspectiveOriginY&& originY) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin.y, WTFMove(originY)); }
inline void RenderStyle::setPositionAnchor(const std::optional<Style::ScopedName>& anchor) { SET_NESTED(m_nonInheritedData, rareData, positionAnchor, anchor); }
inline void RenderStyle::setPositionArea(std::optional<PositionArea> value) { SET_NESTED(m_nonInheritedData, rareData, positionArea, value); }
inline void RenderStyle::setPositionTryOrder(Style::PositionTryOrder order) { SET_NESTED(m_nonInheritedData, rareData, positionTryOrder, static_cast<unsigned>(order)); }
inline void RenderStyle::setPositionVisibility(OptionSet<PositionVisibility> value) { SET_NESTED(m_nonInheritedData, rareData, positionVisibility, value.toRaw()); }
inline void RenderStyle::setResize(Resize r) { SET_NESTED(m_nonInheritedData, miscData, resize, static_cast<unsigned>(r)); }
inline void RenderStyle::setRight(Style::InsetEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, inset.right(), WTFMove(edge)); }
inline void RenderStyle::setRotate(Style::Rotate&& rotate) { SET_NESTED(m_nonInheritedData, rareData, rotate, WTFMove(rotate)); }
inline void RenderStyle::setRowGap(Style::GapGutter&& gap) { SET_NESTED(m_nonInheritedData, rareData, rowGap, WTFMove(gap)); }
inline void RenderStyle::setRubyPosition(RubyPosition position) { SET(m_rareInheritedData, rubyPosition, static_cast<unsigned>(position)); }
inline void RenderStyle::setRubyAlign(RubyAlign alignment) { SET(m_rareInheritedData, rubyAlign, static_cast<unsigned>(alignment)); }
inline void RenderStyle::setRubyOverhang(RubyOverhang overhang) { SET(m_rareInheritedData, rubyOverhang, static_cast<unsigned>(overhang)); }
inline void RenderStyle::setScale(Style::Scale&& scale) { SET_NESTED(m_nonInheritedData, rareData, scale, WTFMove(scale)); }
inline void RenderStyle::setScrollBehavior(Style::ScrollBehavior behavior) { SET_NESTED(m_nonInheritedData, rareData, scrollBehavior, static_cast<unsigned>(behavior)); }
inline void RenderStyle::setScrollSnapAlign(Style::ScrollSnapAlign&& align) { SET_NESTED(m_nonInheritedData, rareData, scrollSnapAlign, WTFMove(align)); }
inline void RenderStyle::setScrollSnapStop(ScrollSnapStop stop) { SET_NESTED(m_nonInheritedData, rareData, scrollSnapStop, stop); }
inline void RenderStyle::setScrollSnapType(Style::ScrollSnapType&& type) { SET_NESTED(m_nonInheritedData, rareData, scrollSnapType, WTFMove(type)); }
inline void RenderStyle::setScrollTimelineAxes(Style::ProgressTimelineAxes&& axes) { SET_NESTED(m_nonInheritedData, rareData, scrollTimelineAxes, WTFMove(axes)); }
inline void RenderStyle::setScrollTimelineNames(Style::ProgressTimelineNames&& names) { SET_NESTED(m_nonInheritedData, rareData, scrollTimelineNames, WTFMove(names)); }
inline void RenderStyle::setVerticalAlign(Style::VerticalAlign&& align) { SET_NESTED(m_nonInheritedData, boxData, m_verticalAlign, WTFMove(align)); }
inline void RenderStyle::setViewTimelineAxes(Style::ProgressTimelineAxes&& axes) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineAxes, WTFMove(axes)); }
inline void RenderStyle::setViewTimelineInsets(Style::ViewTimelineInsets&& insets) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineInsets, WTFMove(insets)); }
inline void RenderStyle::setViewTimelineNames(Style::ProgressTimelineNames&& names) { SET_NESTED(m_nonInheritedData, rareData, viewTimelineNames, WTFMove(names)); }
inline void RenderStyle::setTimelineScope(const NameScope& scope) { SET_NESTED(m_nonInheritedData, rareData, timelineScope, scope); }
inline void RenderStyle::setScrollbarColor(Style::ScrollbarColor&& color) { SET(m_rareInheritedData, scrollbarColor, WTFMove(color)); }
inline void RenderStyle::setScrollbarGutter(Style::ScrollbarGutter&& gutter) { SET_NESTED(m_nonInheritedData, rareData, scrollbarGutter, WTFMove(gutter)); }
inline void RenderStyle::setScrollbarWidth(ScrollbarWidth width) { SET_NESTED(m_nonInheritedData, rareData, scrollbarWidth, static_cast<unsigned>(width)); }
inline void RenderStyle::setShapeImageThreshold(Style::ShapeImageThreshold shapeImageThreshold) { SET_NESTED(m_nonInheritedData, rareData, shapeImageThreshold, shapeImageThreshold); }
inline void RenderStyle::setShapeMargin(Style::ShapeMargin&& shapeMargin) { SET_NESTED(m_nonInheritedData, rareData, shapeMargin, WTFMove(shapeMargin)); }
inline void RenderStyle::setShapeOutside(Style::ShapeOutside&& shapeOutside) { SET_NESTED(m_nonInheritedData, rareData, shapeOutside, WTFMove(shapeOutside)); }
inline void RenderStyle::setUsedContentVisibility(ContentVisibility usedContentVisibility) { SET(m_rareInheritedData, usedContentVisibility, static_cast<unsigned>(usedContentVisibility)); }
inline void RenderStyle::setSpeakAs(OptionSet<SpeakAs> style) { SET(m_rareInheritedData, speakAs, style.toRaw()); }
inline void RenderStyle::setSpecifiedZIndex(Style::ZIndex index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_hasAutoSpecifiedZIndex, static_cast<uint8_t>(index.m_isAuto), m_specifiedZIndexValue, index.m_value); }
inline void RenderStyle::setStrokeColor(Style::Color&& color) { SET(m_rareInheritedData, strokeColor, WTFMove(color)); }
inline void RenderStyle::setStrokeMiterLimit(Style::StrokeMiterlimit value) { SET(m_rareInheritedData, miterLimit, value); }
inline void RenderStyle::setStrokeWidth(Style::StrokeWidth&& width) { SET(m_rareInheritedData, strokeWidth, WTFMove(width)); }
inline void RenderStyle::setTabSize(Style::TabSize&& size) { SET(m_rareInheritedData, tabSize, WTFMove(size)); }
inline void RenderStyle::setTextAlignLast(TextAlignLast value) { SET(m_rareInheritedData, textAlignLast, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextBoxTrim(TextBoxTrim value) { SET_NESTED(m_nonInheritedData, rareData, textBoxTrim, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextBoxEdge(Style::TextBoxEdge value) { SET(m_rareInheritedData, textBoxEdge, value); }
inline void RenderStyle::setLineFitEdge(Style::LineFitEdge value) { SET(m_rareInheritedData, lineFitEdge, value); }
inline void RenderStyle::setTextCombine(TextCombine value) { SET(m_rareInheritedData, textCombine, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextDecorationColor(Style::Color&& color) { SET_NESTED(m_nonInheritedData, rareData, textDecorationColor, WTFMove(color)); }
inline void RenderStyle::setTextDecorationLine(Style::TextDecorationLine&& value) { m_nonInheritedFlags.textDecorationLine = value.toRaw(); }
inline void RenderStyle::setTextDecorationSkipInk(TextDecorationSkipInk skipInk) { SET(m_rareInheritedData, textDecorationSkipInk, static_cast<unsigned>(skipInk)); }
inline void RenderStyle::setTextDecorationStyle(TextDecorationStyle value) { SET_NESTED(m_nonInheritedData, rareData, textDecorationStyle, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextDecorationThickness(Style::TextDecorationThickness&& textDecorationThickness) { SET_NESTED(m_nonInheritedData, rareData, textDecorationThickness, WTFMove(textDecorationThickness)); }
inline void RenderStyle::setTextDecorationLineInEffect(Style::TextDecorationLine&& value) { m_inheritedFlags.textDecorationLineInEffect = value.toRaw(); }
inline void RenderStyle::setTextEmphasisColor(Style::Color&& c) { SET(m_rareInheritedData, textEmphasisColor, WTFMove(c)); }
inline void RenderStyle::setTextEmphasisStyle(Style::TextEmphasisStyle&& style) { SET(m_rareInheritedData, textEmphasisStyle, style); }
inline void RenderStyle::setTextEmphasisPosition(OptionSet<TextEmphasisPosition> position) { SET(m_rareInheritedData, textEmphasisPosition, static_cast<unsigned>(position.toRaw())); }
inline void RenderStyle::setTextFillColor(Style::Color&& color) { SET(m_rareInheritedData, textFillColor, WTFMove(color)); }
inline void RenderStyle::setHasExplicitlySetColor(bool value) { m_inheritedFlags.hasExplicitlySetColor = value; }
inline void RenderStyle::setTableLayout(TableLayoutType value) { SET_NESTED(m_nonInheritedData, miscData, tableLayout, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextGroupAlign(TextGroupAlign value) { SET_NESTED(m_nonInheritedData, rareData, textGroupAlign, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextIndent(Style::TextIndent&& textIndent) { SET(m_rareInheritedData, textIndent, WTFMove(textIndent)); }
inline void RenderStyle::setTextJustify(TextJustify value) { SET(m_rareInheritedData, textJustify, static_cast<unsigned>(value)); }
inline void RenderStyle::setTextOverflow(TextOverflow overflow) { SET_NESTED(m_nonInheritedData, miscData, textOverflow, static_cast<unsigned>(overflow)); }
inline void RenderStyle::setTextSecurity(TextSecurity security) { SET(m_rareInheritedData, textSecurity, static_cast<unsigned>(security)); }
inline void RenderStyle::setTextShadow(Style::TextShadows&& textShadow) { SET(m_rareInheritedData, textShadow, WTFMove(textShadow)); }
inline void RenderStyle::setTextStrokeColor(Style::Color&& color) { SET(m_rareInheritedData, textStrokeColor, WTFMove(color)); }
inline void RenderStyle::setTextStrokeWidth(Style::WebkitTextStrokeWidth width) { SET(m_rareInheritedData, textStrokeWidth, width); }
inline void RenderStyle::setTextTransform(OptionSet<TextTransform> value) { m_inheritedFlags.textTransform = value.toRaw(); }
inline void RenderStyle::setTextUnderlineOffset(Style::TextUnderlineOffset&& textUnderlineOffset) { SET(m_rareInheritedData, textUnderlineOffset, WTFMove(textUnderlineOffset)); }
inline void RenderStyle::setTextUnderlinePosition(OptionSet<TextUnderlinePosition> position) { SET(m_rareInheritedData, textUnderlinePosition, static_cast<unsigned>(position.toRaw())); }
inline void RenderStyle::setTextZoom(TextZoom zoom) { SET(m_rareInheritedData, textZoom, static_cast<unsigned>(zoom)); }
inline void RenderStyle::setTop(Style::InsetEdge&& edge) { SET_NESTED(m_nonInheritedData, surroundData, inset.top(), WTFMove(edge)); }
inline void RenderStyle::setTouchActions(OptionSet<TouchAction> actions) { SET_NESTED(m_nonInheritedData, rareData, touchActions, actions); }
inline void RenderStyle::setTransform(Style::Transform&& transform) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, transform, WTFMove(transform)); }
inline void RenderStyle::setTransformBox(TransformBox box) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, transformBox, box); }
inline void RenderStyle::setTransformOrigin(Style::TransformOrigin&& origin) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin, WTFMove(origin)); }
inline void RenderStyle::setTransformOriginX(Style::TransformOriginX&& originX) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin.x, WTFMove(originX)); }
inline void RenderStyle::setTransformOriginY(Style::TransformOriginY&& originY) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin.y, WTFMove(originY)); }
inline void RenderStyle::setTransformOriginZ(Style::TransformOriginZ&& originZ) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin.z, WTFMove(originZ)); }
inline void RenderStyle::setTransformStyle3D(TransformStyle3D b) { SET_NESTED(m_nonInheritedData, rareData, transformStyle3D, static_cast<unsigned>(b)); }
inline void RenderStyle::setTransformStyleForcedToFlat(bool b) { SET_NESTED(m_nonInheritedData, rareData, transformStyleForcedToFlat, static_cast<unsigned>(b)); }
inline void RenderStyle::setTranslate(Style::Translate&& translate) { SET_NESTED(m_nonInheritedData, rareData, translate, WTFMove(translate)); }
inline void RenderStyle::setUsesAnchorFunctions() { SET_NESTED(m_nonInheritedData, rareData, usesAnchorFunctions, true); }
inline void RenderStyle::setAnchorFunctionScrollCompensatedAxes(OptionSet<BoxAxisFlag> axes) { SET_NESTED(m_nonInheritedData, rareData, anchorFunctionScrollCompensatedAxes, axes.toRaw()); }
inline void RenderStyle::setIsPopoverInvoker() { SET_NESTED(m_nonInheritedData, rareData, isPopoverInvoker, true); }
inline void RenderStyle::setUsedZIndex(Style::ZIndex index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, m_hasAutoUsedZIndex, static_cast<uint8_t>(index.m_isAuto), m_usedZIndexValue, index.m_value); }
inline void RenderStyle::setUserDrag(UserDrag value) { SET_NESTED(m_nonInheritedData, miscData, userDrag, static_cast<unsigned>(value)); }
inline void RenderStyle::setUserModify(UserModify value) { SET(m_rareInheritedData, userModify, static_cast<unsigned>(value)); }
inline void RenderStyle::setUserSelect(UserSelect value) { SET(m_rareInheritedData, userSelect, static_cast<unsigned>(value)); }
inline void RenderStyle::setViewTransitionClasses(Style::ViewTransitionClasses&& value) { SET_NESTED(m_nonInheritedData, rareData, viewTransitionClasses, WTFMove(value)); }
inline void RenderStyle::setViewTransitionName(Style::ViewTransitionName&& value) { SET_NESTED(m_nonInheritedData, rareData, viewTransitionName, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkBackgroundColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, background, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkBorderBottomColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderBottom, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkBorderLeftColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderLeft, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkBorderRightColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderRight, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkBorderTopColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, borderTop, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkCaretColor(Style::Color&& value) { SET_PAIR(m_rareInheritedData, visitedLinkCaretColor, WTFMove(value), hasVisitedLinkAutoCaretColor, false); }
inline void RenderStyle::setVisitedLinkColumnRuleColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, visitedLinkColumnRuleColor, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkOutlineColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, outline, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkStrokeColor(Style::Color&& value) { SET(m_rareInheritedData, visitedLinkStrokeColor, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkTextDecorationColor(Style::Color&& value) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, visitedLinkColor, textDecoration, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkTextEmphasisColor(Style::Color&& value) { SET(m_rareInheritedData, visitedLinkTextEmphasisColor, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkTextFillColor(Style::Color&& value) { SET(m_rareInheritedData, visitedLinkTextFillColor, WTFMove(value)); }
inline void RenderStyle::setVisitedLinkTextStrokeColor(Style::Color&& value) { SET(m_rareInheritedData, visitedLinkTextStrokeColor, WTFMove(value)); }
inline void RenderStyle::setWidows(Style::Widows widows) { SET(m_rareInheritedData, widows, widows); }
inline void RenderStyle::setWidth(Style::PreferredSize&& length) { SET_NESTED(m_nonInheritedData, boxData, m_width, WTFMove(length)); }
inline void RenderStyle::setWordBreak(WordBreak rule) { SET(m_rareInheritedData, wordBreak, static_cast<unsigned>(rule)); }
inline void RenderStyle::setWordSpacing(Style::WordSpacing&& wordSpacing) { SET_NESTED(m_inheritedData, fontData, wordSpacing, WTFMove(wordSpacing)); }
inline void RenderStyle::setCornerBottomLeftShape(Style::CornerShapeValue&& shape) { SET_NESTED(m_nonInheritedData, surroundData, border.m_cornerShapes.bottomLeft(), WTFMove(shape)); }
inline void RenderStyle::setCornerBottomRightShape(Style::CornerShapeValue&& shape) { SET_NESTED(m_nonInheritedData, surroundData, border.m_cornerShapes.bottomRight(), WTFMove(shape)); }
inline void RenderStyle::setCornerTopLeftShape(Style::CornerShapeValue&& shape) { SET_NESTED(m_nonInheritedData, surroundData, border.m_cornerShapes.topLeft(), WTFMove(shape)); }
inline void RenderStyle::setCornerTopRightShape(Style::CornerShapeValue&& shape) { SET_NESTED(m_nonInheritedData, surroundData, border.m_cornerShapes.topRight(), WTFMove(shape)); }

inline void RenderStyle::setNativeAppearanceDisabled(bool value) { SET_NESTED(m_nonInheritedData, rareData, nativeAppearanceDisabled, value); }

#if ENABLE(APPLE_PAY)
inline void RenderStyle::setApplePayButtonStyle(ApplePayButtonStyle style) { SET_NESTED(m_nonInheritedData, rareData, applePayButtonStyle, static_cast<unsigned>(style)); }
inline void RenderStyle::setApplePayButtonType(ApplePayButtonType type) { SET_NESTED(m_nonInheritedData, rareData, applePayButtonType, static_cast<unsigned>(type)); }
#endif

inline void RenderStyle::setBoxDecorationBreak(BoxDecorationBreak value) { SET_NESTED(m_nonInheritedData, boxData, m_boxDecorationBreak, static_cast<uint8_t>(value)); }

#if ENABLE(DARK_MODE_CSS)
inline void RenderStyle::setColorScheme(Style::ColorScheme scheme) { SET(m_rareInheritedData, colorScheme, scheme); }
inline void RenderStyle::setHasExplicitlySetColorScheme() { SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetColorScheme, true); }
#endif

inline void RenderStyle::setDynamicRangeLimit(Style::DynamicRangeLimit&& limit) { SET(m_rareInheritedData, dynamicRangeLimit, WTFMove(limit)); }

inline void RenderStyle::setBackdropFilter(Style::Filter&& filter) { SET_DOUBLY_NESTED(m_nonInheritedData, rareData, backdropFilter, filter, WTFMove(filter)); }

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
inline void RenderStyle::setTouchCallout(Style::WebkitTouchCallout value) { SET(m_rareInheritedData, webkitTouchCallout, static_cast<unsigned>(value)); }
#endif

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
inline void RenderStyle::setOverflowScrolling(Style::WebkitOverflowScrolling value) { SET(m_rareInheritedData, webkitOverflowScrolling, static_cast<unsigned>(value)); }
#endif

#if ENABLE(TEXT_AUTOSIZING)
inline void RenderStyle::setTextSizeAdjust(Style::TextSizeAdjust adjustment) { SET(m_rareInheritedData, textSizeAdjust, adjustment); }
#endif

#if ENABLE(TOUCH_EVENTS)
inline void RenderStyle::setTapHighlightColor(Style::Color&& color) { SET(m_rareInheritedData, tapHighlightColor, WTFMove(color)); }
#endif

inline void RenderStyle::setInsideDefaultButton(bool value) { SET(m_rareInheritedData, insideDefaultButton, value); }
inline void RenderStyle::setInsideSubmitButton(bool value) { SET(m_rareInheritedData, insideSubmitButton, value); }

inline void RenderStyle::setAlignmentBaseline(AlignmentBaseline val) { SET_NESTED_STRUCT(m_svgStyle, nonInheritedFlags, alignmentBaseline, static_cast<unsigned>(val)); }
inline void RenderStyle::setDominantBaseline(DominantBaseline val) { SET_NESTED_STRUCT(m_svgStyle, nonInheritedFlags, dominantBaseline, static_cast<unsigned>(val)); }
inline void RenderStyle::setVectorEffect(VectorEffect val) { SET_NESTED_STRUCT(m_svgStyle, nonInheritedFlags, vectorEffect, static_cast<unsigned>(val)); }
inline void RenderStyle::setBufferedRendering(BufferedRendering val) { SET_NESTED_STRUCT(m_svgStyle, nonInheritedFlags, bufferedRendering, static_cast<unsigned>(val)); }
inline void RenderStyle::setMaskType(MaskType val) { SET_NESTED_STRUCT(m_svgStyle, nonInheritedFlags, maskType, static_cast<unsigned>(val)); }

inline void RenderStyle::setClipRule(WindRule val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, clipRule, static_cast<unsigned>(val)); }
inline void RenderStyle::setColorInterpolation(ColorInterpolation val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, colorInterpolation, static_cast<unsigned>(val)); }
inline void RenderStyle::setColorInterpolationFilters(ColorInterpolation val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, colorInterpolationFilters, static_cast<unsigned>(val)); }
inline void RenderStyle::setFillRule(WindRule val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, fillRule, static_cast<unsigned>(val)); }
inline void RenderStyle::setShapeRendering(ShapeRendering val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, shapeRendering, static_cast<unsigned>(val)); }
inline void RenderStyle::setTextAnchor(TextAnchor val) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, textAnchor, static_cast<unsigned>(val)); }
inline void RenderStyle::setGlyphOrientationHorizontal(Style::SVGGlyphOrientationHorizontal value) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, glyphOrientationHorizontal, static_cast<unsigned>(value)); }
inline void RenderStyle::setGlyphOrientationVertical(Style::SVGGlyphOrientationVertical value) { SET_NESTED_STRUCT(m_svgStyle, inheritedFlags, glyphOrientationVertical, static_cast<unsigned>(value)); }
inline void RenderStyle::setBaselineShift(Style::SVGBaselineShift&& baselineShift) { SET_NESTED(m_svgStyle, miscData, baselineShift, WTFMove(baselineShift)); }
inline void RenderStyle::setCx(Style::SVGCenterCoordinateComponent&& cx) { SET_NESTED(m_svgStyle, layoutData, cx, WTFMove(cx)); }
inline void RenderStyle::setCy(Style::SVGCenterCoordinateComponent&& cy) { SET_NESTED(m_svgStyle, layoutData, cy, WTFMove(cy)); }
inline void RenderStyle::setD(Style::SVGPathData&& d) { SET_NESTED(m_svgStyle, layoutData, d, WTFMove(d)); }
inline void RenderStyle::setFillOpacity(Style::Opacity opacity) { SET_NESTED(m_svgStyle, fillData, opacity, opacity); }
inline void RenderStyle::setFill(Style::SVGPaint&& paint) { SET_NESTED(m_svgStyle, fillData, paint, WTFMove(paint)); }
inline void RenderStyle::setVisitedLinkFill(Style::SVGPaint&& paint) { SET_NESTED(m_svgStyle, fillData, visitedLinkPaint, WTFMove(paint)); }
inline void RenderStyle::setFloodColor(Style::Color&& color) { SET_NESTED(m_svgStyle, miscData, floodColor, WTFMove(color)); }
inline void RenderStyle::setFloodOpacity(Style::Opacity opacity) { SET_NESTED(m_svgStyle, miscData, floodOpacity, opacity); }
inline void RenderStyle::setLightingColor(Style::Color&& color) { SET_NESTED(m_svgStyle, miscData, lightingColor, WTFMove(color)); }
inline void RenderStyle::setR(Style::SVGRadius&& r) { SET_NESTED(m_svgStyle, layoutData, r, WTFMove(r)); }
inline void RenderStyle::setRx(Style::SVGRadiusComponent&& rx) { SET_NESTED(m_svgStyle, layoutData, rx, WTFMove(rx)); }
inline void RenderStyle::setRy(Style::SVGRadiusComponent&& ry) { SET_NESTED(m_svgStyle, layoutData, ry, WTFMove(ry)); }
inline void RenderStyle::setStopColor(Style::Color&& c) { SET_NESTED(m_svgStyle, stopData, color, WTFMove(c)); }
inline void RenderStyle::setStopOpacity(Style::Opacity opacity) { SET_NESTED(m_svgStyle, stopData, opacity, opacity); }
inline void RenderStyle::setStrokeDashArray(Style::SVGStrokeDasharray&& array) { SET_NESTED(m_svgStyle, strokeData, dashArray, WTFMove(array)); }
inline void RenderStyle::setStrokeDashOffset(Style::SVGStrokeDashoffset&& offset) { SET_NESTED(m_svgStyle, strokeData, dashOffset, WTFMove(offset)); }
inline void RenderStyle::setStrokeOpacity(Style::Opacity opacity) { SET_NESTED(m_svgStyle, strokeData, opacity, opacity); }
inline void RenderStyle::setStroke(Style::SVGPaint&& paint) { SET_NESTED(m_svgStyle, strokeData, paint, WTFMove(paint)); }
inline void RenderStyle::setVisitedLinkStroke(Style::SVGPaint&& paint) { SET_NESTED(m_svgStyle, strokeData, visitedLinkPaint, WTFMove(paint)); }
inline void RenderStyle::setX(Style::SVGCoordinateComponent&& x) { SET_NESTED(m_svgStyle, layoutData, x, WTFMove(x)); }
inline void RenderStyle::setY(Style::SVGCoordinateComponent&& y) { SET_NESTED(m_svgStyle, layoutData, y, WTFMove(y)); }

inline void RenderStyle::setMarkerStart(Style::SVGMarkerResource&& marker) { SET_NESTED(m_svgStyle, inheritedResourceData, markerStart, WTFMove(marker)); }
inline void RenderStyle::setMarkerMid(Style::SVGMarkerResource&& marker) { SET_NESTED(m_svgStyle, inheritedResourceData, markerMid, WTFMove(marker)); }
inline void RenderStyle::setMarkerEnd(Style::SVGMarkerResource&& marker) { SET_NESTED(m_svgStyle, inheritedResourceData, markerEnd, WTFMove(marker)); }

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyles(PseudoIdSet pseudoIdSet)
{
    ASSERT(pseudoIdSet);
    ASSERT((pseudoIdSet.data() & PublicPseudoIdMask) == pseudoIdSet.data());
    pseudoBits |= pseudoIdSet.data() >> 1; // Shift down as we do not store a bit for PseudoId::None.
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

inline void RenderStyle::setBorderRadius(Style::BorderRadiusValue&& size)
{
    setBorderTopLeftRadius(Style::BorderRadiusValue { size });
    setBorderTopRightRadius(Style::BorderRadiusValue { size });
    setBorderBottomLeftRadius(Style::BorderRadiusValue { size });
    setBorderBottomRightRadius(WTFMove(size));
}

inline void RenderStyle::setClipPath(Style::ClipPath&& operation)
{
    if (m_nonInheritedData->rareData->clipPath != operation)
        m_nonInheritedData.access().rareData.access().clipPath = WTFMove(operation);
}

inline void RenderStyle::setCursor(Style::Cursor&& cursor)
{
    m_inheritedFlags.cursorType = static_cast<unsigned>(cursor.predefined);
    SET(m_rareInheritedData, cursorImages, WTFMove(cursor.images));
}

inline bool RenderStyle::setDirection(TextDirection bidiDirection)
{
    if (writingMode().computedTextDirection() == bidiDirection)
        return false;
    m_inheritedFlags.writingMode.setTextDirection(bidiDirection);
    return true;
}

inline bool RenderStyle::setUsedZoom(float zoomLevel)
{
    if (compareEqual(m_rareInheritedData->usedZoom, zoomLevel))
        return false;
    m_rareInheritedData.access().usedZoom = zoomLevel;
    return true;
}

inline void RenderStyle::setPseudoElementNameArgument(const AtomString& identifier)
{
    ASSERT(pseudoElementType() == PseudoId::ViewTransitionGroup
        || pseudoElementType() == PseudoId::ViewTransitionImagePair
        || pseudoElementType() == PseudoId::ViewTransitionNew
        || pseudoElementType() == PseudoId::ViewTransitionOld
        || pseudoElementType() == PseudoId::Highlight
        || identifier.isNull());
    SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, identifier);
}

inline void RenderStyle::setGridTemplateColumns(Style::GridTemplateList&& list)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->m_gridTemplateColumns, list))
        m_nonInheritedData.access().rareData.access().grid.access().m_gridTemplateColumns = WTFMove(list);
}

inline void RenderStyle::setGridTemplateRows(Style::GridTemplateList&& list)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->m_gridTemplateRows, list))
        m_nonInheritedData.access().rareData.access().grid.access().m_gridTemplateRows = WTFMove(list);
}

inline void RenderStyle::setHasExplicitlySetDirection()
{
    SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetDirection, true);
}

inline void RenderStyle::setHasExplicitlySetWritingMode()
{
    SET_NESTED(m_nonInheritedData, miscData, hasExplicitlySetWritingMode, true);
}

inline void RenderStyle::setLogicalHeight(Style::PreferredSize&& height)
{
    if (writingMode().isHorizontal())
        setHeight(WTFMove(height));
    else
        setWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalWidth(Style::PreferredSize&& width)
{
    if (writingMode().isHorizontal())
        setWidth(WTFMove(width));
    else
        setHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinWidth(Style::MinimumSize&& width)
{
    if (writingMode().isHorizontal())
        setMinWidth(WTFMove(width));
    else
        setMinHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMaxWidth(Style::MaximumSize&& width)
{
    if (writingMode().isHorizontal())
        setMaxWidth(WTFMove(width));
    else
        setMaxHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinHeight(Style::MinimumSize&& height)
{
    if (writingMode().isHorizontal())
        setMinHeight(WTFMove(height));
    else
        setMinWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalMaxHeight(Style::MaximumSize&& height)
{
    if (writingMode().isHorizontal())
        setMaxHeight(WTFMove(height));
    else
        setMaxWidth(WTFMove(height));
}

inline bool RenderStyle::setTextOrientation(TextOrientation textOrientation)
{
    if (writingMode().computedTextOrientation() == textOrientation)
        return false;
    m_inheritedFlags.writingMode.setTextOrientation(textOrientation);
    return true;
}

inline bool RenderStyle::setWritingMode(StyleWritingMode mode)
{
    if (mode == writingMode().computedWritingMode())
        return false;
    m_inheritedFlags.writingMode.setWritingMode(mode);
    return true;
}

inline bool RenderStyle::setZoom(float zoomLevel)
{
    setUsedZoom(clampTo<float>(usedZoom() * zoomLevel, std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::max()));
    if (compareEqual(m_nonInheritedData->rareData->zoom, zoomLevel))
        return false;
    m_nonInheritedData.access().rareData.access().zoom = zoomLevel;
    return true;
}

inline void RenderStyle::containIntrinsicWidthAddAuto()
{
    setContainIntrinsicWidth(containIntrinsicWidth().addingAuto());
}

inline void RenderStyle::containIntrinsicHeightAddAuto()
{
    setContainIntrinsicHeight(containIntrinsicHeight().addingAuto());
}

inline void RenderStyle::setBlendMode(BlendMode mode)
{
    SET_NESTED(m_nonInheritedData, rareData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET(m_rareInheritedData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}

inline void RenderStyle::setIsForceHidden() { SET(m_rareInheritedData, isForceHidden, true); }
inline void RenderStyle::setIsolation(Isolation isolation) { SET_NESTED(m_nonInheritedData, rareData, isolation, static_cast<unsigned>(isolation)); }

inline void RenderStyle::setAutoRevealsWhenFound() { SET(m_rareInheritedData, autoRevealsWhenFound, true); }

#undef SET
#undef SET_DOUBLY_NESTED
#undef SET_DOUBLY_NESTED_PAIR
#undef SET_NESTED
#undef SET_NESTED_PAIR
#undef SET_NESTED_STRUCT
#undef SET_PAIR
#undef SET_STYLE_PROPERTY
#undef SET_STYLE_PROPERTY_BASE
#undef SET_STYLE_PROPERTY_PAIR

} // namespace WebCore

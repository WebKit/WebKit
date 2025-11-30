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

#define RENDER_STYLE_SETTERS_GENERATED_INCLUDE_TRAP 1
#include "RenderStyleSettersGenerated.h"
#undef RENDER_STYLE_SETTERS_GENERATED_INCLUDE_TRAP

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

// MARK: - Non-property setters

inline void RenderStyle::setEffectiveInert(bool effectiveInert) { SET(m_rareInheritedData, effectiveInert, effectiveInert); }
inline void RenderStyle::setIsEffectivelyTransparent(bool effectivelyTransparent) { SET(m_rareInheritedData, effectivelyTransparent, effectivelyTransparent); }
inline void RenderStyle::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes) { SET(m_rareInheritedData, eventListenerRegionTypes, eventListenerTypes); }
inline void RenderStyle::setHasAttrContent() { SET_NESTED(m_nonInheritedData, miscData, hasAttrContent, true); }
inline void RenderStyle::setHasDisplayAffectedByAnimations() { SET_NESTED(m_nonInheritedData, miscData, hasDisplayAffectedByAnimations, true); }
inline void RenderStyle::setHasPseudoStyles(EnumSet<PseudoElementType> set) { m_nonInheritedFlags.setHasPseudoStyles(set); }
inline void RenderStyle::setTransformStyleForcedToFlat(bool b) { SET_NESTED(m_nonInheritedData, rareData, transformStyleForcedToFlat, static_cast<unsigned>(b)); }
inline void RenderStyle::setUsesAnchorFunctions() { SET_NESTED(m_nonInheritedData, rareData, usesAnchorFunctions, true); }
inline void RenderStyle::setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis> axes) { SET_NESTED(m_nonInheritedData, rareData, anchorFunctionScrollCompensatedAxes, axes.toRaw()); }
inline void RenderStyle::setIsPopoverInvoker() { SET_NESTED(m_nonInheritedData, rareData, isPopoverInvoker, true); }
inline void RenderStyle::setNativeAppearanceDisabled(bool value) { SET_NESTED(m_nonInheritedData, rareData, nativeAppearanceDisabled, value); }
inline void RenderStyle::setIsForceHidden() { SET(m_rareInheritedData, isForceHidden, true); }
inline void RenderStyle::setAutoRevealsWhenFound() { SET(m_rareInheritedData, autoRevealsWhenFound, true); }
inline void RenderStyle::setInsideDefaultButton(bool value) { SET(m_rareInheritedData, insideDefaultButton, value); }
inline void RenderStyle::setInsideSubmitButton(bool value) { SET(m_rareInheritedData, insideSubmitButton, value); }
inline void RenderStyle::addToTextDecorationLineInEffect(Style::TextDecorationLine value) { m_inheritedFlags.textDecorationLineInEffect = textDecorationLineInEffect().addOrReplaceIfNotNone(value); }
inline void RenderStyle::inheritColumnPropertiesFrom(const RenderStyle& parent) { m_nonInheritedData.access().miscData.access().multiCol = parent.m_nonInheritedData->miscData->multiCol; }

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyles(EnumSet<PseudoElementType> pseudoElementSet)
{
    ASSERT(pseudoElementSet);
    ASSERT(pseudoElementSet.containsOnly(allPublicPseudoElementTypes));
    pseudoBits = pseudoElementSet.toRaw();
}

inline void RenderStyle::setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&& identifier)
{
    if (identifier) {
        m_nonInheritedFlags.pseudoElementType = enumToUnderlyingType(identifier->type) + 1;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, WTFMove(identifier->nameArgument));
    } else {
        m_nonInheritedFlags.pseudoElementType = 0;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, nullAtom());
    }
}

// MARK: - Style adjustment utilities

inline void RenderStyle::containIntrinsicWidthAddAuto() { setContainIntrinsicWidth(containIntrinsicWidth().addingAuto()); }
inline void RenderStyle::containIntrinsicHeightAddAuto() { setContainIntrinsicHeight(containIntrinsicHeight().addingAuto()); }

// MARK: - Cache used values

inline void RenderStyle::setUsedAppearance(StyleAppearance a) { SET_NESTED(m_nonInheritedData, miscData, usedAppearance, static_cast<unsigned>(a)); }
inline void RenderStyle::setUsedTouchAction(Style::TouchAction touchAction) { SET(m_rareInheritedData, usedTouchAction, touchAction); }
inline void RenderStyle::setUsedContentVisibility(ContentVisibility usedContentVisibility) { SET(m_rareInheritedData, usedContentVisibility, static_cast<unsigned>(usedContentVisibility)); }
inline void RenderStyle::setUsedZIndex(Style::ZIndex index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoUsedZIndex, static_cast<uint8_t>(index.m_isAuto), usedZIndexValue, index.m_value); }
#if HAVE(CORE_MATERIAL)
inline void RenderStyle::setUsedAppleVisualEffectForSubtree(AppleVisualEffect effect) { SET(m_rareInheritedData, usedAppleVisualEffectForSubtree, static_cast<unsigned>(effect)); }
#endif

inline bool RenderStyle::setUsedZoom(float zoomLevel)
{
    if (compareEqual(m_rareInheritedData->usedZoom, zoomLevel))
        return false;
    m_rareInheritedData.access().usedZoom = zoomLevel;
    return true;
}

// MARK: - reset*()

inline void RenderStyle::resetBorderBottom() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.bottom(), BorderValue()); }
inline void RenderStyle::resetBorderBottomLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderBottomRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.bottomRight(), initialBorderRadius()); }
inline void RenderStyle::resetBorderImage() { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, Style::BorderImage()); }
inline void RenderStyle::resetBorderLeft() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.left(), BorderValue()); }
inline void RenderStyle::resetBorderRight() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.right(), BorderValue()); }
inline void RenderStyle::resetBorderTop() { SET_NESTED(m_nonInheritedData, surroundData, border.m_edges.top(), BorderValue { }); }
inline void RenderStyle::resetBorderTopLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderTopRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.m_radii.topRight(), initialBorderRadius()); }
inline void RenderStyle::resetColumnRule() { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, columnRule, BorderValue()); }
inline void RenderStyle::resetMargin() { SET_NESTED(m_nonInheritedData, surroundData, margin, Style::MarginBox { 0_css_px }); }
inline void RenderStyle::resetPadding() { SET_NESTED(m_nonInheritedData, surroundData, padding, Style::PaddingBox { 0_css_px }); }
inline void RenderStyle::resetPageSize() { SET_NESTED(m_nonInheritedData, rareData, pageSize, Style::PageSize { CSS::Keyword::Auto { } }); }

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

// MARK: - Aggregate setters/ensurers

inline Style::Animations& RenderStyle::ensureAnimations() { return m_nonInheritedData.access().miscData.access().animations.access(); }
inline Style::Transitions& RenderStyle::ensureTransitions() { return m_nonInheritedData.access().miscData.access().transitions.access(); }
inline Style::BackgroundLayers& RenderStyle::ensureBackgroundLayers() { return m_nonInheritedData.access().backgroundData.access().background.access(); }
inline void RenderStyle::setBackgroundLayers(Style::BackgroundLayers&& layers) { SET_NESTED(m_nonInheritedData, backgroundData, background, WTFMove(layers)); }
inline Style::MaskLayers& RenderStyle::ensureMaskLayers() { return m_nonInheritedData.access().miscData.access().mask.access(); }
inline void RenderStyle::setMaskLayers(Style::MaskLayers&& layers) { SET_NESTED(m_nonInheritedData, miscData, mask, WTFMove(layers)); }
inline void RenderStyle::setMaskBorder(Style::MaskBorder&& image) { SET_NESTED(m_nonInheritedData, rareData, maskBorder, WTFMove(image)); }
inline void RenderStyle::setBorderImage(Style::BorderImage&& image) { SET_NESTED(m_nonInheritedData, surroundData, border.m_image, WTFMove(image)); }
inline void RenderStyle::setPerspectiveOrigin(Style::PerspectiveOrigin&& origin) { SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin, WTFMove(origin)); }
inline void RenderStyle::setTransformOrigin(Style::TransformOrigin&& origin) { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin, WTFMove(origin)); }
inline void RenderStyle::setInsetBox(Style::InsetBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, inset, WTFMove(box)); }
inline void RenderStyle::setMarginBox(Style::MarginBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, margin, WTFMove(box)); }
inline void RenderStyle::setPaddingBox(Style::PaddingBox&& box) { SET_NESTED(m_nonInheritedData, surroundData, padding, WTFMove(box)); }

inline void RenderStyle::setBorderRadius(Style::BorderRadiusValue&& size)
{
    setBorderTopLeftRadius(Style::BorderRadiusValue { size });
    setBorderTopRightRadius(Style::BorderRadiusValue { size });
    setBorderBottomLeftRadius(Style::BorderRadiusValue { size });
    setBorderBottomRightRadius(WTFMove(size));
}

// MARK: - Logical setters

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

// MARK: - Property setters

// FIXME: - Below are property setters that are not yet generated

inline void RenderStyle::setGridAutoFlowDirection(Style::GridAutoFlow::Direction direction)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->gridAutoFlow.direction(), direction))
        m_nonInheritedData.access().rareData.access().grid.access().gridAutoFlow.setDirection(direction);
}

// FIXME: Support setters that need to return a `bool` value to indicate if the property changed.
inline bool RenderStyle::setDirection(TextDirection bidiDirection)
{
    if (writingMode().computedTextDirection() == bidiDirection)
        return false;
    m_inheritedFlags.writingMode.setTextDirection(bidiDirection);
    return true;
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

// FIXME: Support properties that set more than one value when set.
inline void RenderStyle::setAppearance(StyleAppearance appearance) { SET_NESTED_PAIR(m_nonInheritedData, miscData, appearance, static_cast<unsigned>(appearance), usedAppearance, static_cast<unsigned>(appearance)); }
inline void RenderStyle::setBlendMode(BlendMode mode)
{
    SET_NESTED(m_nonInheritedData, rareData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET(m_rareInheritedData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}

// FIXME: Add a type that encapsulates both caretColor() and hasAutoCaretColor().
inline void RenderStyle::setCaretColor(Style::Color&& color) { SET_PAIR(m_rareInheritedData, caretColor, WTFMove(color), hasAutoCaretColor, false); }
inline void RenderStyle::setHasAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasAutoCaretColor, true, caretColor, Style::Color::currentColor()); }
inline void RenderStyle::setVisitedLinkCaretColor(Style::Color&& value) { SET_PAIR(m_rareInheritedData, visitedLinkCaretColor, WTFMove(value), hasVisitedLinkAutoCaretColor, false); }
inline void RenderStyle::setHasVisitedLinkAutoCaretColor() { SET_PAIR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, true, visitedLinkCaretColor, Style::Color::currentColor()); }

// FIXME: Support generating properties that have their storage spread out
inline void RenderStyle::setSpecifiedZIndex(Style::ZIndex index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoSpecifiedZIndex, static_cast<uint8_t>(index.m_isAuto), specifiedZIndexValue, index.m_value); }
inline void RenderStyle::setCursor(Style::Cursor&& cursor) { m_inheritedFlags.cursorType = static_cast<unsigned>(cursor.predefined); SET(m_rareInheritedData, cursorImages, WTFMove(cursor.images)); }

// FIXME: Support descriptors
inline void RenderStyle::setPageSize(Style::PageSize&& pageSize) { SET_NESTED(m_nonInheritedData, rareData, pageSize, WTFMove(pageSize)); }

// FIXME: Support generating getter and setter with different names (or rename computedLetterSpacing() to letterSpacing() and computedWordSpacing() to wordSpacing())
inline void RenderStyle::setWordSpacing(Style::WordSpacing&& wordSpacing) { SET_NESTED(m_inheritedData, fontData, wordSpacing, WTFMove(wordSpacing)); }
inline void RenderStyle::setLetterSpacing(Style::LetterSpacing&& letterSpacing) { SET_NESTED(m_inheritedData, fontData, letterSpacing, WTFMove(letterSpacing)); }

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

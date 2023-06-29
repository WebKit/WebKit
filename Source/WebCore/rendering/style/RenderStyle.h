/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
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

#include <unicode/utypes.h>
#include <wtf/DataRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class AnimationList;
class AutosizeStatus;
class BorderData;
class BorderValue;
class CSSCustomPropertyValue;
class Color;
class ContentData;
class CounterContent;
class CursorList;
class FillLayer;
class FilterOperations;
class FloatPoint;
class FloatPoint3D;
class FloatRect;
class FontCascade;
class FontCascadeDescription;
class FontMetrics;
class FontSelectionValue;
class GapLength;
class GridPosition;
class GridTrackSize;
class IntPoint;
class IntSize;
class LayoutRect;
class LayoutSize;
class LayoutUnit;
class LengthBox;
class LineClampValue;
class NinePieceImage;
class OffsetRotation;
class PathOperation;
class PseudoIdSet;
class QuotesData;
class RenderObject;
class RenderStyle;
class RotateTransformOperation;
class RoundedRect;
class SVGLengthValue;
class SVGRenderStyle;
class ScaleTransformOperation;
class ShadowData;
class ShapeValue;
class StyleColor;
class StyleColorScheme;
class StyleContentAlignmentData;
class StyleCustomPropertyData;
class StyleImage;
class StyleInheritedData;
class StyleNonInheritedData;
class StyleRareInheritedData;
class StyleReflection;
class StyleScrollSnapArea;
class StyleSelfAlignmentData;
class TextDecorationThickness;
class TextSizeAdjustment;
class TextUnderlineOffset;
class TransformOperations;
class TransformationMatrix;
class TranslateTransformOperation;
class WillChangeData;
class WordBoundaryDetection;

enum CSSPropertyID : uint16_t;
enum GridAutoFlow : uint8_t;
enum PageSizeType : uint8_t;
enum PaginationMode : uint8_t;

enum class ApplePayButtonStyle : uint8_t;
enum class ApplePayButtonType : uint8_t;
enum class AspectRatioType : uint8_t;
enum class AutoRepeatType : uint8_t;
enum class BackfaceVisibility : uint8_t;
enum class BlendMode : uint8_t;
enum class BlockStepInsert : bool;
enum class BorderCollapse : bool;
enum class BorderStyle : uint8_t;
enum class BoxAlignment : uint8_t;
enum class BoxDecorationBreak : bool;
enum class BoxDirection : bool;
enum class BoxLines : bool;
enum class BoxOrient : bool;
enum class BoxPack : uint8_t;
enum class BoxSizing : bool;
enum class BreakBetween : uint8_t;
enum class BreakInside : uint8_t;
enum class CaptionSide : uint8_t;
enum class Clear : uint8_t;
enum class ColumnAxis : uint8_t;
enum class ColumnFill : bool;
enum class ColumnProgression : bool;
enum class ColumnSpan : bool;
enum class CompositeOperator : uint8_t;
enum class ContainIntrinsicSizeType : uint8_t;
enum class ContainerType : uint8_t;
enum class Containment : uint8_t;
enum class ContentDistribution : uint8_t;
enum class ContentPosition : uint8_t;
enum class ContentVisibility : uint8_t;
enum class CursorType : uint8_t;
enum class CursorVisibility : bool;
enum class DisplayType : uint8_t;
enum class EmptyCell : bool;
enum class EventListenerRegionType : uint8_t;
enum class FillAttachment : uint8_t;
enum class FillBox : uint8_t;
enum class FillSizeType : uint8_t;
enum class FlexDirection : uint8_t;
enum class FlexWrap : uint8_t;
enum class Float : uint8_t;
enum class FontOrientation : bool;
enum class HangingPunctuation : uint8_t;
enum class Hyphens : uint8_t;
enum class ImageRendering : uint8_t;
enum class ImageResolutionSnap : bool;
enum class ImageResolutionSource : bool;
enum class InputSecurity : bool;
enum class InsideLink : uint8_t;
enum class Isolation : bool;
enum class ItemPosition : uint8_t;
enum class LengthType : uint8_t;
enum class LineAlign : bool;
enum class LineBoxContain : uint8_t;
enum class LineBreak : uint8_t;
enum class LineCap : uint8_t;
enum class LineJoin : uint8_t;
enum class LineSnap : uint8_t;
enum class ListStylePosition : bool;
enum class MarginTrimType : uint8_t;
enum class MarqueeBehavior : uint8_t;
enum class MarqueeDirection : uint8_t;
enum class MathStyle : bool;
enum class NBSPMode : bool;
enum class NinePieceImageRule : uint8_t;
enum class NonCJKGlyphOrientation : bool;
enum class ObjectFit : uint8_t;
enum class Order : bool;
enum class OutlineIsAuto : bool;
enum class Overflow : uint8_t;
enum class OverflowAnchor : bool;
enum class OverflowWrap : uint8_t;
enum class OverscrollBehavior : uint8_t;
enum class PaintOrder : uint8_t;
enum class PaintType : uint8_t;
enum class PointerEvents : uint8_t;
enum class PositionType : uint8_t;
enum class PrintColorAdjust : bool;
enum class PseudoId : uint16_t;
enum class QuoteType : uint8_t;
enum class Resize : uint8_t;
enum class RubyPosition : uint8_t;
enum class SVGPaintType : uint8_t;
enum class ScrollSnapStop : bool;
enum class ScrollbarWidth : uint8_t;
enum class SpeakAs : uint8_t;
enum class StyleAppearance : uint8_t;
enum class StyleDifference : uint8_t;
enum class StyleDifferenceContextSensitiveProperty : uint8_t;
enum class TableLayoutType : bool;
enum class TextAlignLast : uint8_t;
enum class TextAlignMode : uint8_t;
enum class TextBoxTrim : uint8_t;
enum class TextCombine : bool;
enum class TextDecorationLine : uint8_t;
enum class TextDecorationSkipInk : uint8_t;
enum class TextDecorationStyle : uint8_t;
enum class TextDirection : bool;
enum class TextEmphasisFill : bool;
enum class TextEmphasisMark : uint8_t;
enum class TextEmphasisPosition : uint8_t;
enum class TextGroupAlign : uint8_t;
enum class TextIndentLine : bool;
enum class TextIndentType : bool;
enum class TextJustify : uint8_t;
enum class TextOrientation : uint8_t;
enum class TextOverflow : bool;
enum class TextSecurity : uint8_t;
enum class TextTransform : uint8_t;
enum class TextUnderlinePosition : uint8_t;
enum class TextWrap : uint8_t;
enum class TextZoom : bool;
enum class TouchAction : uint8_t;
enum class TransformBox : uint8_t;
enum class TransformStyle3D : uint8_t;
enum class UnicodeBidi : uint8_t;
enum class UsedClear : uint8_t;
enum class UsedFloat : uint8_t;
enum class UserDrag : uint8_t;
enum class UserModify : uint8_t;
enum class UserSelect : uint8_t;
enum class VerticalAlign : uint8_t;
enum class Visibility : uint8_t;
enum class WhiteSpace : uint8_t;
enum class WhiteSpaceCollapse : uint8_t;
enum class WordBreak : uint8_t;
enum class WritingMode : uint8_t;

struct BorderDataRadii;
struct CounterDirectiveMap;
struct FillRepeatXY;
struct FontPalette;
struct FontSizeAdjust;
struct GridTrackList;
struct ImageOrientation;
struct Length;
struct LengthPoint;
struct LengthSize;
struct ListStyleType;
struct MasonryAutoFlow;
struct NamedGridAreaMap;
struct NamedGridLinesMap;
struct OrderedNamedGridLinesMap;
struct ScrollSnapAlign;
struct ScrollSnapType;
struct ScrollbarGutter;
struct ScrollbarColor;

struct TabSize;
struct TextAutospace;
struct TextBoxEdge;
struct TextSpacingTrim;

template<typename> class FontTaggedSettings;
template<typename> class RectEdges;

using FloatBoxExtent = RectEdges<float>;
using FontVariationSettings = FontTaggedSettings<float>;
using IntOutsets = RectEdges<int>;
using LayoutBoxExtent = RectEdges<LayoutUnit>;

namespace Style {
class CustomPropertyRegistry;
}

constexpr auto PublicPseudoIDBits = 9;
constexpr auto TextDecorationLineBits = 4;
constexpr auto TextTransformBits = 5;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
struct PseudoStyleCache {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
    Vector<std::unique_ptr<RenderStyle>, 4> styles;
};

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

    WEBCORE_EXPORT static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();
    static std::unique_ptr<RenderStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static RenderStyle clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool operator==(const RenderStyle&) const;

    void inheritFrom(const RenderStyle&);
    void inheritIgnoringCustomPropertiesFrom(const RenderStyle&);
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

    inline const StyleCustomPropertyData& inheritedCustomProperties() const;
    inline const StyleCustomPropertyData& nonInheritedCustomProperties() const;
    const CSSCustomPropertyValue* customPropertyValue(const AtomString&) const;

    void deduplicateCustomProperties(const RenderStyle&);
    void setCustomPropertyValue(Ref<const CSSCustomPropertyValue>&&, bool isInherited);
    bool customPropertiesEqual(const RenderStyle&) const;

    void setUsesViewportUnits() { m_nonInheritedFlags.usesViewportUnits = true; }
    bool usesViewportUnits() const { return m_nonInheritedFlags.usesViewportUnits; }
    void setUsesContainerUnits() { m_nonInheritedFlags.usesContainerUnits = true; }
    bool usesContainerUnits() const { return m_nonInheritedFlags.usesContainerUnits; }

    void setColumnStylesFromPaginationMode(PaginationMode);
    
    inline bool isFloating() const;
    inline bool hasMargin() const;
    inline bool hasBorder() const;
    inline bool hasBorderImage() const;
    inline bool hasVisibleBorderDecoration() const;
    inline bool hasVisibleBorder() const;
    inline bool hasPadding() const;
    inline bool hasOffset() const;

    inline bool hasBackgroundImage() const;
    inline bool hasAnyFixedBackground() const;

    bool hasEntirelyFixedBackground() const;
    inline bool hasAnyLocalBackground() const;

    inline bool hasAppearance() const;
    inline bool hasEffectiveAppearance() const;

    inline bool hasBackground() const;
    
    LayoutBoxExtent imageOutsets(const NinePieceImage&) const;
    inline bool hasBorderImageOutsets() const;
    inline LayoutBoxExtent borderImageOutsets() const;

    inline LayoutBoxExtent maskBoxImageOutsets() const;

    inline IntOutsets filterOutsets() const;

    Order rtlOrdering() const { return static_cast<Order>(m_inheritedFlags.rtlOrdering); }
    void setRTLOrdering(Order ordering) { m_inheritedFlags.rtlOrdering = static_cast<unsigned>(ordering); }

    bool isStyleAvailable() const;

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoId) const;
    inline void setHasPseudoStyles(PseudoIdSet);
    bool hasUniquePseudoStyle() const;

    // attribute getter methods

    constexpr DisplayType display() const { return static_cast<DisplayType>(m_nonInheritedFlags.effectiveDisplay); }

    inline const Length& left() const;
    inline const Length& right() const;
    inline const Length& top() const;
    inline const Length& bottom() const;

    // Accessors for positioned object edges that take into account writing mode.
    inline const Length& logicalLeft() const;
    inline const Length& logicalRight() const;
    inline const Length& logicalTop() const;
    inline const Length& logicalBottom() const;

    // Whether or not a positioned element requires normal flow x/y to be computed to determine its position.
    inline bool hasStaticInlinePosition(bool horizontal) const;
    inline bool hasStaticBlockPosition(bool horizontal) const;

    PositionType position() const { return static_cast<PositionType>(m_nonInheritedFlags.position); }
    inline bool hasOutOfFlowPosition() const;
    inline bool hasInFlowPosition() const;
    inline bool hasViewportConstrainedPosition() const;
    Float floating() const { return static_cast<Float>(m_nonInheritedFlags.floating); }
    static UsedFloat usedFloat(const RenderObject&);

    inline const Length& width() const;
    inline const Length& height() const;
    inline const Length& minWidth() const;
    inline const Length& maxWidth() const;
    inline const Length& minHeight() const;
    inline const Length& maxHeight() const;
    
    inline const Length& logicalWidth() const;
    inline const Length& logicalHeight() const;
    inline const Length& logicalMinWidth() const;
    inline const Length& logicalMaxWidth() const;
    inline const Length& logicalMinHeight() const;
    inline const Length& logicalMaxHeight() const;

    inline const BorderData& border() const;
    inline const BorderValue& borderLeft() const;
    inline const BorderValue& borderRight() const;
    inline const BorderValue& borderTop() const;
    inline const BorderValue& borderBottom() const;

    const BorderValue& borderBefore() const;
    const BorderValue& borderAfter() const;
    const BorderValue& borderStart() const;
    const BorderValue& borderEnd() const;

    inline const NinePieceImage& borderImage() const;
    inline StyleImage* borderImageSource() const;
    inline const LengthBox& borderImageSlices() const;
    inline const LengthBox& borderImageWidth() const;
    inline const LengthBox& borderImageOutset() const;
    inline NinePieceImageRule borderImageHorizontalRule() const;
    inline NinePieceImageRule borderImageVerticalRule() const;

    inline const LengthSize& borderTopLeftRadius() const;
    inline const LengthSize& borderTopRightRadius() const;
    inline const LengthSize& borderBottomLeftRadius() const;
    inline const LengthSize& borderBottomRightRadius() const;
    inline const BorderDataRadii& borderRadii() const;
    inline bool hasBorderRadius() const;
    inline bool hasExplicitlySetBorderBottomLeftRadius() const;
    inline bool hasExplicitlySetBorderBottomRightRadius() const;
    inline bool hasExplicitlySetBorderTopLeftRadius() const;
    inline bool hasExplicitlySetBorderTopRightRadius() const;
    inline bool hasExplicitlySetBorderRadius() const;

    inline float borderLeftWidth() const;
    inline BorderStyle borderLeftStyle() const;
    inline bool borderLeftIsTransparent() const;
    inline float borderRightWidth() const;
    inline BorderStyle borderRightStyle() const;
    inline bool borderRightIsTransparent() const;
    inline float borderTopWidth() const;
    inline BorderStyle borderTopStyle() const;
    inline bool borderTopIsTransparent() const;
    inline float borderBottomWidth() const;
    inline BorderStyle borderBottomStyle() const;
    inline bool borderBottomIsTransparent() const;
    inline FloatBoxExtent borderWidth() const;

    float borderBeforeWidth() const;
    float borderAfterWidth() const;
    float borderStartWidth() const;
    float borderEndWidth() const;

    float outlineSize() const { return std::max<float>(0, outlineWidth() + outlineOffset()); }
    float outlineWidth() const;
    inline bool hasOutline() const;
    inline BorderStyle outlineStyle() const;
    inline OutlineIsAuto outlineStyleIsAuto() const;
    inline bool hasOutlineInVisualOverflow() const;
    
    Overflow overflowX() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowX); }
    Overflow overflowY() const { return static_cast<Overflow>(m_nonInheritedFlags.overflowY); }
    inline bool isOverflowVisible() const;

    inline OverscrollBehavior overscrollBehaviorX() const;
    inline OverscrollBehavior overscrollBehaviorY() const;
    
    Visibility visibility() const { return static_cast<Visibility>(m_inheritedFlags.visibility); }
    VerticalAlign verticalAlign() const { return static_cast<VerticalAlign>(m_nonInheritedFlags.verticalAlign); }
    const Length& verticalAlignLength() const;

    inline const Length& clipLeft() const;
    inline const Length& clipRight() const;
    inline const Length& clipTop() const;
    inline const Length& clipBottom() const;
    inline const LengthBox& clip() const;
    inline bool hasClip() const;

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
    std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();

    inline FontVariationSettings fontVariationSettings() const;
    inline FontSelectionValue fontWeight() const;
    inline FontSelectionValue fontStretch() const;
    inline std::optional<FontSelectionValue> fontItalic() const;
    inline FontPalette fontPalette() const;
    inline FontSizeAdjust fontSizeAdjust() const;

    inline const Length& textIndent() const;
    inline TextAlignMode textAlign() const { return static_cast<TextAlignMode>(m_inheritedFlags.textAlign); }
    inline TextAlignLast textAlignLast() const;
    inline TextGroupAlign textGroupAlign() const;
    inline OptionSet<TextTransform> textTransform() const;
    inline OptionSet<TextDecorationLine> textDecorationsInEffect() const;
    inline OptionSet<TextDecorationLine> textDecorationLine() const;
    inline TextDecorationStyle textDecorationStyle() const;
    inline TextDecorationSkipInk textDecorationSkipInk() const;
    inline TextUnderlinePosition textUnderlinePosition() const;
    inline TextUnderlineOffset textUnderlineOffset() const;
    inline TextDecorationThickness textDecorationThickness() const;

    inline TextIndentLine textIndentLine() const;
    inline TextIndentType textIndentType() const;
    inline TextJustify textJustify() const;

    inline TextBoxTrim textBoxTrim() const;
    TextBoxEdge textBoxEdge() const;

    inline OptionSet<MarginTrimType> marginTrim() const;

    const Length& wordSpacing() const;
    float letterSpacing() const;
    TextSpacingTrim textSpacingTrim() const;
    TextAutospace textAutospace() const;

    inline float zoom() const;
    inline float effectiveZoom() const;
    
    inline TextZoom textZoom() const;

    TextDirection direction() const { return static_cast<TextDirection>(m_inheritedFlags.direction); }
    inline bool isLeftToRightDirection() const;
    bool hasExplicitlySetDirection() const { return m_nonInheritedFlags.hasExplicitlySetDirection; }

    const Length& specifiedLineHeight() const;
    WEBCORE_EXPORT const Length& lineHeight() const;
    WEBCORE_EXPORT int computedLineHeight() const;
    int computeLineHeight(const Length&) const;

    WhiteSpace whiteSpace() const;
    static constexpr bool autoWrap(WhiteSpace);
    inline bool autoWrap() const;
    static constexpr bool preserveNewline(WhiteSpace);
    inline bool preserveNewline() const;
    static constexpr bool collapseWhiteSpace(WhiteSpace);
    inline bool collapseWhiteSpace() const;
    inline bool isCollapsibleWhiteSpace(UChar) const;
    inline bool breakOnlyAfterWhiteSpace() const;
    inline bool breakWords() const;

    WhiteSpaceCollapse whiteSpaceCollapse() const { return static_cast<WhiteSpaceCollapse>(m_inheritedFlags.whiteSpaceCollapse); }
    TextWrap textWrap() const { return static_cast<TextWrap>(m_inheritedFlags.textWrap); }

    inline FillRepeatXY backgroundRepeat() const;
    inline FillAttachment backgroundAttachment() const;
    inline FillBox backgroundClip() const;
    inline FillBox backgroundOrigin() const;
    inline const Length& backgroundXPosition() const;
    inline const Length& backgroundYPosition() const;
    inline FillSizeType backgroundSizeType() const;
    inline const LengthSize& backgroundSizeLength() const;
    inline FillLayer& ensureBackgroundLayers();
    inline const FillLayer& backgroundLayers() const;
    inline BlendMode backgroundBlendMode() const;

    inline StyleImage* maskImage() const;
    inline FillRepeatXY maskRepeat() const;
    inline CompositeOperator maskComposite() const;
    inline FillBox maskClip() const;
    inline FillBox maskOrigin() const;
    inline const Length& maskXPosition() const;
    inline const Length& maskYPosition() const;
    inline FillSizeType maskSizeType() const;
    inline const LengthSize& maskSizeLength() const;
    inline FillLayer& ensureMaskLayers();
    inline const FillLayer& maskLayers() const;
    inline const NinePieceImage& maskBoxImage() const;
    inline StyleImage* maskBoxImageSource() const;

    BorderCollapse borderCollapse() const { return static_cast<BorderCollapse>(m_inheritedFlags.borderCollapse); }
    float horizontalBorderSpacing() const;
    float verticalBorderSpacing() const;
    EmptyCell emptyCells() const { return static_cast<EmptyCell>(m_inheritedFlags.emptyCells); }
    CaptionSide captionSide() const { return static_cast<CaptionSide>(m_inheritedFlags.captionSide); }

    inline ListStyleType listStyleType() const;
    StyleImage* listStyleImage() const;
    ListStylePosition listStylePosition() const { return static_cast<ListStylePosition>(m_inheritedFlags.listStylePosition); }
    inline bool isFixedTableLayout() const;
    inline const Length& marginTop() const;
    inline const Length& marginBottom() const;
    inline const Length& marginLeft() const;
    inline const Length& marginRight() const;
    inline const Length& marginBefore() const;
    inline const Length& marginAfter() const;
    inline const Length& marginStart() const;
    inline const Length& marginEnd() const;
    inline const Length& marginStartUsing(const RenderStyle* otherStyle) const;
    inline const Length& marginEndUsing(const RenderStyle* otherStyle) const;
    inline const Length& marginBeforeUsing(const RenderStyle* otherStyle) const;
    inline const Length& marginAfterUsing(const RenderStyle* otherStyle) const;

    inline const LengthBox& paddingBox() const;
    inline const Length& paddingTop() const;
    inline const Length& paddingBottom() const;
    inline const Length& paddingLeft() const;
    inline const Length& paddingRight() const;
    inline const Length& paddingBefore() const;
    inline const Length& paddingAfter() const;
    inline const Length& paddingStart() const;
    inline const Length& paddingEnd() const;

    CursorType cursor() const { return static_cast<CursorType>(m_inheritedFlags.cursor); }

#if ENABLE(CURSOR_VISIBILITY)
    CursorVisibility cursorVisibility() const { return static_cast<CursorVisibility>(m_inheritedFlags.cursorVisibility); }
#endif

    CursorList* cursors() const;

    InsideLink insideLink() const { return static_cast<InsideLink>(m_inheritedFlags.insideLink); }
    bool isLink() const { return m_nonInheritedFlags.isLink; }

    bool insideDefaultButton() const { return m_inheritedFlags.insideDefaultButton; }

    inline unsigned short widows() const;
    inline unsigned short orphans() const;
    inline bool hasAutoWidows() const;
    inline bool hasAutoOrphans() const;

    inline BreakInside breakInside() const;
    inline BreakBetween breakBefore() const;
    inline BreakBetween breakAfter() const;

    OptionSet<HangingPunctuation> hangingPunctuation() const;

    float outlineOffset() const;
    const ShadowData* textShadow() const;
    LayoutBoxExtent textShadowExtent() const;
    inline void getTextShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const;
    inline void getTextShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const;

    inline float textStrokeWidth() const;
    inline float opacity() const;
    inline bool hasOpacity() const;
    inline StyleAppearance appearance() const;
    inline StyleAppearance effectiveAppearance() const;
    inline AspectRatioType aspectRatioType() const;
    inline double aspectRatioWidth() const;
    inline double aspectRatioHeight() const;
    inline double aspectRatioLogicalWidth() const;
    inline double aspectRatioLogicalHeight() const;
    inline double logicalAspectRatio() const;
    inline BoxSizing boxSizingForAspectRatio() const;
    inline bool hasAspectRatio() const;
    inline OptionSet<Containment> contain() const;
    inline OptionSet<Containment> effectiveContainment() const;
    inline bool containsLayout() const;
    inline bool containsSize() const;
    inline bool containsInlineSize() const;
    inline bool containsSizeOrInlineSize() const;
    inline bool containsStyle() const;
    inline bool containsPaint() const;
    inline bool containsLayoutOrPaint() const;
    inline ContainerType containerType() const;
    inline const Vector<AtomString>& containerNames() const;

    inline ContentVisibility contentVisibility() const;

    inline bool effectiveSkipsContent() const;

    inline ContainIntrinsicSizeType containIntrinsicWidthType() const;
    inline ContainIntrinsicSizeType containIntrinsicHeightType() const;
    inline ContainIntrinsicSizeType containIntrinsicLogicalWidthType() const;
    inline ContainIntrinsicSizeType containIntrinsicLogicalHeightType() const;
    inline bool containIntrinsicWidthHasAuto() const;
    inline bool containIntrinsicHeightHasAuto() const;
    inline std::optional<Length> containIntrinsicWidth() const;
    inline std::optional<Length> containIntrinsicHeight() const;
    inline bool hasAutoLengthContainIntrinsicSize() const;

    inline BoxAlignment boxAlign() const;
    BoxDirection boxDirection() const { return static_cast<BoxDirection>(m_inheritedFlags.boxDirection); }
    inline float boxFlex() const;
    inline unsigned boxFlexGroup() const;
    inline BoxLines boxLines() const;
    inline unsigned boxOrdinalGroup() const;
    inline BoxOrient boxOrient() const;
    inline BoxPack boxPack() const;

    inline int order() const;
    inline float flexGrow() const;
    inline float flexShrink() const;
    inline const Length& flexBasis() const;
    inline const StyleContentAlignmentData& alignContent() const;
    inline const StyleSelfAlignmentData& alignItems() const;
    inline const StyleSelfAlignmentData& alignSelf() const;
    inline FlexDirection flexDirection() const;
    inline bool isColumnFlexDirection() const;
    inline bool isReverseFlexDirection() const;
    inline FlexWrap flexWrap() const;
    inline const StyleContentAlignmentData& justifyContent() const;
    inline const StyleSelfAlignmentData& justifyItems() const;
    inline const StyleSelfAlignmentData& justifySelf() const;

    inline const Vector<GridTrackSize>& gridColumnTrackSizes() const;
    inline const Vector<GridTrackSize>& gridRowTrackSizes() const;
    inline const GridTrackList& gridColumnList() const;
    inline const GridTrackList& gridRowList() const;
    inline const Vector<GridTrackSize>& gridAutoRepeatColumns() const;
    inline const Vector<GridTrackSize>& gridAutoRepeatRows() const;
    inline unsigned gridAutoRepeatColumnsInsertionPoint() const;
    inline unsigned gridAutoRepeatRowsInsertionPoint() const;
    inline AutoRepeatType gridAutoRepeatColumnsType() const;
    inline AutoRepeatType gridAutoRepeatRowsType() const;
    inline const NamedGridLinesMap& namedGridColumnLines() const;
    inline const NamedGridLinesMap& namedGridRowLines() const;
    inline const OrderedNamedGridLinesMap& orderedNamedGridColumnLines() const;
    inline const OrderedNamedGridLinesMap& orderedNamedGridRowLines() const;
    inline const NamedGridLinesMap& autoRepeatNamedGridColumnLines() const;
    inline const NamedGridLinesMap& autoRepeatNamedGridRowLines() const;
    inline const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridColumnLines() const;
    inline const OrderedNamedGridLinesMap& autoRepeatOrderedNamedGridRowLines() const;
    inline const NamedGridLinesMap& implicitNamedGridColumnLines() const;
    inline const NamedGridLinesMap& implicitNamedGridRowLines() const;
    inline const NamedGridAreaMap& namedGridArea() const;
    inline size_t namedGridAreaRowCount() const;
    inline size_t namedGridAreaColumnCount() const;
    inline GridAutoFlow gridAutoFlow() const;
    inline const Vector<StyleContentAlignmentData>& alignTracks() const;
    inline const Vector<StyleContentAlignmentData>& justifyTracks() const;
    inline MasonryAutoFlow masonryAutoFlow() const;
    inline bool gridSubgridRows() const;
    inline bool gridSubgridColumns() const;
    inline bool gridMasonryRows() const;
    inline bool gridMasonryColumns() const;
    inline bool isGridAutoFlowDirectionRow() const;
    inline bool isGridAutoFlowDirectionColumn() const;
    inline bool isGridAutoFlowAlgorithmSparse() const;
    inline bool isGridAutoFlowAlgorithmDense() const;
    inline const Vector<GridTrackSize>& gridAutoColumns() const;
    inline const Vector<GridTrackSize>& gridAutoRows() const;

    inline const GridPosition& gridItemColumnStart() const;
    inline const GridPosition& gridItemColumnEnd() const;
    inline const GridPosition& gridItemRowStart() const;
    inline const GridPosition& gridItemRowEnd() const;

    inline const ShadowData* boxShadow() const;
    inline LayoutBoxExtent boxShadowExtent() const;
    inline LayoutBoxExtent boxShadowInsetExtent() const;
    inline void getBoxShadowHorizontalExtent(LayoutUnit& left, LayoutUnit& right) const;
    inline void getBoxShadowVerticalExtent(LayoutUnit& top, LayoutUnit& bottom) const;
    inline void getBoxShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const;
    inline void getBoxShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const;

#if ENABLE(CSS_BOX_DECORATION_BREAK)
    inline BoxDecorationBreak boxDecorationBreak() const;
#endif

    inline StyleReflection* boxReflect() const;
    inline BoxSizing boxSizing() const;
    inline const Length& marqueeIncrement() const;
    inline int marqueeSpeed() const;
    inline int marqueeLoopCount() const;
    inline MarqueeBehavior marqueeBehavior() const;
    inline MarqueeDirection marqueeDirection() const;
    inline UserModify effectiveUserModify() const;
    inline UserModify userModify() const;
    inline UserDrag userDrag() const;
    WEBCORE_EXPORT UserSelect effectiveUserSelect() const;
    inline UserSelect userSelect() const;
    inline TextOverflow textOverflow() const;
    inline WordBreak wordBreak() const;
    inline OverflowWrap overflowWrap() const;
    inline NBSPMode nbspMode() const;
    inline LineBreak lineBreak() const;
    inline Hyphens hyphens() const;
    inline short hyphenationLimitBefore() const;
    inline short hyphenationLimitAfter() const;
    inline short hyphenationLimitLines() const;
    inline const AtomString& hyphenationString() const;
    inline const AtomString& computedLocale() const;
    inline const AtomString& specifiedLocale() const;
    inline Resize resize() const;
    inline ColumnAxis columnAxis() const;
    inline bool hasInlineColumnAxis() const;
    inline ColumnProgression columnProgression() const;
    inline float columnWidth() const;
    inline bool hasAutoColumnWidth() const;
    inline unsigned short columnCount() const;
    inline bool hasAutoColumnCount() const;
    inline bool specifiesColumns() const;
    inline ColumnFill columnFill() const;
    inline const GapLength& columnGap() const;
    inline const GapLength& rowGap() const;
    inline BorderStyle columnRuleStyle() const;
    inline unsigned short columnRuleWidth() const;
    inline bool columnRuleIsTransparent() const;
    inline ColumnSpan columnSpan() const;
    inline const WordBoundaryDetection& wordBoundaryDetection() const;

    inline const TransformOperations& transform() const;
    inline bool hasTransform() const;
    inline const Length& transformOriginX() const;
    inline const Length& transformOriginY() const;
    inline float transformOriginZ() const;
    inline LengthPoint transformOriginXY() const;

    inline TransformBox transformBox() const;

    inline RotateTransformOperation* rotate() const;
    inline ScaleTransformOperation* scale() const;
    inline TranslateTransformOperation* translate() const;

    inline bool affectsTransform() const;

    inline TextEmphasisFill textEmphasisFill() const;
    TextEmphasisMark textEmphasisMark() const;
    inline const AtomString& textEmphasisCustomMark() const;
    inline OptionSet<TextEmphasisPosition> textEmphasisPosition() const;
    const AtomString& textEmphasisMarkString() const;

    inline RubyPosition rubyPosition() const;

#if ENABLE(DARK_MODE_CSS)
    inline StyleColorScheme colorScheme() const;
    void setHasExplicitlySetColorScheme(bool v) { m_nonInheritedFlags.hasExplicitlySetColorScheme = v; }
    bool hasExplicitlySetColorScheme() const { return m_nonInheritedFlags.hasExplicitlySetColorScheme; };
#endif

    inline TextOrientation textOrientation() const;

    inline ObjectFit objectFit() const;
    inline const LengthPoint& objectPosition() const;

    // Return true if any transform related property (currently transform, translate, scale, rotate, transformStyle3D or perspective)
    // indicates that we are transforming. The usedTransformStyle3D is not used here because in many cases (such as for deciding
    // whether or not to establish a containing block), the computed value is what matters.
    inline bool hasTransformRelatedProperty() const;

    enum class TransformOperationOption : uint8_t {
        TransformOrigin = 1 << 0,
        Translate       = 1 << 1,
        Rotate          = 1 << 2,
        Scale           = 1 << 3,
        Offset          = 1 << 4
    };

    static constexpr OptionSet<TransformOperationOption> allTransformOperations();
    static constexpr OptionSet<TransformOperationOption> individualTransformOperations();

    bool affectedByTransformOrigin() const;

    FloatPoint computePerspectiveOrigin(const FloatRect& boundingBox) const;
    void applyPerspective(TransformationMatrix&, const FloatPoint& originTranslate) const;

    FloatPoint3D computeTransformOrigin(const FloatRect& boundingBox) const;
    void applyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;
    void unapplyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;

    // applyTransform calls applyTransformOrigin(), then applyCSSTransform(), followed by unapplyTransformOrigin().
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox) const;
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, OptionSet<TransformOperationOption>) const;
    void applyCSSTransform(TransformationMatrix&, const FloatRect& boundingBox) const;
    void applyCSSTransform(TransformationMatrix&, const FloatRect& boundingBox, OptionSet<TransformOperationOption>) const;
    void applyMotionPathTransform(TransformationMatrix&, const FloatRect& boundingBox) const;
    void setPageScaleTransform(float);

    inline bool hasPositionedMask() const;
    inline bool hasMask() const;

    inline TextCombine textCombine() const;
    inline bool hasTextCombine() const;

    inline const TabSize& tabSize() const;

    // End CSS3 Getters

    inline const AtomString& lineGrid() const;
    inline LineSnap lineSnap() const;
    inline LineAlign lineAlign() const;

    PointerEvents pointerEvents() const { return static_cast<PointerEvents>(m_inheritedFlags.pointerEvents); }
    inline PointerEvents effectivePointerEvents() const;
    inline const AnimationList* animations() const;
    inline const AnimationList* transitions() const;

    AnimationList* animations();
    AnimationList* transitions();
    
    inline bool hasAnimationsOrTransitions() const;

    AnimationList& ensureAnimations();
    AnimationList& ensureTransitions();

    inline bool hasAnimations() const;
    inline bool hasTransitions() const;

    inline TransformStyle3D transformStyle3D() const;
    inline TransformStyle3D usedTransformStyle3D() const;
    inline bool preserves3D() const;

    inline BackfaceVisibility backfaceVisibility() const;
    inline float perspective() const;
    inline float usedPerspective() const;
    inline bool hasPerspective() const;
    inline const Length& perspectiveOriginX() const;
    inline const Length& perspectiveOriginY() const;
    inline LengthPoint perspectiveOrigin() const;

    inline const LengthSize& pageSize() const;
    inline PageSizeType pageSizeType() const;

    inline OptionSet<LineBoxContain> lineBoxContain() const;
    inline const LineClampValue& lineClamp() const;
    inline const IntSize& initialLetter() const;
    inline int initialLetterDrop() const;
    inline int initialLetterHeight() const;

    inline OptionSet<TouchAction> touchActions() const;
    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    inline OptionSet<TouchAction> effectiveTouchActions() const;
    inline OptionSet<EventListenerRegionType> eventListenerRegionTypes() const;

    inline bool effectiveInert() const;

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

    inline std::optional<ScrollbarColor> scrollbarColor() const;
    inline const StyleColor& scrollbarThumbColor() const;
    inline const StyleColor& scrollbarTrackColor() const;
    WEBCORE_EXPORT const ScrollbarGutter scrollbarGutter() const;
    WEBCORE_EXPORT ScrollbarWidth scrollbarWidth() const;

#if ENABLE(TOUCH_EVENTS)
    inline StyleColor tapHighlightColor() const;
#endif

#if PLATFORM(IOS_FAMILY)
    inline bool touchCalloutEnabled() const;
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    inline bool useTouchOverflowScrolling() const;
#endif

    inline bool useSmoothScrolling() const;

#if ENABLE(TEXT_AUTOSIZING)
    inline TextSizeAdjustment textSizeAdjust() const;
    AutosizeStatus autosizeStatus() const;
    bool isIdempotentTextAutosizingCandidate() const;
    bool isIdempotentTextAutosizingCandidate(AutosizeStatus overrideStatus) const;
#endif

    inline TextSecurity textSecurity() const;
    inline InputSecurity inputSecurity() const;

    WritingMode writingMode() const { return static_cast<WritingMode>(m_inheritedFlags.writingMode); }
    inline bool isHorizontalWritingMode() const;
    inline bool isVerticalWritingMode() const;
    inline bool isFlippedLinesWritingMode() const;
    inline bool isFlippedBlocksWritingMode() const;

    inline ImageOrientation imageOrientation() const;
    inline ImageRendering imageRendering() const;

    inline OptionSet<SpeakAs> speakAs() const;

    inline FilterOperations& mutableFilter();
    inline const FilterOperations& filter() const;
    inline bool hasFilter() const;
    bool hasReferenceFilterOnly() const;

    inline FilterOperations& mutableAppleColorFilter();
    inline const FilterOperations& appleColorFilter() const;
    inline bool hasAppleColorFilter() const;

#if ENABLE(FILTERS_LEVEL_2)
    inline FilterOperations& mutableBackdropFilter();
    inline const FilterOperations& backdropFilter() const;
    inline bool hasBackdropFilter() const;
#else
    bool hasBackdropFilter() const { return false; };
#endif

#if ENABLE(CSS_COMPOSITING)
    inline void setBlendMode(BlendMode);
    inline bool isInSubtreeWithBlendMode() const;

    inline void setIsolation(Isolation);
#endif

    inline BlendMode blendMode() const;
    inline bool hasBlendMode() const;

    inline Isolation isolation() const;
    inline bool hasIsolation() const;

    bool shouldPlaceVerticalScrollbarOnLeft() const;

    inline bool usesLegacyScrollbarStyle() const;

#if ENABLE(APPLE_PAY)
    inline ApplePayButtonStyle applePayButtonStyle() const;
    inline ApplePayButtonType applePayButtonType() const;
#endif

    inline MathStyle mathStyle() const;

    void setDisplay(DisplayType value)
    {
        m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
        m_nonInheritedFlags.effectiveDisplay = m_nonInheritedFlags.originalDisplay;
    }
    void setEffectiveDisplay(DisplayType v) { m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(v); }
    void setPosition(PositionType v) { m_nonInheritedFlags.position = static_cast<unsigned>(v); }
    void setFloating(Float v) { m_nonInheritedFlags.floating = static_cast<unsigned>(v); }

    inline void setLeft(Length&&);
    inline void setRight(Length&&);
    inline void setTop(Length&&);
    inline void setBottom(Length&&);

    inline void setWidth(Length&&);
    inline void setHeight(Length&&);

    inline void setLogicalWidth(Length&&);
    inline void setLogicalHeight(Length&&);

    inline void setMinWidth(Length&&);
    inline void setMaxWidth(Length&&);
    inline void setMinHeight(Length&&);
    inline void setMaxHeight(Length&&);

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderImage();
    inline void resetBorderRadius();
    inline void resetBorderTopLeftRadius();
    inline void resetBorderTopRightRadius();
    inline void resetBorderBottomLeftRadius();
    inline void resetBorderBottomRightRadius();

    inline void setBackgroundColor(const StyleColor&);

    inline void setBackgroundXPosition(Length&&);
    inline void setBackgroundYPosition(Length&&);
    inline void setBackgroundSize(FillSizeType);
    inline void setBackgroundSizeLength(LengthSize&&);
    inline void setBackgroundAttachment(FillAttachment);
    inline void setBackgroundClip(FillBox);
    inline void setBackgroundOrigin(FillBox);
    inline void setBackgroundRepeat(FillRepeatXY);
    inline void setBackgroundBlendMode(BlendMode);

    inline void setBorderImage(const NinePieceImage&);
    void setBorderImageSource(RefPtr<StyleImage>&&);
    void setBorderImageSliceFill(bool);
    void setBorderImageSlices(LengthBox&&);
    void setBorderImageWidth(LengthBox&&);
    void setBorderImageWidthOverridesBorderWidths(bool);
    void setBorderImageOutset(LengthBox&&);
    void setBorderImageHorizontalRule(NinePieceImageRule);
    void setBorderImageVerticalRule(NinePieceImageRule);

    inline void setBorderTopLeftRadius(LengthSize&&);
    inline void setBorderTopRightRadius(LengthSize&&);
    inline void setBorderBottomLeftRadius(LengthSize&&);
    inline void setBorderBottomRightRadius(LengthSize&&);

    inline void setBorderRadius(LengthSize&&);
    inline void setBorderRadius(const IntSize&);
    inline void setHasExplicitlySetBorderBottomLeftRadius(bool);
    inline void setHasExplicitlySetBorderBottomRightRadius(bool);
    inline void setHasExplicitlySetBorderTopLeftRadius(bool);
    inline void setHasExplicitlySetBorderTopRightRadius(bool);

    RoundedRect getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    static RoundedRect getRoundedInnerBorderFor(const LayoutRect&, LayoutUnit topWidth, LayoutUnit bottomWidth, LayoutUnit leftWidth, LayoutUnit rightWidth, std::optional<BorderDataRadii>, bool isHorizontal, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);

    inline void setBorderLeftWidth(float);
    inline void setBorderLeftStyle(BorderStyle);
    inline void setBorderLeftColor(const StyleColor&);
    inline void setBorderRightWidth(float);
    inline void setBorderRightStyle(BorderStyle);
    inline void setBorderRightColor(const StyleColor&);
    inline void setBorderTopWidth(float);
    inline void setBorderTopStyle(BorderStyle);
    inline void setBorderTopColor(const StyleColor&);
    inline void setBorderBottomWidth(float);
    inline void setBorderBottomStyle(BorderStyle);
    inline void setBorderBottomColor(const StyleColor&);

    inline void setOutlineWidth(float);
    void setOutlineStyleIsAuto(OutlineIsAuto);
    inline void setOutlineStyle(BorderStyle);
    inline void setOutlineColor(const StyleColor&);

    void setOverflowX(Overflow v) { m_nonInheritedFlags.overflowX =  static_cast<unsigned>(v); }
    void setOverflowY(Overflow v) { m_nonInheritedFlags.overflowY = static_cast<unsigned>(v); }
    inline void setOverscrollBehaviorX(OverscrollBehavior);
    inline void setOverscrollBehaviorY(OverscrollBehavior);
    void setVisibility(Visibility v) { m_inheritedFlags.visibility = static_cast<unsigned>(v); }
    void setVerticalAlign(VerticalAlign v) { m_nonInheritedFlags.verticalAlign = static_cast<unsigned>(v); }
    void setVerticalAlignLength(Length&&);

    inline void setHasClip(bool = true);
    inline void setClipLeft(Length&&);
    inline void setClipRight(Length&&);
    inline void setClipTop(Length&&);
    inline void setClipBottom(Length&&);
    void setClip(Length&& top, Length&& right, Length&& bottom, Length&& left);
    inline void setClip(LengthBox&&);

    void setUnicodeBidi(UnicodeBidi v) { m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(v); }

    void setClear(Clear v) { m_nonInheritedFlags.clear = static_cast<unsigned>(v); }
    void setTableLayout(TableLayoutType v) { m_nonInheritedFlags.tableLayout = static_cast<unsigned>(v); }

    WEBCORE_EXPORT bool setFontDescription(FontCascadeDescription&&);

    // Only used for blending font sizes when animating, for MathML anonymous blocks, and for text autosizing.
    void setFontSize(float);
    void setFontSizeAdjust(FontSizeAdjust);

    void setFontVariationSettings(FontVariationSettings);
    void setFontWeight(FontSelectionValue);
    void setFontStretch(FontSelectionValue);
    void setFontItalic(std::optional<FontSelectionValue>);
    void setFontPalette(FontPalette);

    void setColor(const Color&);
    inline void setTextIndent(Length&&);
    void setTextAlign(TextAlignMode v) { m_inheritedFlags.textAlign = static_cast<unsigned>(v); }
    inline void setTextAlignLast(TextAlignLast);
    inline void setTextGroupAlign(TextGroupAlign);
    inline void setTextTransform(OptionSet<TextTransform>);
    inline void addToTextDecorationsInEffect(OptionSet<TextDecorationLine>);
    inline void setTextDecorationsInEffect(OptionSet<TextDecorationLine>);
    inline void setTextDecorationLine(OptionSet<TextDecorationLine>);
    inline void setTextDecorationStyle(TextDecorationStyle);
    inline void setTextDecorationSkipInk(TextDecorationSkipInk);
    inline void setTextUnderlinePosition(TextUnderlinePosition);
    inline void setTextUnderlineOffset(TextUnderlineOffset);
    inline void setTextDecorationThickness(TextDecorationThickness);
    void setDirection(TextDirection v) { m_inheritedFlags.direction = static_cast<unsigned>(v); }
    void setHasExplicitlySetDirection(bool v) { m_nonInheritedFlags.hasExplicitlySetDirection = v; }
    void setLineHeight(Length&&);
    bool setZoom(float);
    void setZoomWithoutReturnValue(float f) { setZoom(f); }
    inline bool setEffectiveZoom(float);
    inline void setTextZoom(TextZoom);

    void setTextIndentLine(TextIndentLine);
    void setTextIndentType(TextIndentType);
    inline void setTextJustify(TextJustify);

    inline void setTextBoxTrim(TextBoxTrim);
    void setTextBoxEdge(TextBoxEdge);

    inline void setMarginTrim(OptionSet<MarginTrimType>);

#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(Length&&);
#endif

    inline void setImageOrientation(ImageOrientation);
    inline void setImageRendering(ImageRendering);

    void setWhiteSpaceCollapse(WhiteSpaceCollapse v) { m_inheritedFlags.whiteSpaceCollapse = static_cast<unsigned>(v); }
    void setTextWrap(TextWrap v) { m_inheritedFlags.textWrap = static_cast<unsigned>(v); }

    void setWordSpacing(Length&&);

    // If letter-spacing is nonzero, we disable ligatures, which means this property affects font preparation.
    void setLetterSpacing(float);
    void setLetterSpacingWithoutUpdatingFontDescription(float);

    inline void clearBackgroundLayers();
    inline void inheritBackgroundLayers(const FillLayer& parent);

    void adjustBackgroundLayers();

    inline void clearMaskLayers();
    inline void inheritMaskLayers(const FillLayer& parent);

    inline void adjustMaskLayers();

    inline void setMaskImage(RefPtr<StyleImage>&&);

    inline void setMaskBoxImage(const NinePieceImage&);
    inline void setMaskBoxImageSource(RefPtr<StyleImage>&&);
    inline void setMaskXPosition(Length&&);
    inline void setMaskYPosition(Length&&);
    inline void setMaskRepeat(FillRepeatXY);

    inline void setMaskSize(LengthSize);

    void setBorderCollapse(BorderCollapse collapse) { m_inheritedFlags.borderCollapse = static_cast<unsigned>(collapse); }
    void setHorizontalBorderSpacing(float);
    void setVerticalBorderSpacing(float);
    void setEmptyCells(EmptyCell v) { m_inheritedFlags.emptyCells = static_cast<unsigned>(v); }
    void setCaptionSide(CaptionSide v) { m_inheritedFlags.captionSide = static_cast<unsigned>(v); }

    inline void setAspectRatioType(AspectRatioType);
    inline void setAspectRatio(double width, double height);

    inline void setContain(OptionSet<Containment>);
    inline void setContainerType(ContainerType);
    inline void setContainerNames(const Vector<AtomString>&);

    inline void setContainIntrinsicWidthType(ContainIntrinsicSizeType);
    inline void setContainIntrinsicHeightType(ContainIntrinsicSizeType);
    inline void setContainIntrinsicWidth(std::optional<Length>);
    inline void setContainIntrinsicHeight(std::optional<Length>);

    inline void setContentVisibility(ContentVisibility);

    inline void setEffectiveSkipsContent(bool);

    inline void setListStyleType(ListStyleType);
    void setListStyleImage(RefPtr<StyleImage>&&);
    void setListStylePosition(ListStylePosition v) { m_inheritedFlags.listStylePosition = static_cast<unsigned>(v); }
    inline void resetMargin();
    inline void setMarginTop(Length&&);
    inline void setMarginBottom(Length&&);
    inline void setMarginLeft(Length&&);
    inline void setMarginRight(Length&&);
    void setMarginStart(Length&&);
    void setMarginEnd(Length&&);

    inline void resetPadding();
    inline void setPaddingBox(LengthBox&&);
    inline void setPaddingTop(Length&&);
    inline void setPaddingBottom(Length&&);
    inline void setPaddingLeft(Length&&);
    inline void setPaddingRight(Length&&);

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

    inline int specifiedZIndex() const;
    inline bool hasAutoSpecifiedZIndex() const;
    inline void setSpecifiedZIndex(int);
    inline void setHasAutoSpecifiedZIndex();

    inline int usedZIndex() const;
    inline bool hasAutoUsedZIndex() const;
    inline void setUsedZIndex(int);
    inline void setHasAutoUsedZIndex();

    inline void setHasAutoWidows();
    inline void setWidows(unsigned short);

    inline void setHasAutoOrphans();
    inline void setOrphans(unsigned short);

    inline void setOutlineOffset(float);
    void setTextShadow(std::unique_ptr<ShadowData>, bool add = false);
    inline void setTextStrokeColor(const StyleColor&);
    inline void setTextStrokeWidth(float);
    inline void setTextFillColor(const StyleColor&);
    inline void setCaretColor(const StyleColor&);
    inline void setHasAutoCaretColor();
    inline void setAccentColor(const StyleColor&);
    inline void setHasAutoAccentColor();
    inline void setOpacity(float);
    inline void setAppearance(StyleAppearance);
    inline void setEffectiveAppearance(StyleAppearance);
    inline void setBoxAlign(BoxAlignment);
    void setBoxDirection(BoxDirection d) { m_inheritedFlags.boxDirection = static_cast<unsigned>(d); }
    inline void setBoxFlex(float);
    inline void setBoxFlexGroup(unsigned);
    inline void setBoxLines(BoxLines);
    inline void setBoxOrdinalGroup(unsigned);
    inline void setBoxOrient(BoxOrient);
    inline void setBoxPack(BoxPack);
    void setBoxShadow(std::unique_ptr<ShadowData>, bool add = false);
    inline void setBoxReflect(RefPtr<StyleReflection>&&);
    inline void setBoxSizing(BoxSizing);
    inline void setFlexGrow(float);
    inline void setFlexShrink(float);
    inline void setFlexBasis(Length&&);
    inline void setOrder(int);
    inline void setAlignContent(const StyleContentAlignmentData&);
    inline void setAlignItems(const StyleSelfAlignmentData&);
    inline void setAlignItemsPosition(ItemPosition);
    inline void setAlignSelf(const StyleSelfAlignmentData&);
    inline void setAlignSelfPosition(ItemPosition);
    inline void setFlexDirection(FlexDirection);
    inline void setFlexWrap(FlexWrap);
    inline void setJustifyContent(const StyleContentAlignmentData&);
    inline void setJustifyContentPosition(ContentPosition);
    inline void setJustifyItems(const StyleSelfAlignmentData&);
    inline void setJustifySelf(const StyleSelfAlignmentData&);
    inline void setJustifySelfPosition(ItemPosition);

#if ENABLE(CSS_BOX_DECORATION_BREAK)
    inline void setBoxDecorationBreak(BoxDecorationBreak);
#endif

    inline void setGridColumnList(const GridTrackList&);
    inline void setGridRowList(const GridTrackList&);
    inline void setGridAutoColumns(const Vector<GridTrackSize>&);
    inline void setGridAutoRows(const Vector<GridTrackSize>&);
    inline void setImplicitNamedGridColumnLines(const NamedGridLinesMap&);
    inline void setImplicitNamedGridRowLines(const NamedGridLinesMap&);
    inline void setNamedGridArea(const NamedGridAreaMap&);
    inline void setNamedGridAreaRowCount(size_t);
    inline void setNamedGridAreaColumnCount(size_t);
    inline void setGridAutoFlow(GridAutoFlow);
    inline void setGridItemColumnStart(const GridPosition&);
    inline void setGridItemColumnEnd(const GridPosition&);
    inline void setGridItemRowStart(const GridPosition&);
    inline void setGridItemRowEnd(const GridPosition&);

    inline void setAlignTracks(Vector<StyleContentAlignmentData>);
    inline void setJustifyTracks(Vector<StyleContentAlignmentData>);
    inline void setMasonryAutoFlow(MasonryAutoFlow);

    inline void setMarqueeIncrement(Length&&);
    inline void setMarqueeSpeed(int);
    inline void setMarqueeDirection(MarqueeDirection);
    inline void setMarqueeBehavior(MarqueeBehavior);
    inline void setMarqueeLoopCount(int);
    inline void setUserModify(UserModify);
    inline void setUserDrag(UserDrag);
    inline void setUserSelect(UserSelect);
    inline void setTextOverflow(TextOverflow);
    inline void setWordBreak(WordBreak);
    inline void setOverflowWrap(OverflowWrap);
    inline void setNBSPMode(NBSPMode);
    inline void setLineBreak(LineBreak);
    inline void setHyphens(Hyphens);
    inline void setHyphenationLimitBefore(short);
    inline void setHyphenationLimitAfter(short);
    inline void setHyphenationLimitLines(short);
    inline void setHyphenationString(const AtomString&);
    inline void setResize(Resize);
    inline void setColumnAxis(ColumnAxis);
    inline void setColumnProgression(ColumnProgression);
    inline void setColumnWidth(float);
    inline void setHasAutoColumnWidth();
    inline void setColumnCount(unsigned short);
    inline void setHasAutoColumnCount();
    inline void setColumnFill(ColumnFill);
    inline void setColumnGap(GapLength&&);
    inline void setRowGap(GapLength&&);
    inline void setColumnRuleColor(const StyleColor&);
    inline void setColumnRuleStyle(BorderStyle);
    inline void setColumnRuleWidth(unsigned short);
    inline void resetColumnRule();
    inline void setColumnSpan(ColumnSpan);
    inline void setWordBoundaryDetection(const WordBoundaryDetection&);
    inline void inheritColumnPropertiesFrom(const RenderStyle& parent);

    inline void setTransform(const TransformOperations&);
    inline void setTransformOriginX(Length&&);
    inline void setTransformOriginY(Length&&);
    inline void setTransformOriginZ(float);
    inline void setTransformBox(TransformBox);

    void setRotate(RefPtr<RotateTransformOperation>&&);
    void setScale(RefPtr<ScaleTransformOperation>&&);
    void setTranslate(RefPtr<TranslateTransformOperation>&&);

    inline void setSpeakAs(OptionSet<SpeakAs>);
    inline void setTextCombine(TextCombine);
    inline void setTextDecorationColor(const StyleColor&);
    inline void setTextEmphasisColor(const StyleColor&);
    inline void setTextEmphasisFill(TextEmphasisFill);
    inline void setTextEmphasisMark(TextEmphasisMark);
    inline void setTextEmphasisCustomMark(const AtomString&);
    inline void setTextEmphasisPosition(OptionSet<TextEmphasisPosition>);
    bool setTextOrientation(TextOrientation);

    inline void setObjectFit(ObjectFit);
    inline void setObjectPosition(LengthPoint);

    inline void setRubyPosition(RubyPosition);

#if ENABLE(DARK_MODE_CSS)
    inline void setColorScheme(StyleColorScheme);
#endif

    inline void setFilter(const FilterOperations&);
    inline void setAppleColorFilter(const FilterOperations&);

#if ENABLE(FILTERS_LEVEL_2)
    inline void setBackdropFilter(const FilterOperations&);
#endif

    inline void setTabSize(const TabSize&);

    inline void setBreakBefore(BreakBetween);
    inline void setBreakAfter(BreakBetween);
    inline void setBreakInside(BreakInside);
    
    inline void setHangingPunctuation(OptionSet<HangingPunctuation>);

    inline void setLineGrid(const AtomString&);
    inline void setLineSnap(LineSnap);
    inline void setLineAlign(LineAlign);

    void setPointerEvents(PointerEvents p) { m_inheritedFlags.pointerEvents = static_cast<unsigned>(p); }

    inline void clearAnimations();
    inline void clearTransitions();

    void adjustAnimations();
    void adjustTransitions();

    inline void setTransformStyle3D(TransformStyle3D);
    inline void setTransformStyleForcedToFlat(bool);
    inline void setBackfaceVisibility(BackfaceVisibility);
    inline void setPerspective(float);
    inline void setPerspectiveOriginX(Length&&);
    inline void setPerspectiveOriginY(Length&&);
    inline void setPageSize(LengthSize);
    inline void setPageSizeType(PageSizeType);
    inline void resetPageSizeType();

    inline void setLineBoxContain(OptionSet<LineBoxContain>);
    inline void setLineClamp(LineClampValue);
    
    inline void setInitialLetter(const IntSize&);
    
    inline void setTouchActions(OptionSet<TouchAction>);
    inline void setEffectiveTouchActions(OptionSet<TouchAction>);
    inline void setEventListenerRegionTypes(OptionSet<EventListenerRegionType>);

    inline void setEffectiveInert(bool);

    void setScrollMarginTop(Length&&);
    void setScrollMarginBottom(Length&&);
    void setScrollMarginLeft(Length&&);
    void setScrollMarginRight(Length&&);

    void setScrollPaddingTop(Length&&);
    void setScrollPaddingBottom(Length&&);
    void setScrollPaddingLeft(Length&&);
    void setScrollPaddingRight(Length&&);

    void setScrollSnapType(ScrollSnapType);
    void setScrollSnapAlign(const ScrollSnapAlign&);
    void setScrollSnapStop(ScrollSnapStop);

    inline void setScrollbarColor(const std::optional<ScrollbarColor>&);
    inline void setScrollbarThumbColor(const StyleColor&);
    inline void setScrollbarTrackColor(const StyleColor&);
    void setScrollbarGutter(ScrollbarGutter);
    void setScrollbarWidth(ScrollbarWidth);

#if ENABLE(TOUCH_EVENTS)
    inline void setTapHighlightColor(const StyleColor&);
#endif

#if PLATFORM(IOS_FAMILY)
    inline void setTouchCalloutEnabled(bool);
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    inline void setUseTouchOverflowScrolling(bool);
#endif

    inline void setUseSmoothScrolling(bool);

#if ENABLE(TEXT_AUTOSIZING)
    inline void setTextSizeAdjust(TextSizeAdjustment);
    void setAutosizeStatus(AutosizeStatus);
#endif

    inline void setTextSecurity(TextSecurity);
    inline void setInputSecurity(InputSecurity);

#if ENABLE(APPLE_PAY)
    inline void setApplePayButtonStyle(ApplePayButtonStyle);
    inline void setApplePayButtonType(ApplePayButtonType);
#endif

#if ENABLE(CSS_PAINTING_API)
    void addCustomPaintWatchProperty(const AtomString&);
#endif

    // Support for paint-order, stroke-linecap, stroke-linejoin, and stroke-miterlimit from https://drafts.fxtf.org/paint/.
    inline void setPaintOrder(PaintOrder);
    inline PaintOrder paintOrder() const;
    static constexpr PaintOrder initialPaintOrder();
    static std::span<const PaintType, 3> paintTypesForPaintOrder(PaintOrder);
    
    inline void setCapStyle(LineCap);
    inline LineCap capStyle() const;
    static constexpr LineCap initialCapStyle();
    
    inline void setJoinStyle(LineJoin);
    inline LineJoin joinStyle() const;
    static constexpr LineJoin initialJoinStyle();
    
    inline const Length& strokeWidth() const;
    inline void setStrokeWidth(Length&&);
    inline bool hasVisibleStroke() const;
    static inline Length initialStrokeWidth();

    float computedStrokeWidth(const IntSize& viewportSize) const;
    inline void setHasExplicitlySetStrokeWidth(bool);
    inline bool hasExplicitlySetStrokeWidth() const;
    bool hasPositiveStrokeWidth() const;
    
    inline StyleColor strokeColor() const;
    inline void setStrokeColor(const StyleColor&);
    inline void setVisitedLinkStrokeColor(const StyleColor&);
    inline const StyleColor& visitedLinkStrokeColor() const;
    inline void setHasExplicitlySetStrokeColor(bool);
    inline bool hasExplicitlySetStrokeColor() const;
    static inline StyleColor initialStrokeColor();
    Color computedStrokeColor() const;
    inline CSSPropertyID effectiveStrokeColorProperty() const;

    inline float strokeMiterLimit() const;
    inline void setStrokeMiterLimit(float);
    static constexpr float initialStrokeMiterLimit();

    const SVGRenderStyle& svgStyle() const { return m_svgStyle; }
    inline SVGRenderStyle& accessSVGStyle();

    inline SVGPaintType fillPaintType() const;
    inline StyleColor fillPaintColor() const;
    inline void setFillPaintColor(const StyleColor&);
    inline float fillOpacity() const;
    inline void setFillOpacity(float);

    inline SVGPaintType strokePaintType() const;
    inline StyleColor strokePaintColor() const;
    inline void setStrokePaintColor(const StyleColor&);
    inline float strokeOpacity() const;
    inline void setStrokeOpacity(float);
    inline Vector<SVGLengthValue> strokeDashArray() const;
    inline void setStrokeDashArray(Vector<SVGLengthValue>);
    inline const Length& strokeDashOffset() const;
    inline void setStrokeDashOffset(Length&&);

    inline const Length& cx() const;
    inline void setCx(Length&&);
    inline const Length& cy() const;
    inline void setCy(Length&&);
    inline const Length& r() const;
    inline void setR(Length&&);
    inline const Length& rx() const;
    inline void setRx(Length&&);
    inline const Length& ry() const;
    inline void setRy(Length&&);
    inline const Length& x() const;
    inline void setX(Length&&);
    inline const Length& y() const;
    inline void setY(Length&&);

    inline float floodOpacity() const;
    inline void setFloodOpacity(float);

    inline float stopOpacity() const;
    inline void setStopOpacity(float);

    inline void setStopColor(const StyleColor&);
    inline void setFloodColor(const StyleColor&);
    inline void setLightingColor(const StyleColor&);

    inline SVGLengthValue baselineShiftValue() const;
    inline void setBaselineShiftValue(SVGLengthValue);
    inline SVGLengthValue kerning() const;
    inline void setKerning(SVGLengthValue);

    inline void setShapeOutside(RefPtr<ShapeValue>&&);
    inline ShapeValue* shapeOutside() const;
    static ShapeValue* initialShapeOutside() { return nullptr; }

    inline const Length& shapeMargin() const;
    inline void setShapeMargin(Length&&);
    static inline Length initialShapeMargin();

    inline float shapeImageThreshold() const;
    void setShapeImageThreshold(float);
    static float initialShapeImageThreshold() { return 0; }

    inline void setClipPath(RefPtr<PathOperation>&&);
    inline PathOperation* clipPath() const;
    static PathOperation* initialClipPath() { return nullptr; }

    inline bool hasEffectiveContentNone() const;
    inline bool hasContent() const;
    inline const ContentData* contentData() const;
    void setContent(std::unique_ptr<ContentData>, bool add);
    inline bool contentDataEquivalent(const RenderStyle*) const;
    void clearContent();
    inline void setHasContentNone(bool);
    void setContent(const String&, bool add = false);
    void setContent(RefPtr<StyleImage>&&, bool add = false);
    void setContent(std::unique_ptr<CounterContent>, bool add = false);
    void setContent(QuoteType, bool add = false);
    void setContentAltText(const String&);
    const String& contentAltText() const;
    inline bool hasAttrContent() const;
    void setHasAttrContent();

    const CounterDirectiveMap& counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();

    inline QuotesData* quotes() const;
    void setQuotes(RefPtr<QuotesData>&&);

    inline WillChangeData* willChange() const;
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

    constexpr bool isDisplayInlineType() const;
    constexpr bool isOriginalDisplayInlineType() const;
    constexpr bool isDisplayFlexibleOrGridBox() const;
    constexpr bool isDisplayFlexibleBoxIncludingDeprecatedOrGridBox() const;
    constexpr bool isDisplayRegionType() const;
    constexpr bool isDisplayBlockLevel() const;
    constexpr bool isOriginalDisplayBlockType() const;
    constexpr bool isDisplayTableOrTablePart() const;
    constexpr bool isOriginalDisplayListItemType() const;

    inline bool setWritingMode(WritingMode);

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

    // Resolves the currentColor keyword, but must not be used for the "color" property which has a different semantic.
    WEBCORE_EXPORT Color colorResolvingCurrentColor(const StyleColor&) const;

    WEBCORE_EXPORT Color visitedDependentColor(CSSPropertyID) const;
    WEBCORE_EXPORT Color visitedDependentColorWithColorFilter(CSSPropertyID) const;

    WEBCORE_EXPORT Color colorByApplyingColorFilter(const Color&) const;
    WEBCORE_EXPORT Color colorWithColorFilter(const StyleColor&) const;

    void setHasExplicitlyInheritedProperties() { m_nonInheritedFlags.hasExplicitlyInheritedProperties = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedFlags.hasExplicitlyInheritedProperties; }

    bool disallowsFastPathInheritance() const { return m_nonInheritedFlags.disallowsFastPathInheritance; }
    void setDisallowsFastPathInheritance() { m_nonInheritedFlags.disallowsFastPathInheritance = true; }

    inline void setMathStyle(const MathStyle&);

    inline void setTextSpacingTrim(TextSpacingTrim);
    inline void setTextAutospace(TextAutospace);

    static constexpr Overflow initialOverflowX();
    static constexpr Overflow initialOverflowY();
    static constexpr OverscrollBehavior initialOverscrollBehaviorX();
    static constexpr OverscrollBehavior initialOverscrollBehaviorY();

    static constexpr Clear initialClear();
    static constexpr DisplayType initialDisplay();
    static constexpr UnicodeBidi initialUnicodeBidi();
    static constexpr PositionType initialPosition();
    static constexpr VerticalAlign initialVerticalAlign();
    static constexpr Float initialFloating();
    static constexpr BreakBetween initialBreakBetween();
    static constexpr BreakInside initialBreakInside();
    static constexpr OptionSet<HangingPunctuation> initialHangingPunctuation();
    static constexpr TableLayoutType initialTableLayout();
    static constexpr BorderCollapse initialBorderCollapse();
    static constexpr BorderStyle initialBorderStyle();
    static constexpr OutlineIsAuto initialOutlineStyleIsAuto();
    static inline LengthSize initialBorderRadius();
    static constexpr CaptionSide initialCaptionSide();
    static constexpr ColumnAxis initialColumnAxis();
    static constexpr ColumnProgression initialColumnProgression();
    static constexpr TextDirection initialDirection();
    static constexpr WritingMode initialWritingMode();
    static constexpr TextCombine initialTextCombine();
    static constexpr TextOrientation initialTextOrientation();
    static constexpr TextSpacingTrim initialTextSpacingTrim();
    static constexpr TextAutospace initialTextAutospace();
    static constexpr ObjectFit initialObjectFit();
    static inline LengthPoint initialObjectPosition();
    static constexpr EmptyCell initialEmptyCells();
    static constexpr ListStylePosition initialListStylePosition();
    static inline ListStyleType initialListStyleType();
    static constexpr OptionSet<TextTransform> initialTextTransform();
    static constexpr Visibility initialVisibility();
    static constexpr WhiteSpace initialWhiteSpace();
    static constexpr WhiteSpaceCollapse initialWhiteSpaceCollapse();
    static float initialHorizontalBorderSpacing() { return 0; }
    static float initialVerticalBorderSpacing() { return 0; }
    static constexpr CursorType initialCursor();
    static inline Color initialColor();
    static inline StyleColor initialTextStrokeColor();
    static inline StyleColor initialTextDecorationColor();
    static StyleImage* initialListStyleImage() { return 0; }
    static float initialBorderWidth() { return 3; }
    static unsigned short initialColumnRuleWidth() { return 3; }
    static float initialOutlineWidth() { return 3; }
    static float initialLetterSpacing() { return 0; }
    static inline Length initialWordSpacing();
    static inline Length initialSize();
    static inline Length initialMinSize();
    static inline Length initialMaxSize();
    static inline Length initialOffset();
    static inline Length initialRadius();
    static inline Length initialMargin();
    static constexpr OptionSet<MarginTrimType> initialMarginTrim();
    static inline Length initialPadding();
    static inline Length initialTextIndent();
    static constexpr TextBoxTrim initialTextBoxTrim();
    static TextBoxEdge initialTextBoxEdge();
    static constexpr LengthType zeroLength();
    static inline Length oneLength();
    static unsigned short initialWidows() { return 2; }
    static unsigned short initialOrphans() { return 2; }
    // Returning -100% percent here means the line-height is not set.
    static inline Length initialLineHeight();
    static constexpr TextAlignMode initialTextAlign();
    static constexpr TextAlignLast initialTextAlignLast();
    static constexpr TextGroupAlign initialTextGroupAlign();
    static constexpr OptionSet<TextDecorationLine> initialTextDecorationLine();
    static constexpr TextDecorationStyle initialTextDecorationStyle();
    static constexpr TextDecorationSkipInk initialTextDecorationSkipInk();
    static constexpr TextUnderlinePosition initialTextUnderlinePosition();
    static constexpr TextUnderlineOffset initialTextUnderlineOffset();
    static constexpr TextDecorationThickness initialTextDecorationThickness();
    static float initialZoom() { return 1.0f; }
    static constexpr TextZoom initialTextZoom();
    static float initialOutlineOffset() { return 0; }
    static float initialOpacity() { return 1.0f; }
    static constexpr BoxAlignment initialBoxAlign();
    static constexpr BoxDecorationBreak initialBoxDecorationBreak();
    static constexpr BoxDirection initialBoxDirection();
    static constexpr BoxLines initialBoxLines();
    static constexpr BoxOrient initialBoxOrient();
    static constexpr BoxPack initialBoxPack();
    static float initialBoxFlex() { return 0.0f; }
    static unsigned initialBoxFlexGroup() { return 1; }
    static unsigned initialBoxOrdinalGroup() { return 1; }
    static constexpr BoxSizing initialBoxSizing();
    static StyleReflection* initialBoxReflect() { return 0; }
    static float initialFlexGrow() { return 0; }
    static float initialFlexShrink() { return 1; }
    static inline Length initialFlexBasis();
    static int initialOrder() { return 0; }
    static constexpr StyleSelfAlignmentData initialJustifyItems();
    static constexpr StyleSelfAlignmentData initialSelfAlignment();
    static constexpr StyleSelfAlignmentData initialDefaultAlignment();
    static constexpr StyleContentAlignmentData initialContentAlignment();
    static constexpr FlexDirection initialFlexDirection();
    static constexpr FlexWrap initialFlexWrap();
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static inline Length initialMarqueeIncrement();
    static constexpr MarqueeBehavior initialMarqueeBehavior();
    static constexpr MarqueeDirection initialMarqueeDirection();
    static constexpr UserModify initialUserModify();
    static constexpr UserDrag initialUserDrag();
    static constexpr UserSelect initialUserSelect();
    static constexpr TextOverflow initialTextOverflow();
    static constexpr TextWrap initialTextWrap();
    static constexpr WordBreak initialWordBreak();
    static constexpr OverflowWrap initialOverflowWrap();
    static constexpr NBSPMode initialNBSPMode();
    static constexpr LineBreak initialLineBreak();
    static constexpr OptionSet<SpeakAs> initialSpeakAs();
    static constexpr Hyphens initialHyphens();
    static short initialHyphenationLimitBefore() { return -1; }
    static short initialHyphenationLimitAfter() { return -1; }
    static short initialHyphenationLimitLines() { return -1; }
    static const AtomString& initialHyphenationString();
    static constexpr Resize initialResize();
    static constexpr StyleAppearance initialAppearance();
    static constexpr AspectRatioType initialAspectRatioType();
    static constexpr OptionSet<Containment> initialContainment();
    static constexpr OptionSet<Containment> strictContainment();
    static constexpr OptionSet<Containment> contentContainment();
    static constexpr ContainerType initialContainerType();
    static constexpr ContentVisibility initialContentVisibility();
    static Vector<AtomString> initialContainerNames();
    static double initialAspectRatioWidth() { return 1.0; }
    static double initialAspectRatioHeight() { return 1.0; }
    static WordBoundaryDetection initialWordBoundaryDetection();

    static constexpr ContainIntrinsicSizeType initialContainIntrinsicWidthType();
    static constexpr ContainIntrinsicSizeType initialContainIntrinsicHeightType();
    static inline std::optional<Length> initialContainIntrinsicWidth();
    static inline std::optional<Length> initialContainIntrinsicHeight();

    static constexpr Order initialRTLOrdering();
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static constexpr ColumnFill initialColumnFill();
    static constexpr ColumnSpan initialColumnSpan();
    static inline GapLength initialColumnGap();
    static inline GapLength initialRowGap();
    static inline TransformOperations initialTransform();
    static inline Length initialTransformOriginX();
    static inline Length initialTransformOriginY();
    static constexpr TransformBox initialTransformBox();
    static RotateTransformOperation* initialRotate() { return nullptr; }
    static ScaleTransformOperation* initialScale() { return nullptr; }
    static TranslateTransformOperation* initialTranslate() { return nullptr; }
    static constexpr PointerEvents initialPointerEvents();
    static float initialTransformOriginZ() { return 0; }
    static constexpr TransformStyle3D initialTransformStyle3D();
    static constexpr BackfaceVisibility initialBackfaceVisibility();
    static float initialPerspective() { return -1; }
    static inline Length initialPerspectiveOriginX();
    static inline Length initialPerspectiveOriginY();
    static inline StyleColor initialBackgroundColor();
    static inline StyleColor initialTextEmphasisColor();
    static constexpr TextEmphasisFill initialTextEmphasisFill();
    static constexpr TextEmphasisMark initialTextEmphasisMark();
    static inline const AtomString& initialTextEmphasisCustomMark();
    static constexpr OptionSet<TextEmphasisPosition> initialTextEmphasisPosition();
    static constexpr RubyPosition initialRubyPosition();
    static constexpr OptionSet<LineBoxContain> initialLineBoxContain();
    static constexpr ImageOrientation initialImageOrientation();
    static constexpr ImageRendering initialImageRendering();
    static constexpr ImageResolutionSource initialImageResolutionSource();
    static constexpr ImageResolutionSnap initialImageResolutionSnap();
    static float initialImageResolution() { return 1; }
    static StyleImage* initialBorderImageSource() { return nullptr; }
    static StyleImage* initialMaskBoxImageSource() { return nullptr; }
    static constexpr PrintColorAdjust initialPrintColorAdjust();
    static QuotesData* initialQuotes() { return nullptr; }
    static inline const AtomString& initialContentAltText();

#if ENABLE(DARK_MODE_CSS)
    static constexpr StyleColorScheme initialColorScheme();
#endif

    static constexpr TextIndentLine initialTextIndentLine();
    static constexpr TextIndentType initialTextIndentType();
    static constexpr TextJustify initialTextJustify();

#if ENABLE(CURSOR_VISIBILITY)
    static constexpr CursorVisibility initialCursorVisibility();
#endif

#if ENABLE(TEXT_AUTOSIZING)
    static inline Length initialSpecifiedLineHeight();
    static constexpr TextSizeAdjustment initialTextSizeAdjust();
#endif

    static WillChangeData* initialWillChange() { return nullptr; }

    static constexpr TouchAction initialTouchActions();

    static inline Length initialScrollMargin();
    static inline Length initialScrollPadding();

    static ScrollSnapType initialScrollSnapType();
    static ScrollSnapAlign initialScrollSnapAlign();
    static ScrollSnapStop initialScrollSnapStop();

    static inline std::optional<ScrollbarColor> initialScrollbarColor();
    static ScrollbarGutter initialScrollbarGutter();
    static ScrollbarWidth initialScrollbarWidth();

#if ENABLE(APPLE_PAY)
    static constexpr ApplePayButtonStyle initialApplePayButtonStyle();
    static constexpr ApplePayButtonType initialApplePayButtonType();
#endif

    static inline Vector<GridTrackSize> initialGridColumnTrackSizes();
    static inline Vector<GridTrackSize> initialGridRowTrackSizes();

    static inline Vector<GridTrackSize> initialGridAutoRepeatTracks();
    static unsigned initialGridAutoRepeatInsertionPoint() { return 0; }
    static constexpr AutoRepeatType initialGridAutoRepeatType();

    static constexpr GridAutoFlow initialGridAutoFlow();
    static inline Vector<StyleContentAlignmentData> initialAlignTracks();
    static inline Vector<StyleContentAlignmentData> initialJustifyTracks();
    static constexpr MasonryAutoFlow initialMasonryAutoFlow();

    static inline Vector<GridTrackSize> initialGridAutoColumns();
    static inline Vector<GridTrackSize> initialGridAutoRows();

    static NamedGridAreaMap initialNamedGridArea();
    static size_t initialNamedGridAreaCount() { return 0; }

    static NamedGridLinesMap initialNamedGridColumnLines();
    static NamedGridLinesMap initialNamedGridRowLines();

    static OrderedNamedGridLinesMap initialOrderedNamedGridColumnLines();
    static OrderedNamedGridLinesMap initialOrderedNamedGridRowLines();

    static inline GridPosition initialGridItemColumnStart();
    static inline GridPosition initialGridItemColumnEnd();
    static inline GridPosition initialGridItemRowStart();
    static inline GridPosition initialGridItemRowEnd();

    static constexpr TabSize initialTabSize();

    static inline const AtomString& initialLineGrid();
    static constexpr LineSnap initialLineSnap();
    static constexpr LineAlign initialLineAlign();

    static constexpr IntSize initialInitialLetter();
    static constexpr LineClampValue initialLineClamp();
    static constexpr TextSecurity initialTextSecurity();
    static constexpr InputSecurity initialInputSecurity();

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

    static inline FilterOperations initialFilter();
    static inline FilterOperations initialAppleColorFilter();

#if ENABLE(FILTERS_LEVEL_2)
    static inline FilterOperations initialBackdropFilter();
#endif

#if ENABLE(CSS_COMPOSITING)
    static constexpr BlendMode initialBlendMode();
    static constexpr Isolation initialIsolation();
#endif

    static constexpr MathStyle initialMathStyle();

    void setVisitedLinkColor(const Color&);
    inline void setVisitedLinkBackgroundColor(const StyleColor&);
    inline void setVisitedLinkBorderLeftColor(const StyleColor&);
    inline void setVisitedLinkBorderRightColor(const StyleColor&);
    inline void setVisitedLinkBorderBottomColor(const StyleColor&);
    inline void setVisitedLinkBorderTopColor(const StyleColor&);
    inline void setVisitedLinkOutlineColor(const StyleColor&);
    inline void setVisitedLinkColumnRuleColor(const StyleColor&);
    inline void setVisitedLinkTextDecorationColor(const StyleColor&);
    inline void setVisitedLinkTextEmphasisColor(const StyleColor&);
    inline void setVisitedLinkTextFillColor(const StyleColor&);
    inline void setVisitedLinkTextStrokeColor(const StyleColor&);
    inline void setVisitedLinkCaretColor(const StyleColor&);
    inline void setHasVisitedLinkAutoCaretColor();

    void inheritUnicodeBidiFrom(const RenderStyle* parent) { m_nonInheritedFlags.unicodeBidi = parent->m_nonInheritedFlags.unicodeBidi; }

    inline void getShadowInlineDirectionExtent(const ShadowData*, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const;
    inline void getShadowBlockDirectionExtent(const ShadowData*, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const;

    inline const StyleColor& borderLeftColor() const;
    inline const StyleColor& borderRightColor() const;
    inline const StyleColor& borderTopColor() const;
    inline const StyleColor& borderBottomColor() const;
    inline const StyleColor& backgroundColor() const;
    WEBCORE_EXPORT const Color& color() const;
    inline const StyleColor& columnRuleColor() const;
    inline const StyleColor& outlineColor() const;
    inline const StyleColor& textEmphasisColor() const;
    inline const StyleColor& textFillColor() const;
    static inline StyleColor initialTextFillColor();
    inline const StyleColor& textStrokeColor() const;
    inline const StyleColor& caretColor() const;
    inline bool hasAutoCaretColor() const;
    const Color& visitedLinkColor() const;
    inline const StyleColor& visitedLinkBackgroundColor() const;
    inline const StyleColor& visitedLinkBorderLeftColor() const;
    inline const StyleColor& visitedLinkBorderRightColor() const;
    inline const StyleColor& visitedLinkBorderBottomColor() const;
    inline const StyleColor& visitedLinkBorderTopColor() const;
    inline const StyleColor& visitedLinkOutlineColor() const;
    inline const StyleColor& visitedLinkColumnRuleColor() const;
    inline const StyleColor& textDecorationColor() const;
    inline const StyleColor& visitedLinkTextDecorationColor() const;
    inline const StyleColor& visitedLinkTextEmphasisColor() const;
    inline const StyleColor& visitedLinkTextFillColor() const;
    inline const StyleColor& visitedLinkTextStrokeColor() const;
    inline const StyleColor& visitedLinkCaretColor() const;
    inline bool hasVisitedLinkAutoCaretColor() const;

    inline const StyleColor& stopColor() const;
    inline const StyleColor& floodColor() const;
    inline const StyleColor& lightingColor() const;

    Color effectiveAccentColor() const;
    inline const StyleColor& accentColor() const;
    inline bool hasAutoAccentColor() const;

    inline PathOperation* offsetPath() const;
    inline void setOffsetPath(RefPtr<PathOperation>&&);
    static PathOperation* initialOffsetPath() { return nullptr; }

    inline const Length& offsetDistance() const;
    inline void setOffsetDistance(Length&&);
    static inline Length initialOffsetDistance();

    inline const LengthPoint& offsetPosition() const;
    inline void setOffsetPosition(LengthPoint);
    static inline LengthPoint initialOffsetPosition();

    inline const LengthPoint& offsetAnchor() const;
    inline void setOffsetAnchor(LengthPoint);
    static inline LengthPoint initialOffsetAnchor();

    inline OffsetRotation offsetRotate() const;
    inline void setOffsetRotate(OffsetRotation&&);
    static constexpr OffsetRotation initialOffsetRotate();

    bool borderAndBackgroundEqual(const RenderStyle&) const;
    
    inline OverflowAnchor overflowAnchor() const;
    inline void setOverflowAnchor(OverflowAnchor);
    static constexpr OverflowAnchor initialOverflowAnchor();

    static inline std::optional<Length> initialBlockStepSize();
    inline std::optional<Length> blockStepSize() const;
    inline void setBlockStepSize(std::optional<Length>);

    static constexpr BlockStepInsert initialBlockStepInsert();
    inline BlockStepInsert blockStepInsert() const;
    inline void setBlockStepInsert(BlockStepInsert);

private:
    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const;

        inline void copyNonInheritedFrom(const NonInheritedFlags&);

        inline bool hasAnyPublicPseudoStyles() const;
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
        unsigned textDecorationLine : TextDecorationLineBits; // Text decorations defined *only* by this element.

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
        unsigned pseudoBits : PublicPseudoIDBits;

        // If you add more style bits here, you will also need to update RenderStyle::NonInheritedFlags::copyNonInheritedFrom().
    };

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const;

        unsigned emptyCells : 1; // EmptyCell
        unsigned captionSide : 2; // CaptionSide
        unsigned listStylePosition : 1; // ListStylePosition
        unsigned visibility : 2; // Visibility
        unsigned textAlign : 4; // TextAlignMode
        unsigned textTransform : TextTransformBits; // OptionSet<TextTransform>
        unsigned textDecorationLines : TextDecorationLineBits;
        unsigned cursor : 6; // CursorType
#if ENABLE(CURSOR_VISIBILITY)
        unsigned cursorVisibility : 1; // CursorVisibility
#endif
        unsigned direction : 1; // TextDirection
        unsigned whiteSpaceCollapse : 3; // WhiteSpaceCollapse
        unsigned textWrap : 3; // TextWrap
        // 33 bits
        unsigned borderCollapse : 1; // BorderCollapse
        unsigned boxDirection : 1; // BoxDirection

        // non CSS2 inherited
        unsigned rtlOrdering : 1; // Order
        unsigned printColorAdjust : 1; // PrintColorAdjust
        unsigned pointerEvents : 4; // PointerEvents
        unsigned insideLink : 2; // InsideLink
        unsigned insideDefaultButton : 1;
        // 44 bits

        // CSS Text Layout Module Level 3: Vertical writing support
        unsigned writingMode : 2; // WritingMode
        // 46 bits

#if ENABLE(TEXT_AUTOSIZING)
        unsigned autosizeStatus : 5;
#endif
        // 51 bits
    };

    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    constexpr DisplayType originalDisplay() const { return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay); }

    inline bool hasAutoLeftAndRight() const;
    inline bool hasAutoTopAndBottom() const;

    static constexpr bool isDisplayInlineType(DisplayType);
    static constexpr bool isDisplayBlockType(DisplayType);
    static constexpr bool isDisplayFlexibleBox(DisplayType);
    static constexpr bool isDisplayGridBox(DisplayType);
    static constexpr bool isDisplayFlexibleOrGridBox(DisplayType);
    static constexpr bool isDisplayDeprecatedFlexibleBox(DisplayType);
    static constexpr bool isDisplayListItemType(DisplayType);
    static constexpr bool isDisplayTableOrTablePart(DisplayType);

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
    DataRef<StyleNonInheritedData> m_nonInheritedData;
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

inline int adjustForAbsoluteZoom(int, const RenderStyle&);
inline float adjustFloatForAbsoluteZoom(float, const RenderStyle&);
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderStyle&);
inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderStyle&);

constexpr BorderStyle collapsedBorderStyle(BorderStyle);

inline bool pseudoElementRendererIsNeeded(const RenderStyle*);
inline bool generatesBox(const RenderStyle&);
inline bool isNonVisibleOverflow(Overflow);

} // namespace WebCore

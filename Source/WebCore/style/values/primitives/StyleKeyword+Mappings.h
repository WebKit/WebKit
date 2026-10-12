/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "CompositeOperation.h"
#include "FontSizeAdjust.h"
#include "GraphicsTypes.h"
#include "RenderStyleConstants.h"
#include "ScrollAxis.h"
#include "ScrollTypes.h"
#include "StyleBuilderState.h"
#include "StyleContain.h"
#include "StyleContainerType.h"
#include "StyleDisplay.h"
#include "StyleHangingPunctuation.h"
#include "StyleImageOrientation.h"
#include "StyleMarginTrim.h"
#include "StyleMaskMode.h"
#include "StylePositionTryFallbackTactic.h"
#include "StylePositionVisibility.h"
#include "StyleResize.h"
#include "StyleScrollBehavior.h"
#include "StyleScrollbarWidth.h"
#include "StyleSpeakAs.h"
#include "StyleTextAlign.h"
#include "StyleTextAlignLast.h"
#include "StyleTextDecorationLine.h"
#include "StyleTextEmphasisPosition.h"
#include "StyleTextTransform.h"
#include "StyleTextUnderlinePosition.h"
#include "StyleTouchAction.h"
#include "StyleVisualBox.h"
#include "StyleWebKitLineBoxContain.h"
#include "StyleWebKitOverflowScrolling.h"
#include "StyleWebKitTouchCallout.h"
#include "StyleWhiteSpaceTrim.h"
#include "TextFlags.h"
#include "TextSpacing.h"
#include "ThemeTypes.h"
#include "UnicodeBidi.h"
#include "WritingMode.h"
#include <wtf/MathExtras.h>

#if ENABLE(APPLE_PAY)
#include "ApplePayButtonPart.h"
#endif

#if HAVE(CORE_MATERIAL)
#include "AppleVisualEffect.h"
#endif

namespace WebCore {

template<typename TargetType> constexpr TargetType fromCSSValueID(CSSValueID);

template<typename TargetType> TargetType fromCSSValue(const CSSValue& value)
{
    return fromCSSValueID<TargetType>(valueID(value));
}

template<typename TargetType> TargetType fromCSSValue(const CSSKeywordValue& value)
{
    return fromCSSValueID<TargetType>(value.valueID());
}

#define EMIT_TO_CSS_SWITCH_CASE(VALUE) case TYPE::VALUE: return CSSValueID::VALUE;
#define EMIT_FROM_CSS_SWITCH_CASE(VALUE) case CSSValueID::VALUE: return TYPE::VALUE;
#define EMIT_VALUE_REPRESENTATION_CSS_SWITCH_CASE(VALUE) case WebCore::TYPE::VALUE: return visitor(CSS::Keyword::VALUE { });

#define DEFINE_TO_CSS_VALUE_ID_FUNCTION \
constexpr CSSValueID toCSSValueID(TYPE value) { \
    switch (value) { \
    FOR_EACH(EMIT_TO_CSS_SWITCH_CASE) \
    } \
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT(); \
    return CSSValueID::Invalid; \
}

#define DEFINE_FROM_CSS_VALUE_ID_FUNCTION \
template<> constexpr TYPE fromCSSValueID(CSSValueID value) { \
    switch (value) { \
    FOR_EACH(EMIT_FROM_CSS_SWITCH_CASE) \
    default: \
        break; \
    } \
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT(); \
    return { }; \
}

#define DEFINE_VALUE_REPRESENTATION_CSS_VALUE_ID_FUNCTION \
template<> struct Style::ValueRepresentation<WebCore::TYPE> { \
    template<typename... F> constexpr decltype(auto) operator()(WebCore::TYPE value, NOESCAPE F&&... f) \
    { \
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...); \
        switch (value) { \
        FOR_EACH(EMIT_VALUE_REPRESENTATION_CSS_SWITCH_CASE) \
        } \
        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT(); \
    } \
};

#define DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS \
    DEFINE_TO_CSS_VALUE_ID_FUNCTION \
    DEFINE_FROM_CSS_VALUE_ID_FUNCTION \
    DEFINE_VALUE_REPRESENTATION_CSS_VALUE_ID_FUNCTION

#define TYPE ReflectionDirection
#define FOR_EACH(CASE) CASE(Above) CASE(Below) CASE(Left) CASE(Right)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ColumnFill
#define FOR_EACH(CASE) CASE(Auto) CASE(Balance)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ColumnSpan
#define FOR_EACH(CASE) CASE(All) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE PrintColorAdjust
#define FOR_EACH(CASE) CASE(Exact) CASE(Economy)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE InterpolateSize
#define FOR_EACH(CASE) CASE(NumericOnly) CASE(AllowKeywords)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BlockStepAlign
#define FOR_EACH(CASE) CASE(Auto) CASE(Center) CASE(Start) CASE(End)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BlockStepInsert
#define FOR_EACH(CASE) CASE(MarginBox) CASE(PaddingBox) CASE(ContentBox)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BlockStepRound
#define FOR_EACH(CASE) CASE(Up) CASE(Down) CASE(Nearest)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(BorderStyle e)
{
    switch (e) {
    case BorderStyle::None:
        return CSSValueID::None;
    case BorderStyle::Hidden:
        return CSSValueID::Hidden;
    case BorderStyle::Inset:
        return CSSValueID::Inset;
    case BorderStyle::Groove:
        return CSSValueID::Groove;
    case BorderStyle::Ridge:
        return CSSValueID::Ridge;
    case BorderStyle::Outset:
        return CSSValueID::Outset;
    case BorderStyle::Dotted:
        return CSSValueID::Dotted;
    case BorderStyle::Dashed:
        return CSSValueID::Dashed;
    case BorderStyle::Solid:
        return CSSValueID::Solid;
    case BorderStyle::Double:
        return CSSValueID::Double;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr BorderStyle fromCSSValueID(CSSValueID valueID)
{
    return static_cast<BorderStyle>(std::to_underlying(valueID) - std::to_underlying(CSSValueID::None));
}

#define TYPE OutlineStyle
#define FOR_EACH(CASE) CASE(Auto) CASE(None) CASE(Inset) CASE(Groove) CASE(Ridge) CASE(Outset) CASE(Dotted) CASE(Dashed) CASE(Solid) CASE(Double)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(CompositeOperator e)
{
    switch (e) {
    case CompositeOperator::SourceOver:
        return CSSValueID::Add;
    case CompositeOperator::SourceIn:
        return CSSValueID::Intersect;
    case CompositeOperator::SourceOut:
        return CSSValueID::Subtract;
    case CompositeOperator::XOR:
        return CSSValueID::Exclude;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

constexpr CSSValueID toCSSValueIDForWebkitMaskComposite(CompositeOperator e)
{
    switch (e) {
    case CompositeOperator::Clear:
        return CSSValueID::Clear;
    case CompositeOperator::Copy:
        return CSSValueID::Copy;
    case CompositeOperator::SourceOver:
        return CSSValueID::SourceOver;
    case CompositeOperator::SourceIn:
        return CSSValueID::SourceIn;
    case CompositeOperator::SourceOut:
        return CSSValueID::SourceOut;
    case CompositeOperator::SourceAtop:
        return CSSValueID::SourceAtop;
    case CompositeOperator::DestinationOver:
        return CSSValueID::DestinationOver;
    case CompositeOperator::DestinationIn:
        return CSSValueID::DestinationIn;
    case CompositeOperator::DestinationOut:
        return CSSValueID::DestinationOut;
    case CompositeOperator::DestinationAtop:
        return CSSValueID::DestinationAtop;
    case CompositeOperator::XOR:
        return CSSValueID::Xor;
    case CompositeOperator::PlusDarker:
        return CSSValueID::PlusDarker;
    case CompositeOperator::PlusLighter:
        return CSSValueID::PlusLighter;
    case CompositeOperator::Difference:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr CompositeOperator fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Clear:
        return CompositeOperator::Clear;
    case CSSValueID::Copy:
        return CompositeOperator::Copy;
    case CSSValueID::SourceOver:
    case CSSValueID::Add:
        return CompositeOperator::SourceOver;
    case CSSValueID::SourceIn:
    case CSSValueID::Intersect:
        return CompositeOperator::SourceIn;
    case CSSValueID::SourceOut:
    case CSSValueID::Subtract:
        return CompositeOperator::SourceOut;
    case CSSValueID::SourceAtop:
        return CompositeOperator::SourceAtop;
    case CSSValueID::DestinationOver:
        return CompositeOperator::DestinationOver;
    case CSSValueID::DestinationIn:
        return CompositeOperator::DestinationIn;
    case CSSValueID::DestinationOut:
        return CompositeOperator::DestinationOut;
    case CSSValueID::DestinationAtop:
        return CompositeOperator::DestinationAtop;
    case CSSValueID::Xor:
    case CSSValueID::Exclude:
        return CompositeOperator::XOR;
    case CSSValueID::PlusDarker:
        return CompositeOperator::PlusDarker;
    case CSSValueID::PlusLighter:
        return CompositeOperator::PlusLighter;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CompositeOperator::Clear;
}

constexpr CSSValueID toCSSValueID(StyleAppearance e)
{
    switch (e) {
    case StyleAppearance::None:
        return CSSValueID::None;
    case StyleAppearance::Auto:
        return CSSValueID::Auto;
    case StyleAppearance::Base:
        return CSSValueID::Base;
    case StyleAppearance::BaseSelect:
        return CSSValueID::BaseSelect;
    case StyleAppearance::Checkbox:
        return CSSValueID::Checkbox;
    case StyleAppearance::Radio:
        return CSSValueID::Radio;
    case StyleAppearance::PushButton:
        return CSSValueID::PushButton;
    case StyleAppearance::SquareButton:
        return CSSValueID::SquareButton;
    case StyleAppearance::Button:
        return CSSValueID::Button;
    case StyleAppearance::DefaultButton:
        return CSSValueID::DefaultButton;
    case StyleAppearance::Listbox:
        return CSSValueID::Listbox;
    case StyleAppearance::Menulist:
        return CSSValueID::Menulist;
    case StyleAppearance::MenulistButton:
        return CSSValueID::MenulistButton;
    case StyleAppearance::Meter:
        return CSSValueID::Meter;
    case StyleAppearance::ProgressBar:
        return CSSValueID::ProgressBar;
    case StyleAppearance::SliderHorizontal:
        return CSSValueID::SliderHorizontal;
    case StyleAppearance::SliderVertical:
        return CSSValueID::SliderVertical;
    case StyleAppearance::SearchField:
        return CSSValueID::Searchfield;
    case StyleAppearance::TextField:
        return CSSValueID::Textfield;
    case StyleAppearance::TextArea:
        return CSSValueID::Textarea;
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
        return CSSValueID::Attachment;
    case StyleAppearance::BorderlessAttachment:
        return CSSValueID::BorderlessAttachment;
#endif
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return CSSValueID::ApplePayButton;
#endif
    case StyleAppearance::ColorWell:
    case StyleAppearance::ColorWellSwatch:
    case StyleAppearance::ColorWellSwatchOverlay:
    case StyleAppearance::ColorWellSwatchWrapper:
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
#endif
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::ListButton:
    case StyleAppearance::SearchFieldDecoration:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SearchFieldResultsButton:
    case StyleAppearance::SearchFieldCancelButton:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::Switch:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return CSSValueID::None;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr StyleAppearance fromCSSValueID(CSSValueID valueID)
{
    if (valueID == CSSValueID::None)
        return StyleAppearance::None;

    if (valueID == CSSValueID::Auto)
        return StyleAppearance::Auto;

    return StyleAppearance(std::to_underlying(valueID) - std::to_underlying(CSSValueID::Base) + static_cast<unsigned>(StyleAppearance::Base));
}

#define TYPE BackfaceVisibility
#define FOR_EACH(CASE) CASE(Visible) CASE(Hidden)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE FieldSizing
#define FOR_EACH(CASE) CASE(Fixed) CASE(Content)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BaselineSource
#define FOR_EACH(CASE) CASE(Auto) CASE(First) CASE(Last)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(FillAttachment e)
{
    switch (e) {
    case FillAttachment::ScrollBackground:
        return CSSValueID::Scroll;
    case FillAttachment::LocalBackground:
        return CSSValueID::Local;
    case FillAttachment::FixedBackground:
        return CSSValueID::Fixed;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr FillAttachment fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Scroll:
        return FillAttachment::ScrollBackground;
    case CSSValueID::Local:
        return FillAttachment::LocalBackground;
    case CSSValueID::Fixed:
        return FillAttachment::FixedBackground;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return FillAttachment::ScrollBackground;
}

constexpr CSSValueID toCSSValueID(FillBox e)
{
    switch (e) {
    case FillBox::BorderBox:
        return CSSValueID::BorderBox;
    case FillBox::PaddingBox:
        return CSSValueID::PaddingBox;
    case FillBox::ContentBox:
        return CSSValueID::ContentBox;
    case FillBox::BorderArea:
        return CSSValueID::BorderArea;
    case FillBox::Text:
        return CSSValueID::Text;
    case FillBox::NoClip:
        return CSSValueID::NoClip;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr FillBox fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Border:
    case CSSValueID::BorderBox:
        return FillBox::BorderBox;
    case CSSValueID::Padding:
    case CSSValueID::PaddingBox:
        return FillBox::PaddingBox;
    case CSSValueID::Content:
    case CSSValueID::ContentBox:
        return FillBox::ContentBox;
    case CSSValueID::BorderArea:
        return FillBox::BorderArea;
    case CSSValueID::Text:
    case CSSValueID::WebkitText:
        return FillBox::Text;
    case CSSValueID::NoClip:
        return FillBox::NoClip;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return FillBox::BorderBox;
}

#define TYPE FillRepeat
#define FOR_EACH(CASE) CASE(Repeat) CASE(NoRepeat) CASE(Round) CASE(Space)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxPack
#define FOR_EACH(CASE) CASE(Start) CASE(Center) CASE(End) CASE(Justify)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxAlignment
#define FOR_EACH(CASE) CASE(Stretch) CASE(Start) CASE(Center) CASE(End) CASE(Baseline)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxDecorationBreak
#define FOR_EACH(CASE) CASE(Slice) CASE(Clone)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Edge
#define FOR_EACH(CASE) CASE(Top) CASE(Right) CASE(Bottom) CASE(Left)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxSizing
#define FOR_EACH(CASE) CASE(BorderBox) CASE(ContentBox)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxDirection
#define FOR_EACH(CASE) CASE(Normal) CASE(Reverse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BoxLines
#define FOR_EACH(CASE) CASE(Single) CASE(Multiple)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(BoxOrient e)
{
    switch (e) {
    case BoxOrient::Horizontal:
        return CSSValueID::Horizontal;
    case BoxOrient::Vertical:
        return CSSValueID::Vertical;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr BoxOrient fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Horizontal:
    case CSSValueID::InlineAxis:
        return BoxOrient::Horizontal;
    case CSSValueID::Vertical:
    case CSSValueID::BlockAxis:
        return BoxOrient::Vertical;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return BoxOrient::Horizontal;
}

#define TYPE CaptionSide
#define FOR_EACH(CASE) CASE(Top) CASE(Bottom)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Clear
#define FOR_EACH(CASE) CASE(None) CASE(Left) CASE(Right) CASE(InlineStart) CASE(InlineEnd) CASE(Both)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextBoxTrim
#define FOR_EACH(CASE) CASE(None) CASE(TrimStart) CASE(TrimEnd) CASE(TrimBoth)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(CursorType e)
{
    switch (e) {
    case CursorType::Auto:
        return CSSValueID::Auto;
    case CursorType::Default:
        return CSSValueID::Default;
    case CursorType::None:
        return CSSValueID::None;
    case CursorType::ContextMenu:
        return CSSValueID::ContextMenu;
    case CursorType::Help:
        return CSSValueID::Help;
    case CursorType::Pointer:
        return CSSValueID::Pointer;
    case CursorType::Progress:
        return CSSValueID::Progress;
    case CursorType::Wait:
        return CSSValueID::Wait;
    case CursorType::Cell:
        return CSSValueID::Cell;
    case CursorType::Crosshair:
        return CSSValueID::Crosshair;
    case CursorType::Text:
        return CSSValueID::Text;
    case CursorType::VerticalText:
        return CSSValueID::VerticalText;
    case CursorType::Alias:
        return CSSValueID::Alias;
    case CursorType::Copy:
        return CSSValueID::Copy;
    case CursorType::Move:
        return CSSValueID::Move;
    case CursorType::NoDrop:
        return CSSValueID::NoDrop;
    case CursorType::NotAllowed:
        return CSSValueID::NotAllowed;
    case CursorType::Grab:
        return CSSValueID::Grab;
    case CursorType::Grabbing:
        return CSSValueID::Grabbing;
    case CursorType::EResize:
        return CSSValueID::EResize;
    case CursorType::NResize:
        return CSSValueID::NResize;
    case CursorType::NEResize:
        return CSSValueID::NeResize;
    case CursorType::NWResize:
        return CSSValueID::NwResize;
    case CursorType::SResize:
        return CSSValueID::SResize;
    case CursorType::SEResize:
        return CSSValueID::SeResize;
    case CursorType::SWResize:
        return CSSValueID::SwResize;
    case CursorType::WResize:
        return CSSValueID::WResize;
    case CursorType::EWResize:
        return CSSValueID::EwResize;
    case CursorType::NSResize:
        return CSSValueID::NsResize;
    case CursorType::NESWResize:
        return CSSValueID::NeswResize;
    case CursorType::NWSEResize:
        return CSSValueID::NwseResize;
    case CursorType::ColumnResize:
        return CSSValueID::ColResize;
    case CursorType::RowResize:
        return CSSValueID::RowResize;
    case CursorType::AllScroll:
        return CSSValueID::AllScroll;
    case CursorType::ZoomIn:
        return CSSValueID::ZoomIn;
    case CursorType::ZoomOut:
        return CSSValueID::ZoomOut;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr CursorType fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Copy:
        return CursorType::Copy;
    case CSSValueID::WebkitGrab:
        return CursorType::Grab;
    case CSSValueID::WebkitGrabbing:
        return CursorType::Grabbing;
    case CSSValueID::WebkitZoomIn:
        return CursorType::ZoomIn;
    case CSSValueID::WebkitZoomOut:
        return CursorType::ZoomOut;
    case CSSValueID::None:
        return CursorType::None;
    default:
        return static_cast<CursorType>(std::to_underlying(valueID) - std::to_underlying(CSSValueID::Auto));
    }
}

#if ENABLE(CURSOR_VISIBILITY)

#define TYPE CursorVisibility
#define FOR_EACH(CASE) CASE(Auto) CASE(AutoHide)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#endif

#define TYPE EmptyCell
#define FOR_EACH(CASE) CASE(Show) CASE(Hide)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE FlexDirection
#define FOR_EACH(CASE) CASE(Row) CASE(RowReverse) CASE(Column) CASE(ColumnReverse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Float
#define FOR_EACH(CASE) CASE(None) CASE(Left) CASE(Right) CASE(InlineStart) CASE(InlineEnd)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE LineBreak
#define FOR_EACH(CASE) CASE(Auto) CASE(Loose) CASE(Normal) CASE(Strict) CASE(AfterWhiteSpace) CASE(Anywhere)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::HangingPunctuationValue
#define FOR_EACH(CASE) CASE(First) CASE(ForceEnd) CASE(AllowEnd) CASE(Last)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::WhiteSpaceTrimValue
#define FOR_EACH(CASE) CASE(DiscardBefore) CASE(DiscardAfter) CASE(DiscardInner)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ListStylePosition
#define FOR_EACH(CASE) CASE(Outside) CASE(Inside)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE MarqueeBehavior
#define FOR_EACH(CASE) CASE(None) CASE(Scroll) CASE(Slide) CASE(Alternate)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(MarqueeDirection direction)
{
    switch (direction) {
    case MarqueeDirection::Forward:
        return CSSValueID::Forwards;
    case MarqueeDirection::Backward:
        return CSSValueID::Backwards;
    case MarqueeDirection::Auto:
        return CSSValueID::Auto;
    case MarqueeDirection::Up:
        return CSSValueID::Up;
    case MarqueeDirection::Down:
        return CSSValueID::Down;
    case MarqueeDirection::Left:
        return CSSValueID::Left;
    case MarqueeDirection::Right:
        return CSSValueID::Right;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr MarqueeDirection fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Forwards:
        return MarqueeDirection::Forward;
    case CSSValueID::Backwards:
        return MarqueeDirection::Backward;
    case CSSValueID::Auto:
        return MarqueeDirection::Auto;
    case CSSValueID::Ahead:
    case CSSValueID::Up: // We don't support vertical languages, so AHEAD just maps to UP.
        return MarqueeDirection::Up;
    case CSSValueID::Reverse:
    case CSSValueID::Down: // REVERSE just maps to DOWN, since we don't do vertical text.
        return MarqueeDirection::Down;
    case CSSValueID::Left:
        return MarqueeDirection::Left;
    case CSSValueID::Right:
        return MarqueeDirection::Right;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return MarqueeDirection::Auto;
}

#define TYPE NBSPMode
#define FOR_EACH(CASE) CASE(Normal) CASE(Space)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(Overflow e)
{
    switch (e) {
    case Overflow::Visible:
        return CSSValueID::Visible;
    case Overflow::Hidden:
        return CSSValueID::Hidden;
    case Overflow::Scroll:
        return CSSValueID::Scroll;
    case Overflow::Auto:
        return CSSValueID::Auto;
    case Overflow::PagedX:
        return CSSValueID::WebkitPagedX;
    case Overflow::PagedY:
        return CSSValueID::WebkitPagedY;
    case Overflow::Clip:
        return CSSValueID::Clip;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr Overflow fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Visible:
        return Overflow::Visible;
    case CSSValueID::Hidden:
        return Overflow::Hidden;
    case CSSValueID::Scroll:
        return Overflow::Scroll;
    case CSSValueID::Overlay:
    case CSSValueID::Auto:
        return Overflow::Auto;
    case CSSValueID::WebkitPagedX:
        return Overflow::PagedX;
    case CSSValueID::WebkitPagedY:
        return Overflow::PagedY;
    case CSSValueID::Clip:
        return Overflow::Clip;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return Overflow::Visible;
}

#define TYPE OverscrollBehavior
#define FOR_EACH(CASE) CASE(Contain) CASE(None) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE OverflowAnchor
#define FOR_EACH(CASE) CASE(None) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ScrollAxisLock
#define FOR_EACH(CASE) CASE(None) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(BreakBetween e)
{
    switch (e) {
    case BreakBetween::Auto:
        return CSSValueID::Auto;
    case BreakBetween::Avoid:
        return CSSValueID::Avoid;
    case BreakBetween::AvoidColumn:
        return CSSValueID::AvoidColumn;
    case BreakBetween::AvoidPage:
        return CSSValueID::AvoidPage;
    case BreakBetween::Column:
        return CSSValueID::Column;
    case BreakBetween::Page:
        return CSSValueID::Page;
    case BreakBetween::LeftPage:
        return CSSValueID::Left;
    case BreakBetween::RightPage:
        return CSSValueID::Right;
    case BreakBetween::RectoPage:
        return CSSValueID::Recto;
    case BreakBetween::VersoPage:
        return CSSValueID::Verso;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr BreakBetween fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return BreakBetween::Auto;
    case CSSValueID::Avoid:
        return BreakBetween::Avoid;
    case CSSValueID::AvoidColumn:
        return BreakBetween::AvoidColumn;
    case CSSValueID::AvoidPage:
        return BreakBetween::AvoidPage;
    case CSSValueID::Column:
        return BreakBetween::Column;
    case CSSValueID::Page:
        return BreakBetween::Page;
    case CSSValueID::Left:
        return BreakBetween::LeftPage;
    case CSSValueID::Right:
        return BreakBetween::RightPage;
    case CSSValueID::Recto:
        return BreakBetween::RectoPage;
    case CSSValueID::Verso:
        return BreakBetween::VersoPage;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return BreakBetween::Auto;
}

#define TYPE BreakInside
#define FOR_EACH(CASE) CASE(Auto) CASE(Avoid) CASE(AvoidColumn) CASE(AvoidPage)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(PositionType e)
{
    switch (e) {
    case PositionType::Static:
        return CSSValueID::Static;
    case PositionType::Relative:
        return CSSValueID::Relative;
    case PositionType::Absolute:
        return CSSValueID::Absolute;
    case PositionType::Fixed:
        return CSSValueID::Fixed;
    case PositionType::Sticky:
        return CSSValueID::Sticky;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr PositionType fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Static:
        return PositionType::Static;
    case CSSValueID::Relative:
        return PositionType::Relative;
    case CSSValueID::Absolute:
        return PositionType::Absolute;
    case CSSValueID::Fixed:
        return PositionType::Fixed;
    case CSSValueID::Sticky:
    case CSSValueID::WebkitSticky:
        return PositionType::Sticky;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return PositionType::Static;
}

#define TYPE Style::Resize
#define FOR_EACH(CASE) CASE(Both) CASE(Horizontal) CASE(Vertical) CASE(Block) CASE(Inline) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TableLayoutType
#define FOR_EACH(CASE) CASE(Auto) CASE(Fixed)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#if ENABLE(SPATIAL_PORTAL)
#define TYPE SpatialType
#define FOR_EACH(CASE) CASE(None) CASE(Portal)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE PortalActionType
#define FOR_EACH(CASE) CASE(None) CASE(Orbit)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE PositionContextType
#define FOR_EACH(CASE) CASE(Container) CASE(Anchor)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH
#endif

constexpr CSSValueID toCSSValueID(Style::TextAlign e)
{
    switch (e) {
    case Style::TextAlign::Start:
        return CSSValueID::Start;
    case Style::TextAlign::End:
        return CSSValueID::End;
    case Style::TextAlign::Left:
        return CSSValueID::Left;
    case Style::TextAlign::Right:
        return CSSValueID::Right;
    case Style::TextAlign::Center:
        return CSSValueID::Center;
    case Style::TextAlign::Justify:
        return CSSValueID::Justify;
    case Style::TextAlign::WebKitLeft:
        return CSSValueID::WebkitLeft;
    case Style::TextAlign::WebKitRight:
        return CSSValueID::WebkitRight;
    case Style::TextAlign::WebKitCenter:
        return CSSValueID::WebkitCenter;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr Style::TextAlign fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::WebkitAuto: // Legacy -webkit-auto. Equivalent to start.
    case CSSValueID::Start:
        return Style::TextAlign::Start;
    case CSSValueID::End:
        return Style::TextAlign::End;
    default:
        return static_cast<Style::TextAlign>(std::to_underlying(valueID) - std::to_underlying(CSSValueID::Left));
    }
}

template<> struct Style::ValueRepresentation<Style::TextAlign> {
    template<typename... F> constexpr decltype(auto) operator()(Style::TextAlign value, NOESCAPE F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        switch (value) {
        case Style::TextAlign::Start:
            return visitor(CSS::Keyword::Start { });
        case Style::TextAlign::End:
            return visitor(CSS::Keyword::End { });
        case Style::TextAlign::Left:
            return visitor(CSS::Keyword::Left { });
        case Style::TextAlign::Right:
            return visitor(CSS::Keyword::Right { });
        case Style::TextAlign::Center:
            return visitor(CSS::Keyword::Center { });
        case Style::TextAlign::Justify:
            return visitor(CSS::Keyword::Justify { });
        case Style::TextAlign::WebKitLeft:
            return visitor(CSS::Keyword::WebkitLeft { });
        case Style::TextAlign::WebKitRight:
            return visitor(CSS::Keyword::WebkitRight { });
        case Style::TextAlign::WebKitCenter:
            return visitor(CSS::Keyword::WebkitCenter { });
        }
        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    }
};

#define TYPE Style::TextAlignLast
#define FOR_EACH(CASE) CASE(Start) CASE(End) CASE(Left) CASE(Right) CASE(Center) CASE(Justify) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextGroupAlign
#define FOR_EACH(CASE) CASE(None) CASE(Start) CASE(End) CASE(Left) CASE(Right) CASE(Center)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(TextJustify e)
{
    switch (e) {
    case TextJustify::Auto:
        return CSSValueID::Auto;
    case TextJustify::None:
        return CSSValueID::None;
    case TextJustify::InterWord:
        return CSSValueID::InterWord;
    case TextJustify::InterCharacter:
        return CSSValueID::InterCharacter;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TextJustify fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return TextJustify::Auto;
    case CSSValueID::None:
        return TextJustify::None;
    case CSSValueID::InterWord:
        return TextJustify::InterWord;
    case CSSValueID::InterCharacter:
    case CSSValueID::Distribute:
        return TextJustify::InterCharacter;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TextJustify::Auto;
}

#define TYPE Style::TextDecorationLine::Flag
#define FOR_EACH(CASE) CASE(Underline) CASE(Overline) CASE(LineThrough) CASE(Blink)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextDecorationStyle
#define FOR_EACH(CASE) CASE(Solid) CASE(Double) CASE(Dotted) CASE(Dashed) CASE(Wavy)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::TextEmphasisPositionValue
#define FOR_EACH(CASE) CASE(Over) CASE(Under) CASE(Left) CASE(Right)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::TextUnderlinePositionValue
#define FOR_EACH(CASE) CASE(FromFont) CASE(Under) CASE(Left) CASE(Right)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextSecurity
#define FOR_EACH(CASE) CASE(None) CASE(Disc) CASE(Circle) CASE(Square)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextDecorationSkipInk
#define FOR_EACH(CASE) CASE(None) CASE(Auto) CASE(All)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::TextTransformValue
#define FOR_EACH(CASE) CASE(Capitalize) CASE(Uppercase) CASE(Lowercase) CASE(FullWidth) CASE(FullSizeKana) CASE(MathAuto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(UnicodeBidi e)
{
    switch (e) {
    case UnicodeBidi::Normal:
        return CSSValueID::Normal;
    case UnicodeBidi::Embed:
        return CSSValueID::Embed;
    case UnicodeBidi::Override:
        return CSSValueID::BidiOverride;
    case UnicodeBidi::Isolate:
        return CSSValueID::Isolate;
    case UnicodeBidi::IsolateOverride:
        return CSSValueID::IsolateOverride;
    case UnicodeBidi::Plaintext:
        return CSSValueID::Plaintext;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr UnicodeBidi fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Normal:
        return UnicodeBidi::Normal;
    case CSSValueID::Embed:
        return UnicodeBidi::Embed;
    case CSSValueID::BidiOverride:
        return UnicodeBidi::Override;
    case CSSValueID::Isolate:
    case CSSValueID::WebkitIsolate:
        return UnicodeBidi::Isolate;
    case CSSValueID::IsolateOverride:
    case CSSValueID::WebkitIsolateOverride:
        return UnicodeBidi::IsolateOverride;
    case CSSValueID::Plaintext:
    case CSSValueID::WebkitPlaintext:
        return UnicodeBidi::Plaintext;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return UnicodeBidi::Normal;
}

#define TYPE UserDrag
#define FOR_EACH(CASE) CASE(Auto) CASE(None) CASE(Element)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE UserModify
#define FOR_EACH(CASE) CASE(ReadOnly) CASE(ReadWrite) CASE(ReadWritePlaintextOnly)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(UserSelect e)
{
    switch (e) {
    case UserSelect::Auto:
        return CSSValueID::Auto;
    case UserSelect::None:
        return CSSValueID::None;
    case UserSelect::Text:
        return CSSValueID::Text;
    case UserSelect::All:
        return CSSValueID::All;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr UserSelect fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return UserSelect::Auto;
    case CSSValueID::None:
        return UserSelect::None;
    case CSSValueID::Text:
        return UserSelect::Text;
    case CSSValueID::All:
        return UserSelect::All;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return UserSelect::Text;
}

#define TYPE Visibility
#define FOR_EACH(CASE) CASE(Visible) CASE(Hidden) CASE(Collapse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE WhiteSpaceCollapse
#define FOR_EACH(CASE) CASE(Collapse) CASE(Preserve) CASE(PreserveBreaks) CASE(BreakSpaces)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE WordBreak
#define FOR_EACH(CASE) CASE(Normal) CASE(BreakAll) CASE(KeepAll) CASE(BreakWord) CASE(AutoPhrase)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE OverflowWrap
#define FOR_EACH(CASE) CASE(Normal) CASE(Anywhere) CASE(BreakWord)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(TextDirection e)
{
    switch (e) {
    case TextDirection::LTR:
        return CSSValueID::Ltr;
    case TextDirection::RTL:
        return CSSValueID::Rtl;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TextDirection fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Ltr:
        return TextDirection::LTR;
    case CSSValueID::Rtl:
        return TextDirection::RTL;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TextDirection::LTR;
}

constexpr CSSValueID toCSSValueID(StyleWritingMode e)
{
    switch (e) {
    case StyleWritingMode::HorizontalTb:
        return CSSValueID::HorizontalTb;
    case StyleWritingMode::VerticalRl:
        return CSSValueID::VerticalRl;
    case StyleWritingMode::VerticalLr:
        return CSSValueID::VerticalLr;
    case StyleWritingMode::SidewaysRl:
        return CSSValueID::SidewaysRl;
    case StyleWritingMode::SidewaysLr:
        return CSSValueID::SidewaysLr;
    case StyleWritingMode::HorizontalBt:
        return CSSValueID::HorizontalBt;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr StyleWritingMode fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::HorizontalTb:
    case CSSValueID::Lr:
    case CSSValueID::LrTb:
    case CSSValueID::Rl:
    case CSSValueID::RlTb:
        return StyleWritingMode::HorizontalTb;
    case CSSValueID::VerticalRl:
    case CSSValueID::Tb:
    case CSSValueID::TbRl:
        return StyleWritingMode::VerticalRl;
    case CSSValueID::VerticalLr:
        return StyleWritingMode::VerticalLr;
    case CSSValueID::SidewaysLr:
        return StyleWritingMode::SidewaysLr;
    case CSSValueID::SidewaysRl:
        return StyleWritingMode::SidewaysRl;
    case CSSValueID::HorizontalBt:
        return StyleWritingMode::HorizontalBt;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return StyleWritingMode::HorizontalTb;
}

constexpr CSSValueID toCSSValueID(TextCombine e)
{
    switch (e) {
    case TextCombine::None:
        return CSSValueID::None;
    case TextCombine::All:
        return CSSValueID::All;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TextCombine fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::None:
        return TextCombine::None;
    case CSSValueID::All:
    case CSSValueID::Horizontal: // -webkit-text-combine only
        return TextCombine::All;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TextCombine::None;
}

constexpr CSSValueID toCSSValueID(RubyPosition e)
{
    switch (e) {
    case RubyPosition::Over:
        return CSSValueID::Over;
    case RubyPosition::Under:
        return CSSValueID::Under;
    case RubyPosition::InterCharacter:
        return CSSValueID::InterCharacter;
    case RubyPosition::LegacyInterCharacter:
        return CSSValueID::LegacyInterCharacter;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr RubyPosition fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Over:
    case CSSValueID::Before: // -webkit-ruby-position only
        return RubyPosition::Over;
    case CSSValueID::Under:
    case CSSValueID::After: // -webkit-ruby-position only
        return RubyPosition::Under;
    case CSSValueID::InterCharacter:
        return RubyPosition::InterCharacter;
    case CSSValueID::LegacyInterCharacter:
        return RubyPosition::LegacyInterCharacter;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return RubyPosition::Over;
}

#define TYPE RubyAlign
#define FOR_EACH(CASE) CASE(Start) CASE(Center) CASE(SpaceBetween) CASE(SpaceAround)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE RubyOverhang
#define FOR_EACH(CASE) CASE(Auto) CASE(Spaces)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(TextWrapMode wrap)
{
    switch (wrap) {
    case TextWrapMode::Wrap:
        return CSSValueID::Wrap;
    case TextWrapMode::NoWrap:
        return CSSValueID::Nowrap;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TextWrapMode fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Wrap:
        return TextWrapMode::Wrap;
    case CSSValueID::Nowrap:
        return TextWrapMode::NoWrap;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TextWrapMode::Wrap;
}

#define TYPE TextWrapStyle
#define FOR_EACH(CASE) CASE(Auto) CASE(Balance) CASE(Pretty) CASE(Stable)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE WrapInside
#define FOR_EACH(CASE) CASE(Auto) CASE(Avoid)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextEmphasisFill
#define FOR_EACH(CASE) CASE(Filled) CASE(Open)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(TextEmphasisMark mark)
{
    switch (mark) {
    case TextEmphasisMark::Dot:
        return CSSValueID::Dot;
    case TextEmphasisMark::Circle:
        return CSSValueID::Circle;
    case TextEmphasisMark::DoubleCircle:
        return CSSValueID::DoubleCircle;
    case TextEmphasisMark::Triangle:
        return CSSValueID::Triangle;
    case TextEmphasisMark::Sesame:
        return CSSValueID::Sesame;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TextEmphasisMark fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Dot:
        return TextEmphasisMark::Dot;
    case CSSValueID::Circle:
        return TextEmphasisMark::Circle;
    case CSSValueID::DoubleCircle:
        return TextEmphasisMark::DoubleCircle;
    case CSSValueID::Triangle:
        return TextEmphasisMark::Triangle;
    case CSSValueID::Sesame:
        return TextEmphasisMark::Sesame;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TextEmphasisMark::Dot;
}

#define TYPE TextOrientation
#define FOR_EACH(CASE) CASE(Sideways) CASE(Mixed) CASE(Upright)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE PointerEvents
#define FOR_EACH(CASE) CASE(None) CASE(Stroke) CASE(Fill) CASE(Painted) \
    CASE(Visible) CASE(VisibleStroke) CASE(VisibleFill) CASE(VisiblePainted) \
    CASE(BoundingBox) CASE(Auto) CASE(All)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(Kerning kerning)
{
    switch (kerning) {
    case Kerning::Auto:
        return CSSValueID::Auto;
    case Kerning::Normal:
        return CSSValueID::Normal;
    case Kerning::NoShift:
        return CSSValueID::None;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr Kerning fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return Kerning::Auto;
    case CSSValueID::Normal:
        return Kerning::Normal;
    case CSSValueID::None:
        return Kerning::NoShift;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return Kerning::Auto;
}

#define TYPE ObjectFit
#define FOR_EACH(CASE) CASE(Fill) CASE(Contain) CASE(Cover) CASE(None) CASE(ScaleDown)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(FontSizeAdjust::Metric metric)
{
    switch (metric) {
    case FontSizeAdjust::Metric::ExHeight:
        return CSSValueID::ExHeight;
    case FontSizeAdjust::Metric::CapHeight:
        return CSSValueID::CapHeight;
    case FontSizeAdjust::Metric::ChWidth:
        return CSSValueID::ChWidth;
    case FontSizeAdjust::Metric::IcWidth:
        return CSSValueID::IcWidth;
    case FontSizeAdjust::Metric::IcHeight:
        return CSSValueID::IcHeight;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Auto;
}

template<> constexpr FontSizeAdjust::Metric fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::ExHeight:
        return FontSizeAdjust::Metric::ExHeight;
    case CSSValueID::CapHeight:
        return FontSizeAdjust::Metric::CapHeight;
    case CSSValueID::ChWidth:
        return FontSizeAdjust::Metric::ChWidth;
    case CSSValueID::IcWidth:
        return FontSizeAdjust::Metric::IcWidth;
    case CSSValueID::IcHeight:
        return FontSizeAdjust::Metric::IcHeight;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return FontSizeAdjust::Metric::ExHeight;
}

#define TYPE FontSmoothingMode
#define FOR_EACH(CASE) CASE(Auto) CASE(None) CASE(Antialiased) CASE(SubpixelAntialiased)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextRenderingMode
#define FOR_EACH(CASE) CASE(Auto) CASE(OptimizeSpeed) CASE(OptimizeLegibility) CASE(GeometricPrecision)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Hyphens
#define FOR_EACH(CASE) CASE(None) CASE(Manual) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE LineSnap
#define FOR_EACH(CASE) CASE(None) CASE(Baseline) CASE(Contain)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE LineAlign
#define FOR_EACH(CASE) CASE(None) CASE(Edges)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::SpeakAsValue
#define FOR_EACH(CASE) CASE(SpellOut) CASE(Digits) CASE(LiteralPunctuation) CASE(NoPunctuation)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Order
#define FOR_EACH(CASE) CASE(Logical) CASE(Visual)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BlendMode
#define FOR_EACH(CASE) CASE(Normal) CASE(Multiply) CASE(Screen) CASE(Overlay) \
    CASE(Darken) CASE(Lighten) CASE(ColorDodge) CASE(ColorBurn) \
    CASE(HardLight) CASE(SoftLight) CASE(Difference) CASE(Exclusion) \
    CASE(Hue) CASE(Saturation) CASE(Color) CASE(Luminosity) CASE(PlusDarker) CASE(PlusLighter)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Isolation
#define FOR_EACH(CASE) CASE(Auto) CASE(Isolate)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE LineCap
#define FOR_EACH(CASE) CASE(Butt) CASE(Round) CASE(Square)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE LineJoin
#define FOR_EACH(CASE) CASE(Miter) CASE(Round) CASE(Bevel)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(WindRule e)
{
    switch (e) {
    case WindRule::NonZero:
        return CSSValueID::Nonzero;
    case WindRule::EvenOdd:
        return CSSValueID::Evenodd;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr WindRule fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Nonzero:
        return WindRule::NonZero;
    case CSSValueID::Evenodd:
        return WindRule::EvenOdd;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return WindRule::NonZero;
}

#define TYPE AlignmentBaseline
#define FOR_EACH(CASE) CASE(AfterEdge) CASE(Alphabetic) CASE(Baseline) \
    CASE(BeforeEdge) CASE(Central) CASE(Hanging) CASE(Ideographic) CASE(Mathematical) \
    CASE(Middle) CASE(TextAfterEdge) CASE(TextBeforeEdge)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BorderCollapse
#define FOR_EACH(CASE) CASE(Separate) CASE(Collapse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(ImageRendering imageRendering)
{
    switch (imageRendering) {
    case ImageRendering::Auto:
        return CSSValueID::Auto;
    case ImageRendering::CrispEdges:
        return CSSValueID::CrispEdges;
    case ImageRendering::Pixelated:
        return CSSValueID::Pixelated;
    case ImageRendering::OptimizeSpeed:
        return CSSValueID::OptimizeSpeed;
    case ImageRendering::OptimizeQuality:
        return CSSValueID::OptimizeQuality;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr ImageRendering fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return ImageRendering::Auto;
    case CSSValueID::WebkitOptimizeContrast:
    case CSSValueID::CrispEdges:
    case CSSValueID::WebkitCrispEdges:
        return ImageRendering::CrispEdges;
    case CSSValueID::Pixelated:
        return ImageRendering::Pixelated;
    case CSSValueID::OptimizeSpeed:
        return ImageRendering::OptimizeSpeed;
    case CSSValueID::OptimizeQuality:
        return ImageRendering::OptimizeQuality;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return ImageRendering::Auto;
}

#if HAVE(CORE_MATERIAL)

constexpr CSSValueID toCSSValueID(AppleVisualEffect effect)
{
    switch (effect) {
    case AppleVisualEffect::None:
        return CSSValueID::None;
    case AppleVisualEffect::BlurUltraThinMaterial:
        return CSSValueID::AppleSystemBlurMaterialUltraThin;
    case AppleVisualEffect::BlurThinMaterial:
        return CSSValueID::AppleSystemBlurMaterialThin;
    case AppleVisualEffect::BlurMaterial:
        return CSSValueID::AppleSystemBlurMaterial;
    case AppleVisualEffect::BlurThickMaterial:
        return CSSValueID::AppleSystemBlurMaterialThick;
    case AppleVisualEffect::BlurChromeMaterial:
        return CSSValueID::AppleSystemBlurMaterialChrome;
#if HAVE(MATERIAL_HOSTING)
    case AppleVisualEffect::GlassMaterial:
        return CSSValueID::AppleSystemGlassMaterial;
    case AppleVisualEffect::GlassClearMaterial:
        return CSSValueID::AppleSystemGlassMaterialClear;
    case AppleVisualEffect::GlassSubduedMaterial:
        return CSSValueID::AppleSystemGlassMaterialSubdued;
    case AppleVisualEffect::GlassMediaControlsMaterial:
        return CSSValueID::AppleSystemGlassMaterialMediaControls;
    case AppleVisualEffect::GlassSubduedMediaControlsMaterial:
        return CSSValueID::AppleSystemGlassMaterialMediaControlsSubdued;
#endif
    case AppleVisualEffect::VibrancyLabel:
        return CSSValueID::AppleSystemVibrancyLabel;
    case AppleVisualEffect::VibrancySecondaryLabel:
        return CSSValueID::AppleSystemVibrancySecondaryLabel;
    case AppleVisualEffect::VibrancyTertiaryLabel:
        return CSSValueID::AppleSystemVibrancyTertiaryLabel;
    case AppleVisualEffect::VibrancyQuaternaryLabel:
        return CSSValueID::AppleSystemVibrancyQuaternaryLabel;
    case AppleVisualEffect::VibrancyFill:
        return CSSValueID::AppleSystemVibrancyFill;
    case AppleVisualEffect::VibrancySecondaryFill:
        return CSSValueID::AppleSystemVibrancySecondaryFill;
    case AppleVisualEffect::VibrancyTertiaryFill:
        return CSSValueID::AppleSystemVibrancyTertiaryFill;
    case AppleVisualEffect::VibrancySeparator:
        return CSSValueID::AppleSystemVibrancySeparator;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr AppleVisualEffect fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::None:
        return AppleVisualEffect::None;
    case CSSValueID::AppleSystemBlurMaterialUltraThin:
        return AppleVisualEffect::BlurUltraThinMaterial;
    case CSSValueID::AppleSystemBlurMaterialThin:
        return AppleVisualEffect::BlurThinMaterial;
    case CSSValueID::AppleSystemBlurMaterial:
        return AppleVisualEffect::BlurMaterial;
    case CSSValueID::AppleSystemBlurMaterialThick:
        return AppleVisualEffect::BlurThickMaterial;
    case CSSValueID::AppleSystemBlurMaterialChrome:
        return AppleVisualEffect::BlurChromeMaterial;
#if HAVE(MATERIAL_HOSTING)
    case CSSValueID::AppleSystemGlassMaterial:
        return AppleVisualEffect::GlassMaterial;
    case CSSValueID::AppleSystemGlassMaterialClear:
        return AppleVisualEffect::GlassClearMaterial;
    case CSSValueID::AppleSystemGlassMaterialSubdued:
        return AppleVisualEffect::GlassSubduedMaterial;
    case CSSValueID::AppleSystemGlassMaterialMediaControls:
        return AppleVisualEffect::GlassMediaControlsMaterial;
    case CSSValueID::AppleSystemGlassMaterialMediaControlsSubdued:
        return AppleVisualEffect::GlassSubduedMediaControlsMaterial;
#endif
    case CSSValueID::AppleSystemVibrancyLabel:
        return AppleVisualEffect::VibrancyLabel;
    case CSSValueID::AppleSystemVibrancySecondaryLabel:
        return AppleVisualEffect::VibrancySecondaryLabel;
    case CSSValueID::AppleSystemVibrancyTertiaryLabel:
        return AppleVisualEffect::VibrancyTertiaryLabel;
    case CSSValueID::AppleSystemVibrancyQuaternaryLabel:
        return AppleVisualEffect::VibrancyQuaternaryLabel;
    case CSSValueID::AppleSystemVibrancyFill:
        return AppleVisualEffect::VibrancyFill;
    case CSSValueID::AppleSystemVibrancySecondaryFill:
        return AppleVisualEffect::VibrancySecondaryFill;
    case CSSValueID::AppleSystemVibrancyTertiaryFill:
        return AppleVisualEffect::VibrancyTertiaryFill;
    case CSSValueID::AppleSystemVibrancySeparator:
        return AppleVisualEffect::VibrancySeparator;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return AppleVisualEffect::None;
}

#endif // HAVE(CORE_MATERIAL)

#define TYPE InputSecurity
#define FOR_EACH(CASE) CASE(Auto) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(TransformStyle3D e)
{
    switch (e) {
    case TransformStyle3D::Flat:
        return CSSValueID::Flat;
    case TransformStyle3D::Preserve3D:
        return CSSValueID::Preserve3d;
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case TransformStyle3D::Separated:
        return CSSValueID::Separated;
#endif
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr TransformStyle3D fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Flat:
        return TransformStyle3D::Flat;
    case CSSValueID::Preserve3d:
        return TransformStyle3D::Preserve3D;
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case CSSValueID::Separated:
        return TransformStyle3D::Separated;
#endif
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return TransformStyle3D::Flat;
}

#define TYPE TransformBox
#define FOR_EACH(CASE) CASE(StrokeBox) CASE(ContentBox) CASE(BorderBox) CASE(FillBox) CASE(ViewBox)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ColumnAxis
#define FOR_EACH(CASE) CASE(Horizontal) CASE(Vertical) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ColumnProgression
#define FOR_EACH(CASE) CASE(Normal) CASE(Reverse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE BufferedRendering
#define FOR_EACH(CASE) CASE(Auto) CASE(Dynamic) CASE(Static)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ColorInterpolation
#define FOR_EACH(CASE) CASE(Auto) CASE(SRGB) CASE(LinearRGB)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE DominantBaseline
#define FOR_EACH(CASE) CASE(Auto) CASE(Central) CASE(Middle) CASE(TextBeforeEdge) \
    CASE(TextAfterEdge) CASE(Ideographic) CASE(Alphabetic) CASE(Hanging) CASE(Mathematical)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(ShapeRendering e)
{
    switch (e) {
    case ShapeRendering::Auto:
        return CSSValueID::Auto;
    case ShapeRendering::OptimizeSpeed:
        return CSSValueID::OptimizeSpeed;
    case ShapeRendering::CrispEdges:
        return CSSValueID::Crispedges; // "crispedges", not "crisp-edges"
    case ShapeRendering::GeometricPrecision:
        return CSSValueID::GeometricPrecision;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr ShapeRendering fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Auto:
        return ShapeRendering::Auto;
    case CSSValueID::OptimizeSpeed:
        return ShapeRendering::OptimizeSpeed;
    case CSSValueID::Crispedges: // "crispedges", not "crisp-edges"
        return ShapeRendering::CrispEdges;
    case CSSValueID::GeometricPrecision:
        return ShapeRendering::GeometricPrecision;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return ShapeRendering::Auto;
}

#define TYPE TextAnchor
#define FOR_EACH(CASE) CASE(Start) CASE(Middle) CASE(End)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE VectorEffect
#define FOR_EACH(CASE) CASE(None) CASE(NonScalingStroke)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE MaskType
#define FOR_EACH(CASE) CASE(Luminance) CASE(Alpha)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(CSSBoxType cssBox)
{
    switch (cssBox) {
    case CSSBoxType::MarginBox:
        return CSSValueID::MarginBox;
    case CSSBoxType::BorderBox:
        return CSSValueID::BorderBox;
    case CSSBoxType::PaddingBox:
        return CSSValueID::PaddingBox;
    case CSSBoxType::ContentBox:
        return CSSValueID::ContentBox;
    case CSSBoxType::FillBox:
        return CSSValueID::FillBox;
    case CSSBoxType::StrokeBox:
        return CSSValueID::StrokeBox;
    case CSSBoxType::ViewBox:
        return CSSValueID::ViewBox;
    case CSSBoxType::BoxMissing:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return CSSValueID::None;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr CSSBoxType fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::MarginBox:
        return CSSBoxType::MarginBox;
    case CSSValueID::BorderBox:
        return CSSBoxType::BorderBox;
    case CSSValueID::PaddingBox:
        return CSSBoxType::PaddingBox;
    case CSSValueID::ContentBox:
        return CSSBoxType::ContentBox;
    // The following are used in an SVG context.
    case CSSValueID::FillBox:
        return CSSBoxType::FillBox;
    case CSSValueID::StrokeBox:
        return CSSBoxType::StrokeBox;
    case CSSValueID::ViewBox:
        return CSSBoxType::ViewBox;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSBoxType::BoxMissing;
}

#define TYPE ItemPosition
#define FOR_EACH(CASE) CASE(Legacy) CASE(Auto) CASE(Normal) CASE(Stretch) CASE(Baseline) \
    CASE(LastBaseline) CASE(Center) CASE(Start) CASE(End) CASE(SelfStart) CASE(SelfEnd) \
    CASE(FlexStart) CASE(FlexEnd) CASE(Left) CASE(Right) CASE(AnchorCenter)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE OverflowAlignment
#define FOR_EACH(CASE) CASE(Default) CASE(Unsafe) CASE(Safe)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ContentPosition
#define FOR_EACH(CASE) CASE(Normal) CASE(Baseline) CASE(LastBaseline) CASE(Center) CASE(Start) \
    CASE(End) CASE(FlexStart) CASE(FlexEnd) CASE(Left) CASE(Right)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ContentDistribution
#define FOR_EACH(CASE) CASE(Default) CASE(SpaceBetween) CASE(SpaceAround) CASE(SpaceEvenly) CASE(Stretch)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextZoom
#define FOR_EACH(CASE) CASE(Normal) CASE(Reset)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::TouchActionValue
#define FOR_EACH(CASE) CASE(PanX) CASE(PanY) CASE(PinchZoom)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ScrollSnapStrictness
#define FOR_EACH(CASE) CASE(Proximity) CASE(Mandatory)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueID(ScrollSnapAxis axis)
{
    switch (axis) {
    case ScrollSnapAxis::XAxis:
        return CSSValueID::X;
    case ScrollSnapAxis::YAxis:
        return CSSValueID::Y;
    case ScrollSnapAxis::Block:
        return CSSValueID::Block;
    case ScrollSnapAxis::Inline:
        return CSSValueID::Inline;
    case ScrollSnapAxis::Both:
        return CSSValueID::Both;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr ScrollSnapAxis fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::X:
        return ScrollSnapAxis::XAxis;
    case CSSValueID::Y:
        return ScrollSnapAxis::YAxis;
    case CSSValueID::Block:
        return ScrollSnapAxis::Block;
    case CSSValueID::Inline:
        return ScrollSnapAxis::Inline;
    case CSSValueID::Both:
        return ScrollSnapAxis::Both;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return ScrollSnapAxis::Both;
}

#define TYPE ScrollSnapAxisAlignType
#define FOR_EACH(CASE) CASE(None) CASE(Start) CASE(Center) CASE(End)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ScrollSnapStop
#define FOR_EACH(CASE) CASE(Normal) CASE(Always)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::ScrollbarWidth
#define FOR_EACH(CASE) CASE(Auto) CASE(Thin) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#if ENABLE(APPLE_PAY)

#define TYPE ApplePayButtonStyle
#define FOR_EACH(CASE) CASE(White) CASE(WhiteOutline) CASE(Black)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ApplePayButtonType
#if !ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
#define FOR_EACH(CASE) CASE(Plain) CASE(Buy) CASE(SetUp) CASE(Donate) CASE(CheckOut) CASE(Book) CASE(Subscribe)
#else
#define FOR_EACH(CASE) CASE(Plain) CASE(Buy) CASE(SetUp) CASE(Donate) CASE(CheckOut) CASE(Book) CASE(Subscribe) \
    CASE(Reload) CASE(AddMoney) CASE(TopUp) CASE(Order) CASE(Rent) CASE(Support) CASE(Contribute) CASE(Tip)
#endif
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#endif

constexpr CSSValueID toCSSValueID(FontVariantPosition position)
{
    switch (position) {
    case FontVariantPosition::Normal:
        return CSSValueID::Normal;
    case FontVariantPosition::Subscript:
        return CSSValueID::Sub;
    case FontVariantPosition::Superscript:
        return CSSValueID::Super;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr FontVariantPosition fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Normal:
        return FontVariantPosition::Normal;
    case CSSValueID::Sub:
        return FontVariantPosition::Subscript;
    case CSSValueID::Super:
        return FontVariantPosition::Superscript;
    default:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return FontVariantPosition::Normal;
    }
}

constexpr CSSValueID toCSSValueID(FontVariantCaps caps)
{
    switch (caps) {
    case FontVariantCaps::Normal:
        return CSSValueID::Normal;
    case FontVariantCaps::Small:
        return CSSValueID::SmallCaps;
    case FontVariantCaps::AllSmall:
        return CSSValueID::AllSmallCaps;
    case FontVariantCaps::Petite:
        return CSSValueID::PetiteCaps;
    case FontVariantCaps::AllPetite:
        return CSSValueID::AllPetiteCaps;
    case FontVariantCaps::Unicase:
        return CSSValueID::Unicase;
    case FontVariantCaps::Titling:
        return CSSValueID::TitlingCaps;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

template<> constexpr FontVariantCaps fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Normal:
        return FontVariantCaps::Normal;
    case CSSValueID::SmallCaps:
        return FontVariantCaps::Small;
    case CSSValueID::AllSmallCaps:
        return FontVariantCaps::AllSmall;
    case CSSValueID::PetiteCaps:
        return FontVariantCaps::Petite;
    case CSSValueID::AllPetiteCaps:
        return FontVariantCaps::AllPetite;
    case CSSValueID::Unicase:
        return FontVariantCaps::Unicase;
    case CSSValueID::TitlingCaps:
        return FontVariantCaps::Titling;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return FontVariantCaps::Normal;
}

#define TYPE FontVariantEmoji
#define FOR_EACH(CASE) CASE(Normal) CASE(Text) CASE(Emoji) CASE(Unicode)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE FontOpticalSizing
#define FOR_EACH(CASE) CASE(Auto) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

template<> constexpr FontTechnology fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::ColorColrv0:
        return FontTechnology::ColorColrv0;
    case CSSValueID::ColorColrv1:
        return FontTechnology::ColorColrv1;
    case CSSValueID::ColorCbdt:
        return FontTechnology::ColorCbdt;
    case CSSValueID::ColorSbix:
        return FontTechnology::ColorSbix;
    case CSSValueID::ColorSvg:
        return FontTechnology::ColorSvg;
    case CSSValueID::FeaturesAat:
        return FontTechnology::FeaturesAat;
    case CSSValueID::FeaturesGraphite:
        return FontTechnology::FeaturesGraphite;
    case CSSValueID::FeaturesOpentype:
        return FontTechnology::FeaturesOpentype;
    case CSSValueID::Incremental:
        return FontTechnology::Incremental;
    case CSSValueID::Palettes:
        return FontTechnology::Palettes;
    case CSSValueID::Variations:
        return FontTechnology::Variations;
    default:
        break;
    }
    return FontTechnology::Invalid;
}

#define TYPE FontSynthesisLonghandValue
#define FOR_EACH(CASE) CASE(Auto) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE FontSynthesisStyleLonghandValue
#define FOR_EACH(CASE) CASE(Auto) CASE(None) CASE(ObliqueOnly)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE FontLoadingBehavior
#define FOR_EACH(CASE) CASE(Auto) CASE(Block) CASE(Swap) CASE(Fallback) CASE(Optional)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE MathShift
#define FOR_EACH(CASE) CASE(Normal) CASE(Compact)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE MathStyle
#define FOR_EACH(CASE) CASE(Normal) CASE(Compact)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ContentVisibility
#define FOR_EACH(CASE) CASE(Visible) CASE(Hidden) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE ScrollAxis
#define FOR_EACH(CASE) CASE(Block) CASE(Inline) CASE(X) CASE(Y)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE QuoteType
#define FOR_EACH(CASE) CASE(OpenQuote) CASE(CloseQuote) CASE(NoOpenQuote) CASE(NoCloseQuote)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE SynthesizedGlyph
#define FOR_EACH(CASE) CASE(PickerUp) CASE(PickerDown)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE OverflowContinue
#define FOR_EACH(CASE) CASE(Auto) CASE(Discard) CASE(Collapse) CASE(WebkitLegacy)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::PositionTryOrder
#define FOR_EACH(CASE) CASE(Normal) CASE(MostWidth) CASE(MostHeight) CASE(MostBlockSize) CASE(MostInlineSize)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::PositionVisibilityValue
#define FOR_EACH(CASE) CASE(AnchorsValid) CASE(AnchorValid) CASE(AnchorsVisible) CASE(AnchorVisible) CASE(NoOverflow)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::PositionTryFallbackTactic
#define FOR_EACH(CASE) CASE(FlipBlock) CASE(FlipInline) CASE(FlipStart) CASE(FlipX) CASE(FlipY)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::ScrollBehavior
#define FOR_EACH(CASE) CASE(Auto) CASE(Smooth)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Scroller
#define FOR_EACH(CASE) CASE(Nearest) CASE(Root) CASE(Self)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE NinePieceImageRule
#define FOR_EACH(CASE) CASE(Stretch) CASE(Round) CASE(Space) CASE(Repeat)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE AnimationDirection
#define FOR_EACH(CASE) CASE(Normal) CASE(Alternate) CASE(Reverse) CASE(AlternateReverse)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE AnimationFillMode
#define FOR_EACH(CASE) CASE(None) CASE(Forwards) CASE(Backwards) CASE(Both)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE AnimationPlayState
#define FOR_EACH(CASE) CASE(Running) CASE(Paused)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE CompositeOperation
#define FOR_EACH(CASE) CASE(Replace) CASE(Add) CASE(Accumulate)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TransitionBehavior
#define FOR_EACH(CASE) CASE(Normal) CASE(AllowDiscrete)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextEdgeOver
#define FOR_EACH(CASE) CASE(Text) CASE(Ideographic) CASE(IdeographicInk) CASE(Cap) CASE(Ex)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextEdgeUnder
#define FOR_EACH(CASE) CASE(Text) CASE(Ideographic) CASE(IdeographicInk) CASE(Alphabetic)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE TextSpacingTrim::TrimType
#define FOR_EACH(CASE) CASE(SpaceAll) CASE(TrimAll) CASE(Auto)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::ImageOrientation
#define FOR_EACH(CASE) CASE(FromImage) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::MarginTrimSide
#define FOR_EACH(CASE) CASE(BlockStart) CASE(BlockEnd)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::WebkitLineBoxContainValue
#define FOR_EACH(CASE) CASE(Block) CASE(Inline) CASE(Font) CASE(Glyphs) CASE(Replaced) CASE(InlineBox) CASE(InitialLetter)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::ContainValue
#define FOR_EACH(CASE) CASE(Size) CASE(InlineSize) CASE(Layout) CASE(Style) CASE(Paint)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::ContainerTypeValue
#define FOR_EACH(CASE) CASE(Size) CASE(InlineSize) CASE(ScrollState)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#define TYPE Style::MaskMode
#define FOR_EACH(CASE) CASE(Alpha) CASE(Luminance) CASE(MatchSource)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

constexpr CSSValueID toCSSValueIDForWebkitMaskSourceType(Style::MaskMode e)
{
    switch (e) {
    case Style::MaskMode::Alpha:
        return CSSValueID::Alpha;
    case Style::MaskMode::Luminance:
        return CSSValueID::Luminance;
    case Style::MaskMode::MatchSource:
        return CSSValueID::Alpha;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return CSSValueID::Invalid;
}

#define TYPE Style::VisualBox
#define FOR_EACH(CASE) CASE(BorderBox) CASE(ContentBox) CASE(PaddingBox)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)

#define TYPE Style::WebkitOverflowScrolling
#define FOR_EACH(CASE) CASE(Auto) CASE(Touch)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#endif

#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)

#define TYPE Style::WebkitTouchCallout
#define FOR_EACH(CASE) CASE(Default) CASE(None)
DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS
#undef TYPE
#undef FOR_EACH

#endif

#undef EMIT_TO_CSS_SWITCH_CASE
#undef EMIT_FROM_CSS_SWITCH_CASE
#undef EMIT_VALUE_REPRESENTATION_CSS_SWITCH_CASE
#undef DEFINE_TO_CSS_VALUE_ID_FUNCTION
#undef DEFINE_FROM_CSS_VALUE_ID_FUNCTION
#undef DEFINE_VALUE_REPRESENTATION_CSS_VALUE_ID_FUNCTION
#undef DEFINE_TO_FROM_CSS_VALUE_ID_FUNCTIONS

}

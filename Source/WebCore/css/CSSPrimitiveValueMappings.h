/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "CSSCalculationValue.h"
#include "CSSFontFamily.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectionDirection.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "GraphicsTypes.h"
#include "Length.h"
#include "LineClampValue.h"
#include "RenderStyleConstants.h"
#include "SVGRenderStyleDefs.h"
#include "TextFlags.h"
#include "ThemeTypes.h"
#include "TouchAction.h"
#include "UnicodeBidi.h"
#include "WritingMode.h"
#include <wtf/MathExtras.h>
#include <wtf/OptionSet.h>

#if ENABLE(CSS_IMAGE_ORIENTATION)
#include "ImageOrientation.h"
#endif

namespace WebCore {

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(short i)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_NUMBER;
    m_value.num = static_cast<double>(i);
}

template<> inline CSSPrimitiveValue::operator short() const
{
    if (m_primitiveUnitType == CSS_NUMBER)
        return clampTo<short>(m_value.num);

    ASSERT_NOT_REACHED();
    return 0;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(unsigned short i)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_NUMBER;
    m_value.num = static_cast<double>(i);
}

template<> inline CSSPrimitiveValue::operator unsigned short() const
{
    if (primitiveType() == CSS_NUMBER)
        return value<unsigned short>();

    ASSERT_NOT_REACHED();
    return 0;
}

template<> inline CSSPrimitiveValue::operator int() const
{
    if (primitiveType() == CSS_NUMBER)
        return value<int>();

    ASSERT_NOT_REACHED();
    return 0;
}

template<> inline CSSPrimitiveValue::operator unsigned() const
{
    if (primitiveType() == CSS_NUMBER)
        return value<unsigned>();

    ASSERT_NOT_REACHED();
    return 0;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(float i)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_NUMBER;
    m_value.num = static_cast<double>(i);
}

template<> inline CSSPrimitiveValue::operator float() const
{
    if (primitiveType() == CSS_NUMBER)
        return value<float>();

    ASSERT_NOT_REACHED();
    return 0.0f;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineClampValue i)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = i.isPercentage() ? CSS_PERCENTAGE : CSS_NUMBER;
    m_value.num = static_cast<double>(i.value());
}

template<> inline CSSPrimitiveValue::operator LineClampValue() const
{
    if (primitiveType() == CSS_NUMBER)
        return LineClampValue(value<int>(), LineClamp::LineCount);

    if (primitiveType() == CSS_PERCENTAGE)
        return LineClampValue(value<int>(), LineClamp::Percentage);

    ASSERT_NOT_REACHED();
    return LineClampValue();
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CSSReflectionDirection e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ReflectionAbove:
        m_value.valueID = CSSValueAbove;
        break;
    case ReflectionBelow:
        m_value.valueID = CSSValueBelow;
        break;
    case ReflectionLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case ReflectionRight:
        m_value.valueID = CSSValueRight;
    }
}

template<> inline CSSPrimitiveValue::operator CSSReflectionDirection() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAbove:
        return ReflectionAbove;
    case CSSValueBelow:
        return ReflectionBelow;
    case CSSValueLeft:
        return ReflectionLeft;
    case CSSValueRight:
        return ReflectionRight;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ReflectionBelow;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnFill columnFill)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (columnFill) {
    case ColumnFill::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ColumnFill::Balance:
        m_value.valueID = CSSValueBalance;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColumnFill() const
{
    if (m_primitiveUnitType == CSS_VALUE_ID) {
        if (m_value.valueID == CSSValueBalance)
            return ColumnFill::Balance;
        if (m_value.valueID == CSSValueAuto)
            return ColumnFill::Auto;
    }
    ASSERT_NOT_REACHED();
    return ColumnFill::Balance;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnSpan columnSpan)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (columnSpan) {
    case ColumnSpan::All:
        m_value.valueID = CSSValueAll;
        break;
    case ColumnSpan::None:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColumnSpan() const
{
    // Map 1 to none for compatibility reasons.
    if (m_primitiveUnitType == CSS_NUMBER && m_value.num == 1)
        return ColumnSpan::None;

    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAll:
        return ColumnSpan::All;
    case CSSValueNone:
        return ColumnSpan::None;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ColumnSpan::None;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(PrintColorAdjust value)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (value) {
    case PrintColorAdjust::Exact:
        m_value.valueID = CSSValueExact;
        break;
    case PrintColorAdjust::Economy:
        m_value.valueID = CSSValueEconomy;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator PrintColorAdjust() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueEconomy:
        return PrintColorAdjust::Economy;
    case CSSValueExact:
        return PrintColorAdjust::Exact;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PrintColorAdjust::Economy;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BorderStyle e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BorderStyle::None:
        m_value.valueID = CSSValueNone;
        break;
    case BorderStyle::Hidden:
        m_value.valueID = CSSValueHidden;
        break;
    case BorderStyle::Inset:
        m_value.valueID = CSSValueInset;
        break;
    case BorderStyle::Groove:
        m_value.valueID = CSSValueGroove;
        break;
    case BorderStyle::Ridge:
        m_value.valueID = CSSValueRidge;
        break;
    case BorderStyle::Outset:
        m_value.valueID = CSSValueOutset;
        break;
    case BorderStyle::Dotted:
        m_value.valueID = CSSValueDotted;
        break;
    case BorderStyle::Dashed:
        m_value.valueID = CSSValueDashed;
        break;
    case BorderStyle::Solid:
        m_value.valueID = CSSValueSolid;
        break;
    case BorderStyle::Double:
        m_value.valueID = CSSValueDouble;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BorderStyle() const
{
    ASSERT(isValueID());

    if (m_value.valueID == CSSValueAuto) // Valid for CSS outline-style
        return BorderStyle::Dotted;
    return static_cast<BorderStyle>(m_value.valueID - CSSValueNone);
}

template<> inline CSSPrimitiveValue::operator OutlineIsAuto() const
{
    ASSERT(isValueID());

    if (m_value.valueID == CSSValueAuto)
        return OutlineIsAuto::On;
    return OutlineIsAuto::Off;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CompositeOperator e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case CompositeClear:
        m_value.valueID = CSSValueClear;
        break;
    case CompositeCopy:
        m_value.valueID = CSSValueCopy;
        break;
    case CompositeSourceOver:
        m_value.valueID = CSSValueSourceOver;
        break;
    case CompositeSourceIn:
        m_value.valueID = CSSValueSourceIn;
        break;
    case CompositeSourceOut:
        m_value.valueID = CSSValueSourceOut;
        break;
    case CompositeSourceAtop:
        m_value.valueID = CSSValueSourceAtop;
        break;
    case CompositeDestinationOver:
        m_value.valueID = CSSValueDestinationOver;
        break;
    case CompositeDestinationIn:
        m_value.valueID = CSSValueDestinationIn;
        break;
    case CompositeDestinationOut:
        m_value.valueID = CSSValueDestinationOut;
        break;
    case CompositeDestinationAtop:
        m_value.valueID = CSSValueDestinationAtop;
        break;
    case CompositeXOR:
        m_value.valueID = CSSValueXor;
        break;
    case CompositePlusDarker:
        m_value.valueID = CSSValuePlusDarker;
        break;
    case CompositePlusLighter:
        m_value.valueID = CSSValuePlusLighter;
        break;
    case CompositeDifference:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator CompositeOperator() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueClear:
        return CompositeClear;
    case CSSValueCopy:
        return CompositeCopy;
    case CSSValueSourceOver:
        return CompositeSourceOver;
    case CSSValueSourceIn:
        return CompositeSourceIn;
    case CSSValueSourceOut:
        return CompositeSourceOut;
    case CSSValueSourceAtop:
        return CompositeSourceAtop;
    case CSSValueDestinationOver:
        return CompositeDestinationOver;
    case CSSValueDestinationIn:
        return CompositeDestinationIn;
    case CSSValueDestinationOut:
        return CompositeDestinationOut;
    case CSSValueDestinationAtop:
        return CompositeDestinationAtop;
    case CSSValueXor:
        return CompositeXOR;
    case CSSValuePlusDarker:
        return CompositePlusDarker;
    case CSSValuePlusLighter:
        return CompositePlusLighter;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CompositeClear;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ControlPart e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case NoControlPart:
        m_value.valueID = CSSValueNone;
        break;
    case CheckboxPart:
        m_value.valueID = CSSValueCheckbox;
        break;
    case RadioPart:
        m_value.valueID = CSSValueRadio;
        break;
    case PushButtonPart:
        m_value.valueID = CSSValuePushButton;
        break;
    case SquareButtonPart:
        m_value.valueID = CSSValueSquareButton;
        break;
    case ButtonPart:
        m_value.valueID = CSSValueButton;
        break;
    case ButtonBevelPart:
        m_value.valueID = CSSValueButtonBevel;
        break;
    case DefaultButtonPart:
        m_value.valueID = CSSValueDefaultButton;
        break;
    case InnerSpinButtonPart:
        m_value.valueID = CSSValueInnerSpinButton;
        break;
    case ListboxPart:
        m_value.valueID = CSSValueListbox;
        break;
    case ListItemPart:
        m_value.valueID = CSSValueListitem;
        break;
    case MediaEnterFullscreenButtonPart:
        m_value.valueID = CSSValueMediaEnterFullscreenButton;
        break;
    case MediaExitFullscreenButtonPart:
        m_value.valueID = CSSValueMediaExitFullscreenButton;
        break;
    case MediaPlayButtonPart:
        m_value.valueID = CSSValueMediaPlayButton;
        break;
    case MediaOverlayPlayButtonPart:
        m_value.valueID = CSSValueMediaOverlayPlayButton;
        break;
    case MediaMuteButtonPart:
        m_value.valueID = CSSValueMediaMuteButton;
        break;
    case MediaSeekBackButtonPart:
        m_value.valueID = CSSValueMediaSeekBackButton;
        break;
    case MediaSeekForwardButtonPart:
        m_value.valueID = CSSValueMediaSeekForwardButton;
        break;
    case MediaRewindButtonPart:
        m_value.valueID = CSSValueMediaRewindButton;
        break;
    case MediaReturnToRealtimeButtonPart:
        m_value.valueID = CSSValueMediaReturnToRealtimeButton;
        break;
    case MediaToggleClosedCaptionsButtonPart:
        m_value.valueID = CSSValueMediaToggleClosedCaptionsButton;
        break;
    case MediaSliderPart:
        m_value.valueID = CSSValueMediaSlider;
        break;
    case MediaSliderThumbPart:
        m_value.valueID = CSSValueMediaSliderthumb;
        break;
    case MediaVolumeSliderContainerPart:
        m_value.valueID = CSSValueMediaVolumeSliderContainer;
        break;
    case MediaVolumeSliderPart:
        m_value.valueID = CSSValueMediaVolumeSlider;
        break;
    case MediaVolumeSliderMuteButtonPart:
        m_value.valueID = CSSValueMediaVolumeSliderMuteButton;
        break;
    case MediaVolumeSliderThumbPart:
        m_value.valueID = CSSValueMediaVolumeSliderthumb;
        break;
    case MediaControlsBackgroundPart:
        m_value.valueID = CSSValueMediaControlsBackground;
        break;
    case MediaControlsFullscreenBackgroundPart:
        m_value.valueID = CSSValueMediaControlsFullscreenBackground;
        break;
    case MediaFullScreenVolumeSliderPart:
        m_value.valueID = CSSValueMediaFullscreenVolumeSlider;
        break;
    case MediaFullScreenVolumeSliderThumbPart:
        m_value.valueID = CSSValueMediaFullscreenVolumeSliderThumb;
        break;
    case MediaCurrentTimePart:
        m_value.valueID = CSSValueMediaCurrentTimeDisplay;
        break;
    case MediaTimeRemainingPart:
        m_value.valueID = CSSValueMediaTimeRemainingDisplay;
        break;
    case MediaControlsLightBarBackgroundPart:
        m_value.valueID = CSSValueMediaControlsLightBarBackground;
        break;
    case MediaControlsDarkBarBackgroundPart:
        m_value.valueID = CSSValueMediaControlsDarkBarBackground;
        break;
    case MenulistPart:
        m_value.valueID = CSSValueMenulist;
        break;
    case MenulistButtonPart:
        m_value.valueID = CSSValueMenulistButton;
        break;
    case MenulistTextPart:
        m_value.valueID = CSSValueMenulistText;
        break;
    case MenulistTextFieldPart:
        m_value.valueID = CSSValueMenulistTextfield;
        break;
    case MeterPart:
        m_value.valueID = CSSValueMeter;
        break;
    case RelevancyLevelIndicatorPart:
        m_value.valueID = CSSValueRelevancyLevelIndicator;
        break;
    case ContinuousCapacityLevelIndicatorPart:
        m_value.valueID = CSSValueContinuousCapacityLevelIndicator;
        break;
    case DiscreteCapacityLevelIndicatorPart:
        m_value.valueID = CSSValueDiscreteCapacityLevelIndicator;
        break;
    case RatingLevelIndicatorPart:
        m_value.valueID = CSSValueRatingLevelIndicator;
        break;
    case ProgressBarPart:
        m_value.valueID = CSSValueProgressBar;
        break;
    case ProgressBarValuePart:
        m_value.valueID = CSSValueProgressBarValue;
        break;
    case SliderHorizontalPart:
        m_value.valueID = CSSValueSliderHorizontal;
        break;
    case SliderVerticalPart:
        m_value.valueID = CSSValueSliderVertical;
        break;
    case SliderThumbHorizontalPart:
        m_value.valueID = CSSValueSliderthumbHorizontal;
        break;
    case SliderThumbVerticalPart:
        m_value.valueID = CSSValueSliderthumbVertical;
        break;
    case CaretPart:
        m_value.valueID = CSSValueCaret;
        break;
    case SearchFieldPart:
        m_value.valueID = CSSValueSearchfield;
        break;
    case SearchFieldDecorationPart:
        m_value.valueID = CSSValueSearchfieldDecoration;
        break;
    case SearchFieldResultsDecorationPart:
        m_value.valueID = CSSValueSearchfieldResultsDecoration;
        break;
    case SearchFieldResultsButtonPart:
        m_value.valueID = CSSValueSearchfieldResultsButton;
        break;
    case SearchFieldCancelButtonPart:
        m_value.valueID = CSSValueSearchfieldCancelButton;
        break;
    case SnapshottedPluginOverlayPart:
        m_value.valueID = CSSValueSnapshottedPluginOverlay;
        break;
    case TextFieldPart:
        m_value.valueID = CSSValueTextfield;
        break;
    case TextAreaPart:
        m_value.valueID = CSSValueTextarea;
        break;
    case CapsLockIndicatorPart:
        m_value.valueID = CSSValueCapsLockIndicator;
        break;
#if ENABLE(ATTACHMENT_ELEMENT)
    case AttachmentPart:
        m_value.valueID = CSSValueAttachment;
        break;
    case BorderlessAttachmentPart:
        m_value.valueID = CSSValueBorderlessAttachment;
        break;
#endif
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart:
        m_value.valueID = CSSValueImageControlsButton;
        break;
#endif
#if ENABLE(APPLE_PAY)
    case ApplePayButtonPart:
        m_value.valueID = CSSValueApplePayButton;
        break;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
        m_value.valueID = CSSValueColorWell;
        break;
#endif
#if ENABLE(DATALIST_ELEMENT)
    case ListButtonPart:
        m_value.valueID = CSSValueListButton;
        break;
#endif
    }
}

template<> inline CSSPrimitiveValue::operator ControlPart() const
{
    ASSERT(isValueID());

    if (m_value.valueID == CSSValueNone)
        return NoControlPart;
    return ControlPart(m_value.valueID - CSSValueCheckbox + 1);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BackfaceVisibility e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BackfaceVisibility::Visible:
        m_value.valueID = CSSValueVisible;
        break;
    case BackfaceVisibility::Hidden:
        m_value.valueID = CSSValueHidden;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BackfaceVisibility() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueVisible:
        return BackfaceVisibility::Visible;
    case CSSValueHidden:
        return BackfaceVisibility::Hidden;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BackfaceVisibility::Hidden;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FillAttachment e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case FillAttachment::ScrollBackground:
        m_value.valueID = CSSValueScroll;
        break;
    case FillAttachment::LocalBackground:
        m_value.valueID = CSSValueLocal;
        break;
    case FillAttachment::FixedBackground:
        m_value.valueID = CSSValueFixed;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FillAttachment() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueScroll:
        return FillAttachment::ScrollBackground;
    case CSSValueLocal:
        return FillAttachment::LocalBackground;
    case CSSValueFixed:
        return FillAttachment::FixedBackground;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FillAttachment::ScrollBackground;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FillBox e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case FillBox::Border:
        m_value.valueID = CSSValueBorderBox;
        break;
    case FillBox::Padding:
        m_value.valueID = CSSValuePaddingBox;
        break;
    case FillBox::Content:
        m_value.valueID = CSSValueContentBox;
        break;
    case FillBox::Text:
        m_value.valueID = CSSValueText;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FillBox() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBorder:
    case CSSValueBorderBox:
        return FillBox::Border;
    case CSSValuePadding:
    case CSSValuePaddingBox:
        return FillBox::Padding;
    case CSSValueContent:
    case CSSValueContentBox:
        return FillBox::Content;
    case CSSValueText:
    case CSSValueWebkitText:
        return FillBox::Text;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FillBox::Border;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FillRepeat e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case FillRepeat::Repeat:
        m_value.valueID = CSSValueRepeat;
        break;
    case FillRepeat::NoRepeat:
        m_value.valueID = CSSValueNoRepeat;
        break;
    case FillRepeat::Round:
        m_value.valueID = CSSValueRound;
        break;
    case FillRepeat::Space:
        m_value.valueID = CSSValueSpace;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FillRepeat() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueRepeat:
        return FillRepeat::Repeat;
    case CSSValueNoRepeat:
        return FillRepeat::NoRepeat;
    case CSSValueRound:
        return FillRepeat::Round;
    case CSSValueSpace:
        return FillRepeat::Space;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FillRepeat::Repeat;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxPack e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxPack::Start:
        m_value.valueID = CSSValueStart;
        break;
    case BoxPack::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case BoxPack::End:
        m_value.valueID = CSSValueEnd;
        break;
    case BoxPack::Justify:
        m_value.valueID = CSSValueJustify;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxPack() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueStart:
        return BoxPack::Start;
    case CSSValueEnd:
        return BoxPack::End;
    case CSSValueCenter:
        return BoxPack::Center;
    case CSSValueJustify:
        return BoxPack::Justify;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxPack::Justify;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxAlignment e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxAlignment::Stretch:
        m_value.valueID = CSSValueStretch;
        break;
    case BoxAlignment::Start:
        m_value.valueID = CSSValueStart;
        break;
    case BoxAlignment::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case BoxAlignment::End:
        m_value.valueID = CSSValueEnd;
        break;
    case BoxAlignment::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxAlignment() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueStretch:
        return BoxAlignment::Stretch;
    case CSSValueStart:
        return BoxAlignment::Start;
    case CSSValueEnd:
        return BoxAlignment::End;
    case CSSValueCenter:
        return BoxAlignment::Center;
    case CSSValueBaseline:
        return BoxAlignment::Baseline;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxAlignment::Stretch;
}

#if ENABLE(CSS_BOX_DECORATION_BREAK)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxDecorationBreak e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxDecorationBreak::Slice:
        m_value.valueID = CSSValueSlice;
        break;
    case BoxDecorationBreak::Clone:
        m_value.valueID = CSSValueClone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxDecorationBreak() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSlice:
        return BoxDecorationBreak::Slice;
    case CSSValueClone:
        return BoxDecorationBreak::Clone;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxDecorationBreak::Slice;
}
#endif

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Edge e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Edge::Top:
        m_value.valueID = CSSValueTop;
        break;
    case Edge::Right:
        m_value.valueID = CSSValueRight;
        break;
    case Edge::Bottom:
        m_value.valueID = CSSValueBottom;
        break;
    case Edge::Left:
        m_value.valueID = CSSValueLeft;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Edge() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueTop:
        return Edge::Top;
    case CSSValueRight:
        return Edge::Right;
    case CSSValueBottom:
        return Edge::Bottom;
    case CSSValueLeft:
        return Edge::Left;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Edge::Top;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxSizing e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxSizing::BorderBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case BoxSizing::ContentBox:
        m_value.valueID = CSSValueContentBox;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxSizing() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBorderBox:
        return BoxSizing::BorderBox;
    case CSSValueContentBox:
        return BoxSizing::ContentBox;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxSizing::BorderBox;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxDirection e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxDirection::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case BoxDirection::Reverse:
        m_value.valueID = CSSValueReverse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxDirection() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return BoxDirection::Normal;
    case CSSValueReverse:
        return BoxDirection::Reverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxDirection::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxLines e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxLines::Single:
        m_value.valueID = CSSValueSingle;
        break;
    case BoxLines::Multiple:
        m_value.valueID = CSSValueMultiple;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxLines() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSingle:
        return BoxLines::Single;
    case CSSValueMultiple:
        return BoxLines::Multiple;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxLines::Single;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BoxOrient e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BoxOrient::Horizontal:
        m_value.valueID = CSSValueHorizontal;
        break;
    case BoxOrient::Vertical:
        m_value.valueID = CSSValueVertical;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BoxOrient() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueHorizontal:
    case CSSValueInlineAxis:
        return BoxOrient::Horizontal;
    case CSSValueVertical:
    case CSSValueBlockAxis:
        return BoxOrient::Vertical;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxOrient::Horizontal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CaptionSide e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case CaptionSide::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case CaptionSide::Right:
        m_value.valueID = CSSValueRight;
        break;
    case CaptionSide::Top:
        m_value.valueID = CSSValueTop;
        break;
    case CaptionSide::Bottom:
        m_value.valueID = CSSValueBottom;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator CaptionSide() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueLeft:
        return CaptionSide::Left;
    case CSSValueRight:
        return CaptionSide::Right;
    case CSSValueTop:
        return CaptionSide::Top;
    case CSSValueBottom:
        return CaptionSide::Bottom;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CaptionSide::Top;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Clear e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Clear::None:
        m_value.valueID = CSSValueNone;
        break;
    case Clear::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case Clear::Right:
        m_value.valueID = CSSValueRight;
        break;
    case Clear::Both:
        m_value.valueID = CSSValueBoth;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Clear() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return Clear::None;
    case CSSValueLeft:
        return Clear::Left;
    case CSSValueRight:
        return Clear::Right;
    case CSSValueBoth:
        return Clear::Both;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Clear::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CursorType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case CursorType::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case CursorType::Default:
        m_value.valueID = CSSValueDefault;
        break;
    case CursorType::None:
        m_value.valueID = CSSValueNone;
        break;
    case CursorType::ContextMenu:
        m_value.valueID = CSSValueContextMenu;
        break;
    case CursorType::Help:
        m_value.valueID = CSSValueHelp;
        break;
    case CursorType::Pointer:
        m_value.valueID = CSSValuePointer;
        break;
    case CursorType::Progress:
        m_value.valueID = CSSValueProgress;
        break;
    case CursorType::Wait:
        m_value.valueID = CSSValueWait;
        break;
    case CursorType::Cell:
        m_value.valueID = CSSValueCell;
        break;
    case CursorType::Crosshair:
        m_value.valueID = CSSValueCrosshair;
        break;
    case CursorType::Text:
        m_value.valueID = CSSValueText;
        break;
    case CursorType::VerticalText:
        m_value.valueID = CSSValueVerticalText;
        break;
    case CursorType::Alias:
        m_value.valueID = CSSValueAlias;
        break;
    case CursorType::Copy:
        m_value.valueID = CSSValueCopy;
        break;
    case CursorType::Move:
        m_value.valueID = CSSValueMove;
        break;
    case CursorType::NoDrop:
        m_value.valueID = CSSValueNoDrop;
        break;
    case CursorType::NotAllowed:
        m_value.valueID = CSSValueNotAllowed;
        break;
    case CursorType::Grab:
        m_value.valueID = CSSValueGrab;
        break;
    case CursorType::Grabbing:
        m_value.valueID = CSSValueGrabbing;
        break;
    case CursorType::EResize:
        m_value.valueID = CSSValueEResize;
        break;
    case CursorType::NResize:
        m_value.valueID = CSSValueNResize;
        break;
    case CursorType::NEResize:
        m_value.valueID = CSSValueNeResize;
        break;
    case CursorType::NWResize:
        m_value.valueID = CSSValueNwResize;
        break;
    case CursorType::SResize:
        m_value.valueID = CSSValueSResize;
        break;
    case CursorType::SEResize:
        m_value.valueID = CSSValueSeResize;
        break;
    case CursorType::SWResize:
        m_value.valueID = CSSValueSwResize;
        break;
    case CursorType::WResize:
        m_value.valueID = CSSValueWResize;
        break;
    case CursorType::EWResize:
        m_value.valueID = CSSValueEwResize;
        break;
    case CursorType::NSResize:
        m_value.valueID = CSSValueNsResize;
        break;
    case CursorType::NESWResize:
        m_value.valueID = CSSValueNeswResize;
        break;
    case CursorType::NWSEResize:
        m_value.valueID = CSSValueNwseResize;
        break;
    case CursorType::ColumnResize:
        m_value.valueID = CSSValueColResize;
        break;
    case CursorType::RowResize:
        m_value.valueID = CSSValueRowResize;
        break;
    case CursorType::AllScroll:
        m_value.valueID = CSSValueAllScroll;
        break;
    case CursorType::ZoomIn:
        m_value.valueID = CSSValueZoomIn;
        break;
    case CursorType::ZoomOut:
        m_value.valueID = CSSValueZoomOut;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator CursorType() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueCopy:
        return CursorType::Copy;
    case CSSValueWebkitGrab:
        return CursorType::Grab;
    case CSSValueWebkitGrabbing:
        return CursorType::Grabbing;
    case CSSValueWebkitZoomIn:
        return CursorType::ZoomIn;
    case CSSValueWebkitZoomOut:
        return CursorType::ZoomOut;
    case CSSValueNone:
        return CursorType::None;
    default:
        return static_cast<CursorType>(m_value.valueID - CSSValueAuto);
    }
}

#if ENABLE(CURSOR_VISIBILITY)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CursorVisibility e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case CursorVisibility::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case CursorVisibility::AutoHide:
        m_value.valueID = CSSValueAutoHide;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator CursorVisibility() const
{
    ASSERT(isValueID());

    if (m_value.valueID == CSSValueAuto)
        return CursorVisibility::Auto;
    if (m_value.valueID == CSSValueAutoHide)
        return CursorVisibility::AutoHide;

    ASSERT_NOT_REACHED();
    return CursorVisibility::Auto;
}
#endif

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(DisplayType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case DisplayType::Inline:
        m_value.valueID = CSSValueInline;
        break;
    case DisplayType::Block:
        m_value.valueID = CSSValueBlock;
        break;
    case DisplayType::ListItem:
        m_value.valueID = CSSValueListItem;
        break;
    case DisplayType::Compact:
        m_value.valueID = CSSValueCompact;
        break;
    case DisplayType::InlineBlock:
        m_value.valueID = CSSValueInlineBlock;
        break;
    case DisplayType::Table:
        m_value.valueID = CSSValueTable;
        break;
    case DisplayType::InlineTable:
        m_value.valueID = CSSValueInlineTable;
        break;
    case DisplayType::TableRowGroup:
        m_value.valueID = CSSValueTableRowGroup;
        break;
    case DisplayType::TableHeaderGroup:
        m_value.valueID = CSSValueTableHeaderGroup;
        break;
    case DisplayType::TableFooterGroup:
        m_value.valueID = CSSValueTableFooterGroup;
        break;
    case DisplayType::TableRow:
        m_value.valueID = CSSValueTableRow;
        break;
    case DisplayType::TableColumnGroup:
        m_value.valueID = CSSValueTableColumnGroup;
        break;
    case DisplayType::TableColumn:
        m_value.valueID = CSSValueTableColumn;
        break;
    case DisplayType::TableCell:
        m_value.valueID = CSSValueTableCell;
        break;
    case DisplayType::TableCaption:
        m_value.valueID = CSSValueTableCaption;
        break;
    case DisplayType::Box:
        m_value.valueID = CSSValueWebkitBox;
        break;
    case DisplayType::InlineBox:
        m_value.valueID = CSSValueWebkitInlineBox;
        break;
    case DisplayType::Flex:
    case DisplayType::WebKitFlex:
        m_value.valueID = CSSValueFlex;
        break;
    case DisplayType::InlineFlex:
    case DisplayType::WebKitInlineFlex:
        m_value.valueID = CSSValueInlineFlex;
        break;
    case DisplayType::Grid:
        m_value.valueID = CSSValueGrid;
        break;
    case DisplayType::InlineGrid:
        m_value.valueID = CSSValueInlineGrid;
        break;
    case DisplayType::None:
        m_value.valueID = CSSValueNone;
        break;
    case DisplayType::Contents:
        m_value.valueID = CSSValueContents;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator DisplayType() const
{
    ASSERT(isValueID());

    if (m_value.valueID == CSSValueNone)
        return DisplayType::None;

    DisplayType display = static_cast<DisplayType>(m_value.valueID - CSSValueInline);
    ASSERT(display >= DisplayType::Inline && display <= DisplayType::None);
    if (display == DisplayType::WebKitFlex)
        return DisplayType::Flex;
    if (display == DisplayType::WebKitInlineFlex)
        return DisplayType::InlineFlex;
    return display;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EmptyCell e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case EmptyCell::Show:
        m_value.valueID = CSSValueShow;
        break;
    case EmptyCell::Hide:
        m_value.valueID = CSSValueHide;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator EmptyCell() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueShow:
        return EmptyCell::Show;
    case CSSValueHide:
        return EmptyCell::Hide;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return EmptyCell::Show;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FlexDirection e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case FlexDirection::Row:
        m_value.valueID = CSSValueRow;
        break;
    case FlexDirection::RowReverse:
        m_value.valueID = CSSValueRowReverse;
        break;
    case FlexDirection::Column:
        m_value.valueID = CSSValueColumn;
        break;
    case FlexDirection::ColumnReverse:
        m_value.valueID = CSSValueColumnReverse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FlexDirection() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueRow:
        return FlexDirection::Row;
    case CSSValueRowReverse:
        return FlexDirection::RowReverse;
    case CSSValueColumn:
        return FlexDirection::Column;
    case CSSValueColumnReverse:
        return FlexDirection::ColumnReverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FlexDirection::Row;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(AlignContent e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case AlignContent::FlexStart:
        m_value.valueID = CSSValueFlexStart;
        break;
    case AlignContent::FlexEnd:
        m_value.valueID = CSSValueFlexEnd;
        break;
    case AlignContent::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case AlignContent::SpaceBetween:
        m_value.valueID = CSSValueSpaceBetween;
        break;
    case AlignContent::SpaceAround:
        m_value.valueID = CSSValueSpaceAround;
        break;
    case AlignContent::Stretch:
        m_value.valueID = CSSValueStretch;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator AlignContent() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueFlexStart:
        return AlignContent::FlexStart;
    case CSSValueFlexEnd:
        return AlignContent::FlexEnd;
    case CSSValueCenter:
        return AlignContent::Center;
    case CSSValueSpaceBetween:
        return AlignContent::SpaceBetween;
    case CSSValueSpaceAround:
        return AlignContent::SpaceAround;
    case CSSValueStretch:
        return AlignContent::Stretch;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return AlignContent::Stretch;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FlexWrap e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case FlexWrap::NoWrap:
        m_value.valueID = CSSValueNowrap;
        break;
    case FlexWrap::Wrap:
        m_value.valueID = CSSValueWrap;
        break;
    case FlexWrap::Reverse:
        m_value.valueID = CSSValueWrapReverse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FlexWrap() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNowrap:
        return FlexWrap::NoWrap;
    case CSSValueWrap:
        return FlexWrap::Wrap;
    case CSSValueWrapReverse:
        return FlexWrap::Reverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FlexWrap::NoWrap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Float e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Float::No:
        m_value.valueID = CSSValueNone;
        break;
    case Float::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case Float::Right:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Float() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueLeft:
        return Float::Left;
    case CSSValueRight:
        return Float::Right;
    case CSSValueNone:
    case CSSValueCenter: // Non-standard CSS value.
        return Float::No;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Float::No;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineBreak e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case LineBreak::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case LineBreak::Loose:
        m_value.valueID = CSSValueLoose;
        break;
    case LineBreak::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case LineBreak::Strict:
        m_value.valueID = CSSValueStrict;
        break;
    case LineBreak::AfterWhiteSpace:
        m_value.valueID = CSSValueAfterWhiteSpace;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator OptionSet<HangingPunctuation>() const
{
    ASSERT(isValueID());
    
    switch (m_value.valueID) {
    case CSSValueNone:
        return OptionSet<HangingPunctuation> { };
    case CSSValueFirst:
        return HangingPunctuation::First;
    case CSSValueLast:
        return HangingPunctuation::Last;
    case CSSValueAllowEnd:
        return HangingPunctuation::AllowEnd;
    case CSSValueForceEnd:
        return HangingPunctuation::ForceEnd;
    default:
        break;
    }
    
    ASSERT_NOT_REACHED();
    return OptionSet<HangingPunctuation> { };
}

template<> inline CSSPrimitiveValue::operator LineBreak() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return LineBreak::Auto;
    case CSSValueLoose:
        return LineBreak::Loose;
    case CSSValueNormal:
        return LineBreak::Normal;
    case CSSValueStrict:
        return LineBreak::Strict;
    case CSSValueAfterWhiteSpace:
        return LineBreak::AfterWhiteSpace;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LineBreak::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ListStylePosition e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ListStylePosition::Outside:
        m_value.valueID = CSSValueOutside;
        break;
    case ListStylePosition::Inside:
        m_value.valueID = CSSValueInside;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ListStylePosition() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueOutside:
        return ListStylePosition::Outside;
    case CSSValueInside:
        return ListStylePosition::Inside;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ListStylePosition::Outside;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ListStyleType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ListStyleType::Afar:
        m_value.valueID = CSSValueAfar;
        break;
    case ListStyleType::Amharic:
        m_value.valueID = CSSValueAmharic;
        break;
    case ListStyleType::AmharicAbegede:
        m_value.valueID = CSSValueAmharicAbegede;
        break;
    case ListStyleType::ArabicIndic:
        m_value.valueID = CSSValueArabicIndic;
        break;
    case ListStyleType::Armenian:
        m_value.valueID = CSSValueArmenian;
        break;
    case ListStyleType::Asterisks:
        m_value.valueID = CSSValueAsterisks;
        break;
    case ListStyleType::Binary:
        m_value.valueID = CSSValueBinary;
        break;
    case ListStyleType::Bengali:
        m_value.valueID = CSSValueBengali;
        break;
    case ListStyleType::Cambodian:
        m_value.valueID = CSSValueCambodian;
        break;
    case ListStyleType::Circle:
        m_value.valueID = CSSValueCircle;
        break;
    case ListStyleType::CjkEarthlyBranch:
        m_value.valueID = CSSValueCjkEarthlyBranch;
        break;
    case ListStyleType::CjkHeavenlyStem:
        m_value.valueID = CSSValueCjkHeavenlyStem;
        break;
    case ListStyleType::CJKIdeographic:
        m_value.valueID = CSSValueCjkIdeographic;
        break;
    case ListStyleType::DecimalLeadingZero:
        m_value.valueID = CSSValueDecimalLeadingZero;
        break;
    case ListStyleType::Decimal:
        m_value.valueID = CSSValueDecimal;
        break;
    case ListStyleType::Devanagari:
        m_value.valueID = CSSValueDevanagari;
        break;
    case ListStyleType::Disc:
        m_value.valueID = CSSValueDisc;
        break;
    case ListStyleType::Ethiopic:
        m_value.valueID = CSSValueEthiopic;
        break;
    case ListStyleType::EthiopicAbegede:
        m_value.valueID = CSSValueEthiopicAbegede;
        break;
    case ListStyleType::EthiopicAbegedeAmEt:
        m_value.valueID = CSSValueEthiopicAbegedeAmEt;
        break;
    case ListStyleType::EthiopicAbegedeGez:
        m_value.valueID = CSSValueEthiopicAbegedeGez;
        break;
    case ListStyleType::EthiopicAbegedeTiEr:
        m_value.valueID = CSSValueEthiopicAbegedeTiEr;
        break;
    case ListStyleType::EthiopicAbegedeTiEt:
        m_value.valueID = CSSValueEthiopicAbegedeTiEt;
        break;
    case ListStyleType::EthiopicHalehameAaEr:
        m_value.valueID = CSSValueEthiopicHalehameAaEr;
        break;
    case ListStyleType::EthiopicHalehameAaEt:
        m_value.valueID = CSSValueEthiopicHalehameAaEt;
        break;
    case ListStyleType::EthiopicHalehameAmEt:
        m_value.valueID = CSSValueEthiopicHalehameAmEt;
        break;
    case ListStyleType::EthiopicHalehameGez:
        m_value.valueID = CSSValueEthiopicHalehameGez;
        break;
    case ListStyleType::EthiopicHalehameOmEt:
        m_value.valueID = CSSValueEthiopicHalehameOmEt;
        break;
    case ListStyleType::EthiopicHalehameSidEt:
        m_value.valueID = CSSValueEthiopicHalehameSidEt;
        break;
    case ListStyleType::EthiopicHalehameSoEt:
        m_value.valueID = CSSValueEthiopicHalehameSoEt;
        break;
    case ListStyleType::EthiopicHalehameTiEr:
        m_value.valueID = CSSValueEthiopicHalehameTiEr;
        break;
    case ListStyleType::EthiopicHalehameTiEt:
        m_value.valueID = CSSValueEthiopicHalehameTiEt;
        break;
    case ListStyleType::EthiopicHalehameTig:
        m_value.valueID = CSSValueEthiopicHalehameTig;
        break;
    case ListStyleType::Footnotes:
        m_value.valueID = CSSValueFootnotes;
        break;
    case ListStyleType::Georgian:
        m_value.valueID = CSSValueGeorgian;
        break;
    case ListStyleType::Gujarati:
        m_value.valueID = CSSValueGujarati;
        break;
    case ListStyleType::Gurmukhi:
        m_value.valueID = CSSValueGurmukhi;
        break;
    case ListStyleType::Hangul:
        m_value.valueID = CSSValueHangul;
        break;
    case ListStyleType::HangulConsonant:
        m_value.valueID = CSSValueHangulConsonant;
        break;
    case ListStyleType::Hebrew:
        m_value.valueID = CSSValueHebrew;
        break;
    case ListStyleType::Hiragana:
        m_value.valueID = CSSValueHiragana;
        break;
    case ListStyleType::HiraganaIroha:
        m_value.valueID = CSSValueHiraganaIroha;
        break;
    case ListStyleType::Kannada:
        m_value.valueID = CSSValueKannada;
        break;
    case ListStyleType::Katakana:
        m_value.valueID = CSSValueKatakana;
        break;
    case ListStyleType::KatakanaIroha:
        m_value.valueID = CSSValueKatakanaIroha;
        break;
    case ListStyleType::Khmer:
        m_value.valueID = CSSValueKhmer;
        break;
    case ListStyleType::Lao:
        m_value.valueID = CSSValueLao;
        break;
    case ListStyleType::LowerAlpha:
        m_value.valueID = CSSValueLowerAlpha;
        break;
    case ListStyleType::LowerArmenian:
        m_value.valueID = CSSValueLowerArmenian;
        break;
    case ListStyleType::LowerGreek:
        m_value.valueID = CSSValueLowerGreek;
        break;
    case ListStyleType::LowerHexadecimal:
        m_value.valueID = CSSValueLowerHexadecimal;
        break;
    case ListStyleType::LowerLatin:
        m_value.valueID = CSSValueLowerLatin;
        break;
    case ListStyleType::LowerNorwegian:
        m_value.valueID = CSSValueLowerNorwegian;
        break;
    case ListStyleType::LowerRoman:
        m_value.valueID = CSSValueLowerRoman;
        break;
    case ListStyleType::Malayalam:
        m_value.valueID = CSSValueMalayalam;
        break;
    case ListStyleType::Mongolian:
        m_value.valueID = CSSValueMongolian;
        break;
    case ListStyleType::Myanmar:
        m_value.valueID = CSSValueMyanmar;
        break;
    case ListStyleType::None:
        m_value.valueID = CSSValueNone;
        break;
    case ListStyleType::Octal:
        m_value.valueID = CSSValueOctal;
        break;
    case ListStyleType::Oriya:
        m_value.valueID = CSSValueOriya;
        break;
    case ListStyleType::Oromo:
        m_value.valueID = CSSValueOromo;
        break;
    case ListStyleType::Persian:
        m_value.valueID = CSSValuePersian;
        break;
    case ListStyleType::Sidama:
        m_value.valueID = CSSValueSidama;
        break;
    case ListStyleType::Somali:
        m_value.valueID = CSSValueSomali;
        break;
    case ListStyleType::Square:
        m_value.valueID = CSSValueSquare;
        break;
    case ListStyleType::Telugu:
        m_value.valueID = CSSValueTelugu;
        break;
    case ListStyleType::Thai:
        m_value.valueID = CSSValueThai;
        break;
    case ListStyleType::Tibetan:
        m_value.valueID = CSSValueTibetan;
        break;
    case ListStyleType::Tigre:
        m_value.valueID = CSSValueTigre;
        break;
    case ListStyleType::TigrinyaEr:
        m_value.valueID = CSSValueTigrinyaEr;
        break;
    case ListStyleType::TigrinyaErAbegede:
        m_value.valueID = CSSValueTigrinyaErAbegede;
        break;
    case ListStyleType::TigrinyaEt:
        m_value.valueID = CSSValueTigrinyaEt;
        break;
    case ListStyleType::TigrinyaEtAbegede:
        m_value.valueID = CSSValueTigrinyaEtAbegede;
        break;
    case ListStyleType::UpperAlpha:
        m_value.valueID = CSSValueUpperAlpha;
        break;
    case ListStyleType::UpperArmenian:
        m_value.valueID = CSSValueUpperArmenian;
        break;
    case ListStyleType::UpperGreek:
        m_value.valueID = CSSValueUpperGreek;
        break;
    case ListStyleType::UpperHexadecimal:
        m_value.valueID = CSSValueUpperHexadecimal;
        break;
    case ListStyleType::UpperLatin:
        m_value.valueID = CSSValueUpperLatin;
        break;
    case ListStyleType::UpperNorwegian:
        m_value.valueID = CSSValueUpperNorwegian;
        break;
    case ListStyleType::UpperRoman:
        m_value.valueID = CSSValueUpperRoman;
        break;
    case ListStyleType::Urdu:
        m_value.valueID = CSSValueUrdu;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ListStyleType() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return ListStyleType::None;
    default:
        return static_cast<ListStyleType>(m_value.valueID - CSSValueDisc);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(MarginCollapse e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case MarginCollapse::Collapse:
        m_value.valueID = CSSValueCollapse;
        break;
    case MarginCollapse::Separate:
        m_value.valueID = CSSValueSeparate;
        break;
    case MarginCollapse::Discard:
        m_value.valueID = CSSValueDiscard;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator MarginCollapse() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueCollapse:
        return MarginCollapse::Collapse;
    case CSSValueSeparate:
        return MarginCollapse::Separate;
    case CSSValueDiscard:
        return MarginCollapse::Discard;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MarginCollapse::Collapse;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(MarqueeBehavior e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case MarqueeBehavior::None:
        m_value.valueID = CSSValueNone;
        break;
    case MarqueeBehavior::Scroll:
        m_value.valueID = CSSValueScroll;
        break;
    case MarqueeBehavior::Slide:
        m_value.valueID = CSSValueSlide;
        break;
    case MarqueeBehavior::Alternate:
        m_value.valueID = CSSValueAlternate;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator MarqueeBehavior() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return MarqueeBehavior::None;
    case CSSValueScroll:
        return MarqueeBehavior::Scroll;
    case CSSValueSlide:
        return MarqueeBehavior::Slide;
    case CSSValueAlternate:
        return MarqueeBehavior::Alternate;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MarqueeBehavior::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(MarqueeDirection direction)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (direction) {
    case MarqueeDirection::Forward:
        m_value.valueID = CSSValueForwards;
        break;
    case MarqueeDirection::Backward:
        m_value.valueID = CSSValueBackwards;
        break;
    case MarqueeDirection::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case MarqueeDirection::Up:
        m_value.valueID = CSSValueUp;
        break;
    case MarqueeDirection::Down:
        m_value.valueID = CSSValueDown;
        break;
    case MarqueeDirection::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case MarqueeDirection::Right:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator MarqueeDirection() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueForwards:
        return MarqueeDirection::Forward;
    case CSSValueBackwards:
        return MarqueeDirection::Backward;
    case CSSValueAuto:
        return MarqueeDirection::Auto;
    case CSSValueAhead:
    case CSSValueUp: // We don't support vertical languages, so AHEAD just maps to UP.
        return MarqueeDirection::Up;
    case CSSValueReverse:
    case CSSValueDown: // REVERSE just maps to DOWN, since we don't do vertical text.
        return MarqueeDirection::Down;
    case CSSValueLeft:
        return MarqueeDirection::Left;
    case CSSValueRight:
        return MarqueeDirection::Right;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MarqueeDirection::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(NBSPMode e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case NBSPMode::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case NBSPMode::Space:
        m_value.valueID = CSSValueSpace;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator NBSPMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSpace:
        return NBSPMode::Space;
    case CSSValueNormal:
        return NBSPMode::Normal;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return NBSPMode::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Overflow e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Overflow::Visible:
        m_value.valueID = CSSValueVisible;
        break;
    case Overflow::Hidden:
        m_value.valueID = CSSValueHidden;
        break;
    case Overflow::Scroll:
        m_value.valueID = CSSValueScroll;
        break;
    case Overflow::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case Overflow::PagedX:
        m_value.valueID = CSSValueWebkitPagedX;
        break;
    case Overflow::PagedY:
        m_value.valueID = CSSValueWebkitPagedY;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Overflow() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueVisible:
        return Overflow::Visible;
    case CSSValueHidden:
        return Overflow::Hidden;
    case CSSValueScroll:
        return Overflow::Scroll;
    case CSSValueOverlay:
    case CSSValueAuto:
        return Overflow::Auto;
    case CSSValueWebkitPagedX:
        return Overflow::PagedX;
    case CSSValueWebkitPagedY:
        return Overflow::PagedY;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Overflow::Visible;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BreakBetween e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BreakBetween::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case BreakBetween::Avoid:
        m_value.valueID = CSSValueAvoid;
        break;
    case BreakBetween::AvoidColumn:
        m_value.valueID = CSSValueAvoidColumn;
        break;
    case BreakBetween::AvoidPage:
        m_value.valueID = CSSValueAvoidPage;
        break;
    case BreakBetween::Column:
        m_value.valueID = CSSValueColumn;
        break;
    case BreakBetween::Page:
        m_value.valueID = CSSValuePage;
        break;
    case BreakBetween::LeftPage:
        m_value.valueID = CSSValueLeft;
        break;
    case BreakBetween::RightPage:
        m_value.valueID = CSSValueRight;
        break;
    case BreakBetween::RectoPage:
        m_value.valueID = CSSValueRecto;
        break;
    case BreakBetween::VersoPage:
        m_value.valueID = CSSValueVerso;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BreakBetween() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return BreakBetween::Auto;
    case CSSValueAvoid:
        return BreakBetween::Avoid;
    case CSSValueAvoidColumn:
        return BreakBetween::AvoidColumn;
    case CSSValueAvoidPage:
        return BreakBetween::AvoidPage;
    case CSSValueColumn:
        return BreakBetween::Column;
    case CSSValuePage:
        return BreakBetween::Page;
    case CSSValueLeft:
        return BreakBetween::LeftPage;
    case CSSValueRight:
        return BreakBetween::RightPage;
    case CSSValueRecto:
        return BreakBetween::RectoPage;
    case CSSValueVerso:
        return BreakBetween::VersoPage;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BreakBetween::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BreakInside e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BreakInside::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case BreakInside::Avoid:
        m_value.valueID = CSSValueAvoid;
        break;
    case BreakInside::AvoidColumn:
        m_value.valueID = CSSValueAvoidColumn;
        break;
    case BreakInside::AvoidPage:
        m_value.valueID = CSSValueAvoidPage;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BreakInside() const
{
    ASSERT(isValueID());
    
    switch (m_value.valueID) {
    case CSSValueAuto:
        return BreakInside::Auto;
    case CSSValueAvoid:
        return BreakInside::Avoid;
    case CSSValueAvoidColumn:
        return BreakInside::AvoidColumn;
    case CSSValueAvoidPage:
        return BreakInside::AvoidPage;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BreakInside::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(PositionType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case PositionType::Static:
        m_value.valueID = CSSValueStatic;
        break;
    case PositionType::Relative:
        m_value.valueID = CSSValueRelative;
        break;
    case PositionType::Absolute:
        m_value.valueID = CSSValueAbsolute;
        break;
    case PositionType::Fixed:
        m_value.valueID = CSSValueFixed;
        break;
    case PositionType::Sticky:
        m_value.valueID = CSSValueWebkitSticky;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator PositionType() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueStatic:
        return PositionType::Static;
    case CSSValueRelative:
        return PositionType::Relative;
    case CSSValueAbsolute:
        return PositionType::Absolute;
    case CSSValueFixed:
        return PositionType::Fixed;
    case CSSValueWebkitSticky:
        return PositionType::Sticky;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PositionType::Static;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Resize e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Resize::Both:
        m_value.valueID = CSSValueBoth;
        break;
    case Resize::Horizontal:
        m_value.valueID = CSSValueHorizontal;
        break;
    case Resize::Vertical:
        m_value.valueID = CSSValueVertical;
        break;
    case Resize::None:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Resize() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBoth:
        return Resize::Both;
    case CSSValueHorizontal:
        return Resize::Horizontal;
    case CSSValueVertical:
        return Resize::Vertical;
    case CSSValueAuto:
        ASSERT_NOT_REACHED(); // Depends on settings, thus should be handled by the caller.
        return Resize::None;
    case CSSValueNone:
        return Resize::None;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Resize::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TableLayoutType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TableLayoutType::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case TableLayoutType::Fixed:
        m_value.valueID = CSSValueFixed;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TableLayoutType() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueFixed:
        return TableLayoutType::Fixed;
    case CSSValueAuto:
        return TableLayoutType::Auto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TableLayoutType::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextAlignMode e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextAlignMode::Start:
        m_value.valueID = CSSValueStart;
        break;
    case TextAlignMode::End:
        m_value.valueID = CSSValueEnd;
        break;
    case TextAlignMode::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case TextAlignMode::Right:
        m_value.valueID = CSSValueRight;
        break;
    case TextAlignMode::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case TextAlignMode::Justify:
        m_value.valueID = CSSValueJustify;
        break;
    case TextAlignMode::WebKitLeft:
        m_value.valueID = CSSValueWebkitLeft;
        break;
    case TextAlignMode::WebKitRight:
        m_value.valueID = CSSValueWebkitRight;
        break;
    case TextAlignMode::WebKitCenter:
        m_value.valueID = CSSValueWebkitCenter;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextAlignMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueWebkitAuto: // Legacy -webkit-auto. Eqiuvalent to start.
    case CSSValueStart:
        return TextAlignMode::Start;
    case CSSValueEnd:
        return TextAlignMode::End;
    default:
        return static_cast<TextAlignMode>(m_value.valueID - CSSValueLeft);
    }
}

#if ENABLE(CSS3_TEXT)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextAlignLast e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextAlignLast::Start:
        m_value.valueID = CSSValueStart;
        break;
    case TextAlignLast::End:
        m_value.valueID = CSSValueEnd;
        break;
    case TextAlignLast::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case TextAlignLast::Right:
        m_value.valueID = CSSValueRight;
        break;
    case TextAlignLast::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case TextAlignLast::Justify:
        m_value.valueID = CSSValueJustify;
        break;
    case TextAlignLast::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextAlignLast() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextAlignLast::Auto;
    case CSSValueStart:
        return TextAlignLast::Start;
    case CSSValueEnd:
        return TextAlignLast::End;
    case CSSValueLeft:
        return TextAlignLast::Left;
    case CSSValueRight:
        return TextAlignLast::Right;
    case CSSValueCenter:
        return TextAlignLast::Center;
    case CSSValueJustify:
        return TextAlignLast::Justify;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextAlignLast::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextJustify e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextJustify::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case TextJustify::None:
        m_value.valueID = CSSValueNone;
        break;
    case TextJustify::InterWord:
        m_value.valueID = CSSValueInterWord;
        break;
    case TextJustify::Distribute:
        m_value.valueID = CSSValueDistribute;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextJustify() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextJustify::Auto;
    case CSSValueNone:
        return TextJustify::None;
    case CSSValueInterWord:
        return TextJustify::InterWord;
    case CSSValueDistribute:
        return TextJustify::Distribute;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextJustify::Auto;
}
#endif // CSS3_TEXT

template<> inline CSSPrimitiveValue::operator OptionSet<TextDecoration>() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return OptionSet<TextDecoration> { };
    case CSSValueUnderline:
        return TextDecoration::Underline;
    case CSSValueOverline:
        return TextDecoration::Overline;
    case CSSValueLineThrough:
        return TextDecoration::LineThrough;
    case CSSValueBlink:
        return TextDecoration::Blink;
#if ENABLE(LETTERPRESS)
    case CSSValueWebkitLetterpress:
        return TextDecoration::Letterpress;
#endif
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return OptionSet<TextDecoration> { };
}

template<> inline CSSPrimitiveValue::operator TextDecorationStyle() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSolid:
        return TextDecorationStyle::Solid;
    case CSSValueDouble:
        return TextDecorationStyle::Double;
    case CSSValueDotted:
        return TextDecorationStyle::Dotted;
    case CSSValueDashed:
        return TextDecorationStyle::Dashed;
    case CSSValueWavy:
        return TextDecorationStyle::Wavy;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextDecorationStyle::Solid;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextUnderlinePosition position)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (position) {
    case TextUnderlinePosition::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case TextUnderlinePosition::Under:
        m_value.valueID = CSSValueUnder;
        break;
    case TextUnderlinePosition::FromFont:
        m_value.valueID = CSSValueFromFont;
        break;
    }

    // FIXME: Implement support for 'under left' and 'under right' values.
}

template<> inline CSSPrimitiveValue::operator TextUnderlinePosition() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextUnderlinePosition::Auto;
    case CSSValueUnder:
        return TextUnderlinePosition::Under;
    case CSSValueFromFont:
        return TextUnderlinePosition::FromFont;
    default:
        break;
    }

    // FIXME: Implement support for 'under left' and 'under right' values.
    ASSERT_NOT_REACHED();
    return TextUnderlinePosition::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextSecurity e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextSecurity::None:
        m_value.valueID = CSSValueNone;
        break;
    case TextSecurity::Disc:
        m_value.valueID = CSSValueDisc;
        break;
    case TextSecurity::Circle:
        m_value.valueID = CSSValueCircle;
        break;
    case TextSecurity::Square:
        m_value.valueID = CSSValueSquare;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextSecurity() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return TextSecurity::None;
    case CSSValueDisc:
        return TextSecurity::Disc;
    case CSSValueCircle:
        return TextSecurity::Circle;
    case CSSValueSquare:
        return TextSecurity::Square;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextSecurity::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextTransform e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextTransform::Capitalize:
        m_value.valueID = CSSValueCapitalize;
        break;
    case TextTransform::Uppercase:
        m_value.valueID = CSSValueUppercase;
        break;
    case TextTransform::Lowercase:
        m_value.valueID = CSSValueLowercase;
        break;
    case TextTransform::None:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextTransform() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueCapitalize:
        return TextTransform::Capitalize;
    case CSSValueUppercase:
        return TextTransform::Uppercase;
    case CSSValueLowercase:
        return TextTransform::Lowercase;
    case CSSValueNone:
        return TextTransform::None;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextTransform::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUnicodeBidi e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case UBNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case Embed:
        m_value.valueID = CSSValueEmbed;
        break;
    case Override:
        m_value.valueID = CSSValueBidiOverride;
        break;
    case Isolate:
        m_value.valueID = CSSValueIsolate;
        break;
    case IsolateOverride:
        m_value.valueID = CSSValueIsolateOverride;
        break;
    case Plaintext:
        m_value.valueID = CSSValuePlaintext;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator EUnicodeBidi() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return UBNormal;
    case CSSValueEmbed:
        return Embed;
    case CSSValueBidiOverride:
        return Override;
    case CSSValueIsolate:
    case CSSValueWebkitIsolate:
        return Isolate;
    case CSSValueIsolateOverride:
    case CSSValueWebkitIsolateOverride:
        return IsolateOverride;
    case CSSValuePlaintext:
    case CSSValueWebkitPlaintext:
        return Plaintext;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return UBNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(UserDrag e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case UserDrag::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case UserDrag::None:
        m_value.valueID = CSSValueNone;
        break;
    case UserDrag::Element:
        m_value.valueID = CSSValueElement;
        break;
    default:
        break;
    }
}

template<> inline CSSPrimitiveValue::operator UserDrag() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return UserDrag::Auto;
    case CSSValueNone:
        return UserDrag::None;
    case CSSValueElement:
        return UserDrag::Element;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return UserDrag::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(UserModify e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case UserModify::ReadOnly:
        m_value.valueID = CSSValueReadOnly;
        break;
    case UserModify::ReadWrite:
        m_value.valueID = CSSValueReadWrite;
        break;
    case UserModify::ReadWritePlaintextOnly:
        m_value.valueID = CSSValueReadWritePlaintextOnly;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator UserModify() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueReadOnly:
        return UserModify::ReadOnly;
    case CSSValueReadWrite:
        return UserModify::ReadWrite;
    case CSSValueReadWritePlaintextOnly:
        return UserModify::ReadWritePlaintextOnly;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return UserModify::ReadOnly;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(UserSelect e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case UserSelect::None:
        m_value.valueID = CSSValueNone;
        break;
    case UserSelect::Text:
        m_value.valueID = CSSValueText;
        break;
    case UserSelect::All:
        m_value.valueID = CSSValueAll;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator UserSelect() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return UserSelect::Text;
    case CSSValueNone:
        return UserSelect::None;
    case CSSValueText:
        return UserSelect::Text;
    case CSSValueAll:
        return UserSelect::All;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return UserSelect::Text;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(VerticalAlign a)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (a) {
    case VerticalAlign::Top:
        m_value.valueID = CSSValueTop;
        break;
    case VerticalAlign::Bottom:
        m_value.valueID = CSSValueBottom;
        break;
    case VerticalAlign::Middle:
        m_value.valueID = CSSValueMiddle;
        break;
    case VerticalAlign::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case VerticalAlign::TextBottom:
        m_value.valueID = CSSValueTextBottom;
        break;
    case VerticalAlign::TextTop:
        m_value.valueID = CSSValueTextTop;
        break;
    case VerticalAlign::Sub:
        m_value.valueID = CSSValueSub;
        break;
    case VerticalAlign::Super:
        m_value.valueID = CSSValueSuper;
        break;
    case VerticalAlign::BaselineMiddle:
        m_value.valueID = CSSValueWebkitBaselineMiddle;
        break;
    case VerticalAlign::Length:
        m_value.valueID = CSSValueInvalid;
    }
}

template<> inline CSSPrimitiveValue::operator VerticalAlign() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueTop:
        return VerticalAlign::Top;
    case CSSValueBottom:
        return VerticalAlign::Bottom;
    case CSSValueMiddle:
        return VerticalAlign::Middle;
    case CSSValueBaseline:
        return VerticalAlign::Baseline;
    case CSSValueTextBottom:
        return VerticalAlign::TextBottom;
    case CSSValueTextTop:
        return VerticalAlign::TextTop;
    case CSSValueSub:
        return VerticalAlign::Sub;
    case CSSValueSuper:
        return VerticalAlign::Super;
    case CSSValueWebkitBaselineMiddle:
        return VerticalAlign::BaselineMiddle;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return VerticalAlign::Top;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Visibility e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Visibility::Visible:
        m_value.valueID = CSSValueVisible;
        break;
    case Visibility::Hidden:
        m_value.valueID = CSSValueHidden;
        break;
    case Visibility::Collapse:
        m_value.valueID = CSSValueCollapse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Visibility() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueHidden:
        return Visibility::Hidden;
    case CSSValueVisible:
        return Visibility::Visible;
    case CSSValueCollapse:
        return Visibility::Collapse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Visibility::Visible;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WhiteSpace e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case WhiteSpace::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case WhiteSpace::Pre:
        m_value.valueID = CSSValuePre;
        break;
    case WhiteSpace::PreWrap:
        m_value.valueID = CSSValuePreWrap;
        break;
    case WhiteSpace::PreLine:
        m_value.valueID = CSSValuePreLine;
        break;
    case WhiteSpace::NoWrap:
        m_value.valueID = CSSValueNowrap;
        break;
    case WhiteSpace::KHTMLNoWrap:
        m_value.valueID = CSSValueWebkitNowrap;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator WhiteSpace() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueWebkitNowrap:
        return WhiteSpace::KHTMLNoWrap;
    case CSSValueNowrap:
        return WhiteSpace::NoWrap;
    case CSSValuePre:
        return WhiteSpace::Pre;
    case CSSValuePreWrap:
        return WhiteSpace::PreWrap;
    case CSSValuePreLine:
        return WhiteSpace::PreLine;
    case CSSValueNormal:
        return WhiteSpace::Normal;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return WhiteSpace::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WordBreak e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case WordBreak::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case WordBreak::BreakAll:
        m_value.valueID = CSSValueBreakAll;
        break;
    case WordBreak::KeepAll:
        m_value.valueID = CSSValueKeepAll;
        break;
    case WordBreak::BreakWord:
        m_value.valueID = CSSValueBreakWord;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator WordBreak() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBreakAll:
        return WordBreak::BreakAll;
    case CSSValueKeepAll:
        return WordBreak::KeepAll;
    case CSSValueBreakWord:
        return WordBreak::BreakWord;
    case CSSValueNormal:
        return WordBreak::Normal;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return WordBreak::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(OverflowWrap e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case OverflowWrap::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case OverflowWrap::Break:
        m_value.valueID = CSSValueBreakWord;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator OverflowWrap() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBreakWord:
        return OverflowWrap::Break;
    case CSSValueNormal:
        return OverflowWrap::Normal;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return OverflowWrap::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextDirection e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextDirection::LTR:
        m_value.valueID = CSSValueLtr;
        break;
    case TextDirection::RTL:
        m_value.valueID = CSSValueRtl;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextDirection() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueLtr:
        return TextDirection::LTR;
    case CSSValueRtl:
        return TextDirection::RTL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextDirection::LTR;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WritingMode e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TopToBottomWritingMode:
        m_value.valueID = CSSValueHorizontalTb;
        break;
    case RightToLeftWritingMode:
        m_value.valueID = CSSValueVerticalRl;
        break;
    case LeftToRightWritingMode:
        m_value.valueID = CSSValueVerticalLr;
        break;
    case BottomToTopWritingMode:
        m_value.valueID = CSSValueHorizontalBt;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator WritingMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueHorizontalTb:
    case CSSValueLr:
    case CSSValueLrTb:
    case CSSValueRl:
    case CSSValueRlTb:
        return TopToBottomWritingMode;
    case CSSValueVerticalRl:
    case CSSValueTb:
    case CSSValueTbRl:
        return RightToLeftWritingMode;
    case CSSValueVerticalLr:
        return LeftToRightWritingMode;
    case CSSValueHorizontalBt:
        return BottomToTopWritingMode;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TopToBottomWritingMode;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextCombine e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextCombine::None:
        m_value.valueID = CSSValueNone;
        break;
    case TextCombine::Horizontal:
        m_value.valueID = CSSValueHorizontal;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextCombine() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return TextCombine::None;
    case CSSValueHorizontal:
        return TextCombine::Horizontal;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextCombine::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(RubyPosition position)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (position) {
    case RubyPosition::Before:
        m_value.valueID = CSSValueBefore;
        break;
    case RubyPosition::After:
        m_value.valueID = CSSValueAfter;
        break;
    case RubyPosition::InterCharacter:
        m_value.valueID = CSSValueInterCharacter;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator RubyPosition() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBefore:
        return RubyPosition::Before;
    case CSSValueAfter:
        return RubyPosition::After;
    case CSSValueInterCharacter:
        return RubyPosition::InterCharacter;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RubyPosition::Before;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextOverflow overflow)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (overflow) {
    case TextOverflow::Clip:
        m_value.valueID = CSSValueClip;
        break;
    case TextOverflow::Ellipsis:
        m_value.valueID = CSSValueEllipsis;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextOverflow() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueClip:
        return TextOverflow::Clip;
    case CSSValueEllipsis:
        return TextOverflow::Ellipsis;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextOverflow::Clip;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextEmphasisFill fill)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (fill) {
    case TextEmphasisFill::Filled:
        m_value.valueID = CSSValueFilled;
        break;
    case TextEmphasisFill::Open:
        m_value.valueID = CSSValueOpen;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextEmphasisFill() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueFilled:
        return TextEmphasisFill::Filled;
    case CSSValueOpen:
        return TextEmphasisFill::Open;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextEmphasisFill::Filled;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextEmphasisMark mark)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (mark) {
    case TextEmphasisMark::Dot:
        m_value.valueID = CSSValueDot;
        break;
    case TextEmphasisMark::Circle:
        m_value.valueID = CSSValueCircle;
        break;
    case TextEmphasisMark::DoubleCircle:
        m_value.valueID = CSSValueDoubleCircle;
        break;
    case TextEmphasisMark::Triangle:
        m_value.valueID = CSSValueTriangle;
        break;
    case TextEmphasisMark::Sesame:
        m_value.valueID = CSSValueSesame;
        break;
    case TextEmphasisMark::None:
    case TextEmphasisMark::Auto:
    case TextEmphasisMark::Custom:
        ASSERT_NOT_REACHED();
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextEmphasisMark() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return TextEmphasisMark::None;
    case CSSValueDot:
        return TextEmphasisMark::Dot;
    case CSSValueCircle:
        return TextEmphasisMark::Circle;
    case CSSValueDoubleCircle:
        return TextEmphasisMark::DoubleCircle;
    case CSSValueTriangle:
        return TextEmphasisMark::Triangle;
    case CSSValueSesame:
        return TextEmphasisMark::Sesame;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextEmphasisMark::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextOrientation e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextOrientation::Sideways:
        m_value.valueID = CSSValueSideways;
        break;
    case TextOrientation::Mixed:
        m_value.valueID = CSSValueMixed;
        break;
    case TextOrientation::Upright:
        m_value.valueID = CSSValueUpright;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextOrientation() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSideways:
        return TextOrientation::Sideways;
    case CSSValueSidewaysRight:
        return TextOrientation::Sideways;
    case CSSValueVerticalRight:
        return TextOrientation::Mixed;
    case CSSValueMixed:
        return TextOrientation::Mixed;
    case CSSValueUpright:
        return TextOrientation::Upright;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextOrientation::Mixed;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(PointerEvents e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case PointerEvents::None:
        m_value.valueID = CSSValueNone;
        break;
    case PointerEvents::Stroke:
        m_value.valueID = CSSValueStroke;
        break;
    case PointerEvents::Fill:
        m_value.valueID = CSSValueFill;
        break;
    case PointerEvents::Painted:
        m_value.valueID = CSSValuePainted;
        break;
    case PointerEvents::Visible:
        m_value.valueID = CSSValueVisible;
        break;
    case PointerEvents::VisibleStroke:
        m_value.valueID = CSSValueVisibleStroke;
        break;
    case PointerEvents::VisibleFill:
        m_value.valueID = CSSValueVisibleFill;
        break;
    case PointerEvents::VisiblePainted:
        m_value.valueID = CSSValueVisiblePainted;
        break;
    case PointerEvents::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case PointerEvents::All:
        m_value.valueID = CSSValueAll;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator PointerEvents() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAll:
        return PointerEvents::All;
    case CSSValueAuto:
        return PointerEvents::Auto;
    case CSSValueNone:
        return PointerEvents::None;
    case CSSValueVisiblePainted:
        return PointerEvents::VisiblePainted;
    case CSSValueVisibleFill:
        return PointerEvents::VisibleFill;
    case CSSValueVisibleStroke:
        return PointerEvents::VisibleStroke;
    case CSSValueVisible:
        return PointerEvents::Visible;
    case CSSValuePainted:
        return PointerEvents::Painted;
    case CSSValueFill:
        return PointerEvents::Fill;
    case CSSValueStroke:
        return PointerEvents::Stroke;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PointerEvents::All;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Kerning kerning)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (kerning) {
    case Kerning::Auto:
        m_value.valueID = CSSValueAuto;
        return;
    case Kerning::Normal:
        m_value.valueID = CSSValueNormal;
        return;
    case Kerning::NoShift:
        m_value.valueID = CSSValueNone;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueAuto;
}

template<> inline CSSPrimitiveValue::operator Kerning() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return Kerning::Auto;
    case CSSValueNormal:
        return Kerning::Normal;
    case CSSValueNone:
        return Kerning::NoShift;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Kerning::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ObjectFit fit)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (fit) {
    case ObjectFit::Fill:
        m_value.valueID = CSSValueFill;
        break;
    case ObjectFit::Contain:
        m_value.valueID = CSSValueContain;
        break;
    case ObjectFit::Cover:
        m_value.valueID = CSSValueCover;
        break;
    case ObjectFit::None:
        m_value.valueID = CSSValueNone;
        break;
    case ObjectFit::ScaleDown:
        m_value.valueID = CSSValueScaleDown;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ObjectFit() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueFill:
        return ObjectFit::Fill;
    case CSSValueContain:
        return ObjectFit::Contain;
    case CSSValueCover:
        return ObjectFit::Cover;
    case CSSValueNone:
        return ObjectFit::None;
    case CSSValueScaleDown:
        return ObjectFit::ScaleDown;
    default:
        ASSERT_NOT_REACHED();
        return ObjectFit::Fill;
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontSmoothingMode smoothing)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (smoothing) {
    case FontSmoothingMode::AutoSmoothing:
        m_value.valueID = CSSValueAuto;
        return;
    case FontSmoothingMode::NoSmoothing:
        m_value.valueID = CSSValueNone;
        return;
    case FontSmoothingMode::Antialiased:
        m_value.valueID = CSSValueAntialiased;
        return;
    case FontSmoothingMode::SubpixelAntialiased:
        m_value.valueID = CSSValueSubpixelAntialiased;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueAuto;
}

template<> inline CSSPrimitiveValue::operator FontSmoothingMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return FontSmoothingMode::AutoSmoothing;
    case CSSValueNone:
        return FontSmoothingMode::NoSmoothing;
    case CSSValueAntialiased:
        return FontSmoothingMode::Antialiased;
    case CSSValueSubpixelAntialiased:
        return FontSmoothingMode::SubpixelAntialiased;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FontSmoothingMode::AutoSmoothing;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontSmallCaps smallCaps)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (smallCaps) {
    case FontSmallCaps::Off:
        m_value.valueID = CSSValueNormal;
        return;
    case FontSmallCaps::On:
        m_value.valueID = CSSValueSmallCaps;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueNormal;
}

template<> inline CSSPrimitiveValue::operator FontSmallCaps() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSmallCaps:
        return FontSmallCaps::On;
    case CSSValueNormal:
        return FontSmallCaps::Off;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontSmallCaps::Off;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextRenderingMode e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextRenderingMode::AutoTextRendering:
        m_value.valueID = CSSValueAuto;
        break;
    case TextRenderingMode::OptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case TextRenderingMode::OptimizeLegibility:
        m_value.valueID = CSSValueOptimizeLegibility;
        break;
    case TextRenderingMode::GeometricPrecision:
        m_value.valueID = CSSValueGeometricPrecision;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextRenderingMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextRenderingMode::AutoTextRendering;
    case CSSValueOptimizeSpeed:
        return TextRenderingMode::OptimizeSpeed;
    case CSSValueOptimizeLegibility:
        return TextRenderingMode::OptimizeLegibility;
    case CSSValueGeometricPrecision:
        return TextRenderingMode::GeometricPrecision;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextRenderingMode::AutoTextRendering;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Hyphens hyphens)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (hyphens) {
    case Hyphens::None:
        m_value.valueID = CSSValueNone;
        break;
    case Hyphens::Manual:
        m_value.valueID = CSSValueManual;
        break;
    case Hyphens::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Hyphens() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return Hyphens::None;
    case CSSValueManual:
        return Hyphens::Manual;
    case CSSValueAuto:
        return Hyphens::Auto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Hyphens::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineSnap gridSnap)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (gridSnap) {
    case LineSnap::None:
        m_value.valueID = CSSValueNone;
        break;
    case LineSnap::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case LineSnap::Contain:
        m_value.valueID = CSSValueContain;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator LineSnap() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return LineSnap::None;
    case CSSValueBaseline:
        return LineSnap::Baseline;
    case CSSValueContain:
        return LineSnap::Contain;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LineSnap::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineAlign lineAlign)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (lineAlign) {
    case LineAlign::None:
        m_value.valueID = CSSValueNone;
        break;
    case LineAlign::Edges:
        m_value.valueID = CSSValueEdges;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator LineAlign() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return LineAlign::None;
    case CSSValueEdges:
        return LineAlign::Edges;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LineAlign::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(SpeakAs e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case SpeakAs::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case SpeakAs::SpellOut:
        m_value.valueID = CSSValueSpellOut;
        break;
    case SpeakAs::Digits:
        m_value.valueID = CSSValueDigits;
        break;
    case SpeakAs::LiteralPunctuation:
        m_value.valueID = CSSValueLiteralPunctuation;
        break;
    case SpeakAs::NoPunctuation:
        m_value.valueID = CSSValueNoPunctuation;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator Order() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueLogical:
        return Order::Logical;
    case CSSValueVisual:
        return Order::Visual;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Order::Logical;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Order e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case Order::Logical:
        m_value.valueID = CSSValueLogical;
        break;
    case Order::Visual:
        m_value.valueID = CSSValueVisual;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator OptionSet<SpeakAs>() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return OptionSet<SpeakAs> { };
    case CSSValueSpellOut:
        return SpeakAs::SpellOut;
    case CSSValueDigits:
        return SpeakAs::Digits;
    case CSSValueLiteralPunctuation:
        return SpeakAs::LiteralPunctuation;
    case CSSValueNoPunctuation:
        return SpeakAs::NoPunctuation;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return OptionSet<SpeakAs> { };
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BlendMode blendMode)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (blendMode) {
    case BlendMode::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case BlendMode::Multiply:
        m_value.valueID = CSSValueMultiply;
        break;
    case BlendMode::Screen:
        m_value.valueID = CSSValueScreen;
        break;
    case BlendMode::Overlay:
        m_value.valueID = CSSValueOverlay;
        break;
    case BlendMode::Darken:
        m_value.valueID = CSSValueDarken;
        break;
    case BlendMode::Lighten:
        m_value.valueID = CSSValueLighten;
        break;
    case BlendMode::ColorDodge:
        m_value.valueID = CSSValueColorDodge;
        break;
    case BlendMode::ColorBurn:
        m_value.valueID = CSSValueColorBurn;
        break;
    case BlendMode::HardLight:
        m_value.valueID = CSSValueHardLight;
        break;
    case BlendMode::SoftLight:
        m_value.valueID = CSSValueSoftLight;
        break;
    case BlendMode::Difference:
        m_value.valueID = CSSValueDifference;
        break;
    case BlendMode::Exclusion:
        m_value.valueID = CSSValueExclusion;
        break;
    case BlendMode::Hue:
        m_value.valueID = CSSValueHue;
        break;
    case BlendMode::Saturation:
        m_value.valueID = CSSValueSaturation;
        break;
    case BlendMode::Color:
        m_value.valueID = CSSValueColor;
        break;
    case BlendMode::Luminosity:
        m_value.valueID = CSSValueLuminosity;
        break;
    case BlendMode::PlusDarker:
        m_value.valueID = CSSValuePlusDarker;
        break;
    case BlendMode::PlusLighter:
        m_value.valueID = CSSValuePlusLighter;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BlendMode() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return BlendMode::Normal;
    case CSSValueMultiply:
        return BlendMode::Multiply;
    case CSSValueScreen:
        return BlendMode::Screen;
    case CSSValueOverlay:
        return BlendMode::Overlay;
    case CSSValueDarken:
        return BlendMode::Darken;
    case CSSValueLighten:
        return BlendMode::Lighten;
    case CSSValueColorDodge:
        return BlendMode::ColorDodge;
    case CSSValueColorBurn:
        return BlendMode::ColorBurn;
    case CSSValueHardLight:
        return BlendMode::HardLight;
    case CSSValueSoftLight:
        return BlendMode::SoftLight;
    case CSSValueDifference:
        return BlendMode::Difference;
    case CSSValueExclusion:
        return BlendMode::Exclusion;
    case CSSValueHue:
        return BlendMode::Hue;
    case CSSValueSaturation:
        return BlendMode::Saturation;
    case CSSValueColor:
        return BlendMode::Color;
    case CSSValueLuminosity:
        return BlendMode::Luminosity;
    case CSSValuePlusDarker:
        return BlendMode::PlusDarker;
    case CSSValuePlusLighter:
        return BlendMode::PlusLighter;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BlendMode::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Isolation isolation)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (isolation) {
    case Isolation::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case Isolation::Isolate:
        m_value.valueID = CSSValueIsolate;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

template<> inline CSSPrimitiveValue::operator Isolation() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return Isolation::Auto;
    case CSSValueIsolate:
        return Isolation::Isolate;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Isolation::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineCap e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ButtCap:
        m_value.valueID = CSSValueButt;
        break;
    case RoundCap:
        m_value.valueID = CSSValueRound;
        break;
    case SquareCap:
        m_value.valueID = CSSValueSquare;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator LineCap() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueButt:
        return ButtCap;
    case CSSValueRound:
        return RoundCap;
    case CSSValueSquare:
        return SquareCap;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ButtCap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineJoin e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case MiterJoin:
        m_value.valueID = CSSValueMiter;
        break;
    case RoundJoin:
        m_value.valueID = CSSValueRound;
        break;
    case BevelJoin:
        m_value.valueID = CSSValueBevel;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator LineJoin() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueMiter:
        return MiterJoin;
    case CSSValueRound:
        return RoundJoin;
    case CSSValueBevel:
        return BevelJoin;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MiterJoin;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WindRule e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case WindRule::NonZero:
        m_value.valueID = CSSValueNonzero;
        break;
    case WindRule::EvenOdd:
        m_value.valueID = CSSValueEvenodd;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator WindRule() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNonzero:
        return WindRule::NonZero;
    case CSSValueEvenodd:
        return WindRule::EvenOdd;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return WindRule::NonZero;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(AlignmentBaseline e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case AlignmentBaseline::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case AlignmentBaseline::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case AlignmentBaseline::BeforeEdge:
        m_value.valueID = CSSValueBeforeEdge;
        break;
    case AlignmentBaseline::TextBeforeEdge:
        m_value.valueID = CSSValueTextBeforeEdge;
        break;
    case AlignmentBaseline::Middle:
        m_value.valueID = CSSValueMiddle;
        break;
    case AlignmentBaseline::Central:
        m_value.valueID = CSSValueCentral;
        break;
    case AlignmentBaseline::AfterEdge:
        m_value.valueID = CSSValueAfterEdge;
        break;
    case AlignmentBaseline::TextAfterEdge:
        m_value.valueID = CSSValueTextAfterEdge;
        break;
    case AlignmentBaseline::Ideographic:
        m_value.valueID = CSSValueIdeographic;
        break;
    case AlignmentBaseline::Alphabetic:
        m_value.valueID = CSSValueAlphabetic;
        break;
    case AlignmentBaseline::Hanging:
        m_value.valueID = CSSValueHanging;
        break;
    case AlignmentBaseline::Mathematical:
        m_value.valueID = CSSValueMathematical;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator AlignmentBaseline() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return AlignmentBaseline::Auto;
    case CSSValueBaseline:
        return AlignmentBaseline::Baseline;
    case CSSValueBeforeEdge:
        return AlignmentBaseline::BeforeEdge;
    case CSSValueTextBeforeEdge:
        return AlignmentBaseline::TextBeforeEdge;
    case CSSValueMiddle:
        return AlignmentBaseline::Middle;
    case CSSValueCentral:
        return AlignmentBaseline::Central;
    case CSSValueAfterEdge:
        return AlignmentBaseline::AfterEdge;
    case CSSValueTextAfterEdge:
        return AlignmentBaseline::TextAfterEdge;
    case CSSValueIdeographic:
        return AlignmentBaseline::Ideographic;
    case CSSValueAlphabetic:
        return AlignmentBaseline::Alphabetic;
    case CSSValueHanging:
        return AlignmentBaseline::Hanging;
    case CSSValueMathematical:
        return AlignmentBaseline::Mathematical;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return AlignmentBaseline::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BorderCollapse e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BorderCollapse::Separate:
        m_value.valueID = CSSValueSeparate;
        break;
    case BorderCollapse::Collapse:
        m_value.valueID = CSSValueCollapse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BorderCollapse() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSeparate:
        return BorderCollapse::Separate;
    case CSSValueCollapse:
        return BorderCollapse::Collapse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BorderCollapse::Separate;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BorderFit e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BorderFit::Border:
        m_value.valueID = CSSValueBorder;
        break;
    case BorderFit::Lines:
        m_value.valueID = CSSValueLines;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BorderFit() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBorder:
        return BorderFit::Border;
    case CSSValueLines:
        return BorderFit::Lines;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BorderFit::Lines;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ImageRendering imageRendering)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (imageRendering) {
    case ImageRendering::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ImageRendering::CrispEdges:
        m_value.valueID = CSSValueCrispEdges;
        break;
    case ImageRendering::Pixelated:
        m_value.valueID = CSSValuePixelated;
        break;
    case ImageRendering::OptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case ImageRendering::OptimizeQuality:
        m_value.valueID = CSSValueOptimizeQuality;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ImageRendering() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return ImageRendering::Auto;
    case CSSValueWebkitOptimizeContrast:
    case CSSValueCrispEdges:
    case CSSValueWebkitCrispEdges:
        return ImageRendering::CrispEdges;
    case CSSValuePixelated:
        return ImageRendering::Pixelated;
    case CSSValueOptimizeSpeed:
        return ImageRendering::OptimizeSpeed;
    case CSSValueOptimizeQuality:
        return ImageRendering::OptimizeQuality;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ImageRendering::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TransformStyle3D e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TransformStyle3D::Flat:
        m_value.valueID = CSSValueFlat;
        break;
    case TransformStyle3D::Preserve3D:
        m_value.valueID = CSSValuePreserve3d;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TransformStyle3D() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueFlat:
        return TransformStyle3D::Flat;
    case CSSValuePreserve3d:
        return TransformStyle3D::Preserve3D;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TransformStyle3D::Flat;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TransformBox box)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (box) {
    case TransformBox::BorderBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case TransformBox::FillBox:
        m_value.valueID = CSSValueFillBox;
        break;
    case TransformBox::ViewBox:
        m_value.valueID = CSSValueViewBox;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TransformBox() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueBorderBox:
        return TransformBox::BorderBox;
    case CSSValueFillBox:
        return TransformBox::FillBox;
    case CSSValueViewBox:
        return TransformBox::ViewBox;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TransformBox::BorderBox;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnAxis e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ColumnAxis::Horizontal:
        m_value.valueID = CSSValueHorizontal;
        break;
    case ColumnAxis::Vertical:
        m_value.valueID = CSSValueVertical;
        break;
    case ColumnAxis::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColumnAxis() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueHorizontal:
        return ColumnAxis::Horizontal;
    case CSSValueVertical:
        return ColumnAxis::Vertical;
    case CSSValueAuto:
        return ColumnAxis::Auto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ColumnAxis::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnProgression e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ColumnProgression::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case ColumnProgression::Reverse:
        m_value.valueID = CSSValueReverse;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColumnProgression() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return ColumnProgression::Normal;
    case CSSValueReverse:
        return ColumnProgression::Reverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ColumnProgression::Normal;
}

enum LengthConversion {
    AnyConversion = ~0,
    FixedIntegerConversion = 1 << 0,
    FixedFloatConversion = 1 << 1,
    AutoConversion = 1 << 2,
    PercentConversion = 1 << 3,
    CalculatedConversion = 1 << 4
};

inline bool CSSPrimitiveValue::convertingToLengthRequiresNonNullStyle(int lengthConversion) const
{
    ASSERT(isFontRelativeLength());
    // This matches the implementation in CSSPrimitiveValue::computeLengthDouble().
    switch (m_primitiveUnitType) {
    case CSS_EMS:
    case CSS_EXS:
    case CSS_CHS:
        return lengthConversion & (FixedIntegerConversion | FixedFloatConversion);
    default:
        return false;
    }
}

template<int supported> Length CSSPrimitiveValue::convertToLength(const CSSToLengthConversionData& conversionData) const
{
    if (isFontRelativeLength() && convertingToLengthRequiresNonNullStyle(supported) && !conversionData.style())
        return Length(Undefined);
    if ((supported & FixedIntegerConversion) && isLength())
        return computeLength<Length>(conversionData);
    if ((supported & FixedFloatConversion) && isLength())
        return Length(computeLength<double>(conversionData), Fixed);
    if ((supported & PercentConversion) && isPercentage())
        return Length(doubleValue(), Percent);
    if ((supported & AutoConversion) && valueID() == CSSValueAuto)
        return Length(Auto);
    if ((supported & CalculatedConversion) && isCalculated())
        return Length(cssCalcValue()->createCalculationValue(conversionData));
    return Length(Undefined);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BufferedRendering e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case BufferedRendering::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case BufferedRendering::Dynamic:
        m_value.valueID = CSSValueDynamic;
        break;
    case BufferedRendering::Static:
        m_value.valueID = CSSValueStatic;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator BufferedRendering() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return BufferedRendering::Auto;
    case CSSValueDynamic:
        return BufferedRendering::Dynamic;
    case CSSValueStatic:
        return BufferedRendering::Static;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BufferedRendering::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColorInterpolation e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ColorInterpolation::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ColorInterpolation::SRGB:
        m_value.valueID = CSSValueSRGB;
        break;
    case ColorInterpolation::LinearRGB:
        m_value.valueID = CSSValueLinearRGB;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColorInterpolation() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueSRGB:
        return ColorInterpolation::SRGB;
    case CSSValueLinearRGB:
        return ColorInterpolation::LinearRGB;
    case CSSValueAuto:
        return ColorInterpolation::Auto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ColorInterpolation::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColorRendering e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ColorRendering::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ColorRendering::OptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case ColorRendering::OptimizeQuality:
        m_value.valueID = CSSValueOptimizeQuality;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ColorRendering() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueOptimizeSpeed:
        return ColorRendering::OptimizeSpeed;
    case CSSValueOptimizeQuality:
        return ColorRendering::OptimizeQuality;
    case CSSValueAuto:
        return ColorRendering::Auto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ColorRendering::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(DominantBaseline e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case DominantBaseline::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case DominantBaseline::UseScript:
        m_value.valueID = CSSValueUseScript;
        break;
    case DominantBaseline::NoChange:
        m_value.valueID = CSSValueNoChange;
        break;
    case DominantBaseline::ResetSize:
        m_value.valueID = CSSValueResetSize;
        break;
    case DominantBaseline::Central:
        m_value.valueID = CSSValueCentral;
        break;
    case DominantBaseline::Middle:
        m_value.valueID = CSSValueMiddle;
        break;
    case DominantBaseline::TextBeforeEdge:
        m_value.valueID = CSSValueTextBeforeEdge;
        break;
    case DominantBaseline::TextAfterEdge:
        m_value.valueID = CSSValueTextAfterEdge;
        break;
    case DominantBaseline::Ideographic:
        m_value.valueID = CSSValueIdeographic;
        break;
    case DominantBaseline::Alphabetic:
        m_value.valueID = CSSValueAlphabetic;
        break;
    case DominantBaseline::Hanging:
        m_value.valueID = CSSValueHanging;
        break;
    case DominantBaseline::Mathematical:
        m_value.valueID = CSSValueMathematical;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator DominantBaseline() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return DominantBaseline::Auto;
    case CSSValueUseScript:
        return DominantBaseline::UseScript;
    case CSSValueNoChange:
        return DominantBaseline::NoChange;
    case CSSValueResetSize:
        return DominantBaseline::ResetSize;
    case CSSValueIdeographic:
        return DominantBaseline::Ideographic;
    case CSSValueAlphabetic:
        return DominantBaseline::Alphabetic;
    case CSSValueHanging:
        return DominantBaseline::Hanging;
    case CSSValueMathematical:
        return DominantBaseline::Mathematical;
    case CSSValueCentral:
        return DominantBaseline::Central;
    case CSSValueMiddle:
        return DominantBaseline::Middle;
    case CSSValueTextAfterEdge:
        return DominantBaseline::TextAfterEdge;
    case CSSValueTextBeforeEdge:
        return DominantBaseline::TextBeforeEdge;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return DominantBaseline::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ShapeRendering e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ShapeRendering::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ShapeRendering::OptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case ShapeRendering::CrispEdges:
        m_value.valueID = CSSValueCrispedges;
        break;
    case ShapeRendering::GeometricPrecision:
        m_value.valueID = CSSValueGeometricPrecision;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ShapeRendering() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueAuto:
        return ShapeRendering::Auto;
    case CSSValueOptimizeSpeed:
        return ShapeRendering::OptimizeSpeed;
    case CSSValueCrispedges:
        return ShapeRendering::CrispEdges;
    case CSSValueGeometricPrecision:
        return ShapeRendering::GeometricPrecision;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ShapeRendering::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextAnchor e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TextAnchor::Start:
        m_value.valueID = CSSValueStart;
        break;
    case TextAnchor::Middle:
        m_value.valueID = CSSValueMiddle;
        break;
    case TextAnchor::End:
        m_value.valueID = CSSValueEnd;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TextAnchor() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueStart:
        return TextAnchor::Start;
    case CSSValueMiddle:
        return TextAnchor::Middle;
    case CSSValueEnd:
        return TextAnchor::End;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextAnchor::Start;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(const Color& color)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_RGBCOLOR;
    m_value.color = new Color(color);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CSSFontFamily fontFamily)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_FONT_FAMILY;
    m_value.fontFamily = new CSSFontFamily(WTFMove(fontFamily));
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(VectorEffect e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case VectorEffect::None:
        m_value.valueID = CSSValueNone;
        break;
    case VectorEffect::NonScalingStroke:
        m_value.valueID = CSSValueNonScalingStroke;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator VectorEffect() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNone:
        return VectorEffect::None;
    case CSSValueNonScalingStroke:
        return VectorEffect::NonScalingStroke;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return VectorEffect::None;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(MaskType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case MaskType::Luminance:
        m_value.valueID = CSSValueLuminance;
        break;
    case MaskType::Alpha:
        m_value.valueID = CSSValueAlpha;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator MaskType() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueLuminance:
        return MaskType::Luminance;
    case CSSValueAlpha:
        return MaskType::Alpha;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MaskType::Luminance;
}

#if ENABLE(CSS_IMAGE_ORIENTATION)

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ImageOrientationEnum e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_DEG;
    switch (e) {
    case OriginTopLeft:
        m_value.num = 0;
        break;
    case OriginRightTop:
        m_value.num = 90;
        break;
    case OriginBottomRight:
        m_value.num = 180;
        break;
    case OriginLeftBottom:
        m_value.num = 270;
        break;
    case OriginTopRight:
    case OriginLeftTop:
    case OriginBottomLeft:
    case OriginRightBottom:
        ASSERT_NOT_REACHED();
    }
}

template<> inline CSSPrimitiveValue::operator ImageOrientationEnum() const
{
    ASSERT(isAngle());
    double quarters = 4 * doubleValue(CSS_TURN);
    int orientation = 3 & static_cast<int>(quarters < 0 ? floor(quarters) : ceil(quarters));
    switch (orientation) {
    case 0:
        return OriginTopLeft;
    case 1:
        return OriginRightTop;
    case 2:
        return OriginBottomRight;
    case 3:
        return OriginLeftBottom;
    }

    ASSERT_NOT_REACHED();
    return OriginTopLeft;
}

#endif // ENABLE(CSS_IMAGE_ORIENTATION)

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CSSBoxType cssBox)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (cssBox) {
    case CSSBoxType::MarginBox:
        m_value.valueID = CSSValueMarginBox;
        break;
    case CSSBoxType::BorderBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case CSSBoxType::PaddingBox:
        m_value.valueID = CSSValuePaddingBox;
        break;
    case CSSBoxType::ContentBox:
        m_value.valueID = CSSValueContentBox;
        break;
    case CSSBoxType::FillBox:
        m_value.valueID = CSSValueFillBox;
        break;
    case CSSBoxType::StrokeBox:
        m_value.valueID = CSSValueStrokeBox;
        break;
    case CSSBoxType::ViewBox:
        m_value.valueID = CSSValueViewBox;
        break;
    case CSSBoxType::BoxMissing:
        ASSERT_NOT_REACHED();
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator CSSBoxType() const
{
    switch (valueID()) {
    case CSSValueMarginBox:
        return CSSBoxType::MarginBox;
    case CSSValueBorderBox:
        return CSSBoxType::BorderBox;
    case CSSValuePaddingBox:
        return CSSBoxType::PaddingBox;
    case CSSValueContentBox:
        return CSSBoxType::ContentBox;
    // The following are used in an SVG context.
    case CSSValueFillBox:
        return CSSBoxType::FillBox;
    case CSSValueStrokeBox:
        return CSSBoxType::StrokeBox;
    case CSSValueViewBox:
        return CSSBoxType::ViewBox;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return CSSBoxType::BoxMissing;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ItemPosition itemPosition)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (itemPosition) {
    case ItemPosition::Legacy:
        m_value.valueID = CSSValueLegacy;
        break;
    case ItemPosition::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case ItemPosition::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case ItemPosition::Stretch:
        m_value.valueID = CSSValueStretch;
        break;
    case ItemPosition::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case ItemPosition::LastBaseline:
        m_value.valueID = CSSValueLastBaseline;
        break;
    case ItemPosition::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case ItemPosition::Start:
        m_value.valueID = CSSValueStart;
        break;
    case ItemPosition::End:
        m_value.valueID = CSSValueEnd;
        break;
    case ItemPosition::SelfStart:
        m_value.valueID = CSSValueSelfStart;
        break;
    case ItemPosition::SelfEnd:
        m_value.valueID = CSSValueSelfEnd;
        break;
    case ItemPosition::FlexStart:
        m_value.valueID = CSSValueFlexStart;
        break;
    case ItemPosition::FlexEnd:
        m_value.valueID = CSSValueFlexEnd;
        break;
    case ItemPosition::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case ItemPosition::Right:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ItemPosition() const
{
    switch (m_value.valueID) {
    case CSSValueLegacy:
        return ItemPosition::Legacy;
    case CSSValueAuto:
        return ItemPosition::Auto;
    case CSSValueNormal:
        return ItemPosition::Normal;
    case CSSValueStretch:
        return ItemPosition::Stretch;
    case CSSValueBaseline:
        return ItemPosition::Baseline;
    case CSSValueFirstBaseline:
        return ItemPosition::Baseline;
    case CSSValueLastBaseline:
        return ItemPosition::LastBaseline;
    case CSSValueCenter:
        return ItemPosition::Center;
    case CSSValueStart:
        return ItemPosition::Start;
    case CSSValueEnd:
        return ItemPosition::End;
    case CSSValueSelfStart:
        return ItemPosition::SelfStart;
    case CSSValueSelfEnd:
        return ItemPosition::SelfEnd;
    case CSSValueFlexStart:
        return ItemPosition::FlexStart;
    case CSSValueFlexEnd:
        return ItemPosition::FlexEnd;
    case CSSValueLeft:
        return ItemPosition::Left;
    case CSSValueRight:
        return ItemPosition::Right;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ItemPosition::Auto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(OverflowAlignment overflowAlignment)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (overflowAlignment) {
    case OverflowAlignment::Default:
        m_value.valueID = CSSValueDefault;
        break;
    case OverflowAlignment::Unsafe:
        m_value.valueID = CSSValueUnsafe;
        break;
    case OverflowAlignment::Safe:
        m_value.valueID = CSSValueSafe;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator OverflowAlignment() const
{
    switch (m_value.valueID) {
    case CSSValueUnsafe:
        return OverflowAlignment::Unsafe;
    case CSSValueSafe:
        return OverflowAlignment::Safe;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return OverflowAlignment::Unsafe;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ContentPosition contentPosition)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (contentPosition) {
    case ContentPosition::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case ContentPosition::Baseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case ContentPosition::LastBaseline:
        m_value.valueID = CSSValueLastBaseline;
        break;
    case ContentPosition::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case ContentPosition::Start:
        m_value.valueID = CSSValueStart;
        break;
    case ContentPosition::End:
        m_value.valueID = CSSValueEnd;
        break;
    case ContentPosition::FlexStart:
        m_value.valueID = CSSValueFlexStart;
        break;
    case ContentPosition::FlexEnd:
        m_value.valueID = CSSValueFlexEnd;
        break;
    case ContentPosition::Left:
        m_value.valueID = CSSValueLeft;
        break;
    case ContentPosition::Right:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ContentPosition() const
{
    switch (m_value.valueID) {
    case CSSValueNormal:
        return ContentPosition::Normal;
    case CSSValueBaseline:
        return ContentPosition::Baseline;
    case CSSValueFirstBaseline:
        return ContentPosition::Baseline;
    case CSSValueLastBaseline:
        return ContentPosition::LastBaseline;
    case CSSValueCenter:
        return ContentPosition::Center;
    case CSSValueStart:
        return ContentPosition::Start;
    case CSSValueEnd:
        return ContentPosition::End;
    case CSSValueFlexStart:
        return ContentPosition::FlexStart;
    case CSSValueFlexEnd:
        return ContentPosition::FlexEnd;
    case CSSValueLeft:
        return ContentPosition::Left;
    case CSSValueRight:
        return ContentPosition::Right;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContentPosition::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ContentDistribution contentDistribution)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (contentDistribution) {
    case ContentDistribution::Default:
        m_value.valueID = CSSValueDefault;
        break;
    case ContentDistribution::SpaceBetween:
        m_value.valueID = CSSValueSpaceBetween;
        break;
    case ContentDistribution::SpaceAround:
        m_value.valueID = CSSValueSpaceAround;
        break;
    case ContentDistribution::SpaceEvenly:
        m_value.valueID = CSSValueSpaceEvenly;
        break;
    case ContentDistribution::Stretch:
        m_value.valueID = CSSValueStretch;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ContentDistribution() const
{
    switch (m_value.valueID) {
    case CSSValueSpaceBetween:
        return ContentDistribution::SpaceBetween;
    case CSSValueSpaceAround:
        return ContentDistribution::SpaceAround;
    case CSSValueSpaceEvenly:
        return ContentDistribution::SpaceEvenly;
    case CSSValueStretch:
        return ContentDistribution::Stretch;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContentDistribution::Stretch;
}

template<> inline CSSPrimitiveValue::operator TextZoom() const
{
    ASSERT(isValueID());

    switch (m_value.valueID) {
    case CSSValueNormal:
        return TextZoom::Normal;
    case CSSValueReset:
        return TextZoom::Reset;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextZoom::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextZoom textZoom)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (textZoom) {
    case TextZoom::Normal:
        m_value.valueID = CSSValueNormal;
        return;
    case TextZoom::Reset:
        m_value.valueID = CSSValueReset;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueNormal;
}

#if ENABLE(POINTER_EVENTS)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TouchAction touchAction)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (touchAction) {
    case TouchAction::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case TouchAction::Manipulation:
        m_value.valueID = CSSValueManipulation;
        break;
    case TouchAction::None:
        m_value.valueID = CSSValueNone;
        break;
    case TouchAction::PanX:
        m_value.valueID = CSSValuePanX;
        break;
    case TouchAction::PanY:
        m_value.valueID = CSSValuePanY;
        break;
    case TouchAction::PinchZoom:
        m_value.valueID = CSSValuePinchZoom;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator OptionSet<TouchAction>() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return TouchAction::Auto;
    case CSSValueManipulation:
        return TouchAction::Manipulation;
    case CSSValueNone:
        return TouchAction::None;
    case CSSValuePanX:
        return TouchAction::PanX;
    case CSSValuePanY:
        return TouchAction::PanY;
    case CSSValuePinchZoom:
        return TouchAction::PinchZoom;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return TouchAction::Auto;
}
#endif

#if ENABLE(CSS_SCROLL_SNAP)

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ScrollSnapStrictness strictness)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (strictness) {
    case ScrollSnapStrictness::None:
        m_value.valueID = CSSValueNone;
        break;
    case ScrollSnapStrictness::Proximity:
        m_value.valueID = CSSValueProximity;
        break;
    case ScrollSnapStrictness::Mandatory:
        m_value.valueID = CSSValueMandatory;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ScrollSnapStrictness() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return ScrollSnapStrictness::None;
    case CSSValueProximity:
        return ScrollSnapStrictness::Proximity;
    case CSSValueMandatory:
        return ScrollSnapStrictness::Mandatory;
    default:
        ASSERT_NOT_REACHED();
        return ScrollSnapStrictness::None;
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ScrollSnapAxis axis)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (axis) {
    case ScrollSnapAxis::XAxis:
        m_value.valueID = CSSValueX;
        break;
    case ScrollSnapAxis::YAxis:
        m_value.valueID = CSSValueY;
        break;
    case ScrollSnapAxis::Block:
        m_value.valueID = CSSValueBlock;
        break;
    case ScrollSnapAxis::Inline:
        m_value.valueID = CSSValueInline;
        break;
    case ScrollSnapAxis::Both:
        m_value.valueID = CSSValueBoth;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ScrollSnapAxis() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueX:
        return ScrollSnapAxis::XAxis;
    case CSSValueY:
        return ScrollSnapAxis::YAxis;
    case CSSValueBlock:
        return ScrollSnapAxis::Block;
    case CSSValueInline:
        return ScrollSnapAxis::Inline;
    case CSSValueBoth:
        return ScrollSnapAxis::Both;
    default:
        ASSERT_NOT_REACHED();
        return ScrollSnapAxis::Both;
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ScrollSnapAxisAlignType type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (type) {
    case ScrollSnapAxisAlignType::None:
        m_value.valueID = CSSValueNone;
        break;
    case ScrollSnapAxisAlignType::Start:
        m_value.valueID = CSSValueStart;
        break;
    case ScrollSnapAxisAlignType::Center:
        m_value.valueID = CSSValueCenter;
        break;
    case ScrollSnapAxisAlignType::End:
        m_value.valueID = CSSValueEnd;
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ScrollSnapAxisAlignType() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return ScrollSnapAxisAlignType::None;
    case CSSValueStart:
        return ScrollSnapAxisAlignType::Start;
    case CSSValueCenter:
        return ScrollSnapAxisAlignType::Center;
    case CSSValueEnd:
        return ScrollSnapAxisAlignType::End;
    default:
        ASSERT_NOT_REACHED();
        return ScrollSnapAxisAlignType::None;
    }
}

#endif

#if ENABLE(CSS_TRAILING_WORD)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TrailingWord e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case TrailingWord::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case TrailingWord::PartiallyBalanced:
        m_value.valueID = CSSValueWebkitPartiallyBalanced;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator TrailingWord() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return TrailingWord::Auto;
    case CSSValueWebkitPartiallyBalanced:
        return TrailingWord::PartiallyBalanced;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return TrailingWord::Auto;
}
#endif

#if ENABLE(APPLE_PAY)
template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ApplePayButtonStyle e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ApplePayButtonStyle::White:
        m_value.valueID = CSSValueWhite;
        break;
    case ApplePayButtonStyle::WhiteOutline:
        m_value.valueID = CSSValueWhiteOutline;
        break;
    case ApplePayButtonStyle::Black:
        m_value.valueID = CSSValueBlack;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ApplePayButtonStyle() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueWhite:
        return ApplePayButtonStyle::White;
    case CSSValueWhiteOutline:
        return ApplePayButtonStyle::WhiteOutline;
    case CSSValueBlack:
        return ApplePayButtonStyle::Black;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ApplePayButtonStyle::Black;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ApplePayButtonType e)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (e) {
    case ApplePayButtonType::Plain:
        m_value.valueID = CSSValuePlain;
        break;
    case ApplePayButtonType::Buy:
        m_value.valueID = CSSValueBuy;
        break;
    case ApplePayButtonType::SetUp:
        m_value.valueID = CSSValueSetUp;
        break;
    case ApplePayButtonType::Donate:
        m_value.valueID = CSSValueDonate;
        break;
#if ENABLE(APPLE_PAY_SESSION_V4)
    case ApplePayButtonType::CheckOut:
        m_value.valueID = CSSValueCheckOut;
        break;
    case ApplePayButtonType::Book:
        m_value.valueID = CSSValueBook;
        break;
    case ApplePayButtonType::Subscribe:
        m_value.valueID = CSSValueSubscribe;
        break;
#endif

    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator ApplePayButtonType() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValuePlain:
        return ApplePayButtonType::Plain;
    case CSSValueBuy:
        return ApplePayButtonType::Buy;
    case CSSValueSetUp:
        return ApplePayButtonType::SetUp;
    case CSSValueDonate:
        return ApplePayButtonType::Donate;
#if ENABLE(APPLE_PAY_SESSION_V4)
    case CSSValueCheckOut:
        return ApplePayButtonType::CheckOut;
    case CSSValueBook:
        return ApplePayButtonType::Book;
    case CSSValueSubscribe:
        return ApplePayButtonType::Subscribe;
#endif
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ApplePayButtonType::Plain;
}
#endif

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontVariantPosition position)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (position) {
    case FontVariantPosition::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case FontVariantPosition::Subscript:
        m_value.valueID = CSSValueSub;
        break;
    case FontVariantPosition::Superscript:
        m_value.valueID = CSSValueSuper;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FontVariantPosition() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return FontVariantPosition::Normal;
    case CSSValueSub:
        return FontVariantPosition::Subscript;
    case CSSValueSuper:
        return FontVariantPosition::Superscript;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontVariantPosition::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontVariantCaps caps)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (caps) {
    case FontVariantCaps::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case FontVariantCaps::Small:
        m_value.valueID = CSSValueSmallCaps;
        break;
    case FontVariantCaps::AllSmall:
        m_value.valueID = CSSValueAllSmallCaps;
        break;
    case FontVariantCaps::Petite:
        m_value.valueID = CSSValuePetiteCaps;
        break;
    case FontVariantCaps::AllPetite:
        m_value.valueID = CSSValueAllPetiteCaps;
        break;
    case FontVariantCaps::Unicase:
        m_value.valueID = CSSValueUnicase;
        break;
    case FontVariantCaps::Titling:
        m_value.valueID = CSSValueTitlingCaps;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FontVariantCaps() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return FontVariantCaps::Normal;
    case CSSValueSmallCaps:
        return FontVariantCaps::Small;
    case CSSValueAllSmallCaps:
        return FontVariantCaps::AllSmall;
    case CSSValuePetiteCaps:
        return FontVariantCaps::Petite;
    case CSSValueAllPetiteCaps:
        return FontVariantCaps::AllPetite;
    case CSSValueUnicase:
        return FontVariantCaps::Unicase;
    case CSSValueTitlingCaps:
        return FontVariantCaps::Titling;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontVariantCaps::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontVariantAlternates alternates)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (alternates) {
    case FontVariantAlternates::Normal:
        m_value.valueID = CSSValueNormal;
        break;
    case FontVariantAlternates::HistoricalForms:
        m_value.valueID = CSSValueHistoricalForms;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FontVariantAlternates() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return FontVariantAlternates::Normal;
    case CSSValueHistoricalForms:
        return FontVariantAlternates::HistoricalForms;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontVariantAlternates::Normal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontOpticalSizing sizing)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (sizing) {
    case FontOpticalSizing::Enabled:
        m_value.valueID = CSSValueAuto;
        break;
    case FontOpticalSizing::Disabled:
        m_value.valueID = CSSValueNone;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FontOpticalSizing() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return FontOpticalSizing::Enabled;
    case CSSValueNone:
        return FontOpticalSizing::Disabled;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontOpticalSizing::Enabled;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontLoadingBehavior behavior)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    switch (behavior) {
    case FontLoadingBehavior::Auto:
        m_value.valueID = CSSValueAuto;
        break;
    case FontLoadingBehavior::Block:
        m_value.valueID = CSSValueBlock;
        break;
    case FontLoadingBehavior::Swap:
        m_value.valueID = CSSValueSwap;
        break;
    case FontLoadingBehavior::Fallback:
        m_value.valueID = CSSValueFallback;
        break;
    case FontLoadingBehavior::Optional:
        m_value.valueID = CSSValueOptional;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CSSPrimitiveValue::operator FontLoadingBehavior() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return FontLoadingBehavior::Auto;
    case CSSValueBlock:
        return FontLoadingBehavior::Block;
    case CSSValueSwap:
        return FontLoadingBehavior::Swap;
    case CSSValueFallback:
        return FontLoadingBehavior::Fallback;
    case CSSValueOptional:
        return FontLoadingBehavior::Optional;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontLoadingBehavior::Auto;
}

}

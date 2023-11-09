/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(APPLE_PAY)
#include "ApplePayButtonAppearance.h"
#endif
#include "ButtonAppearance.h"
#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorWellAppearance.h"
#endif
#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsButtonAppearance.h"
#endif
#include "InnerSpinButtonAppearance.h"
#include "MenuListAppearance.h"
#include "MenuListButtonAppearance.h"
#include "MeterAppearance.h"
#include "PlatformControl.h"
#include "ProgressBarAppearance.h"
#include "SearchFieldAppearance.h"
#include "SearchFieldCancelButtonAppearance.h"
#include "SearchFieldResultsAppearance.h"
#include "SliderThumbAppearance.h"
#include "SliderTrackAppearance.h"
#include "SwitchThumbAppearance.h"
#include "SwitchTrackAppearance.h"
#include "TextAreaAppearance.h"
#include "TextFieldAppearance.h"
#include "ToggleButtonAppearance.h"

namespace WebCore {

using ControlAppearance = std::variant<
#if ENABLE(APPLE_PAY)
    ApplePayButtonAppearance,
#endif
ButtonAppearance,
#if ENABLE(INPUT_TYPE_COLOR)
    ColorWellAppearance,
#endif
    DefaultButtonAppearance,
#if ENABLE(SERVICE_CONTROLS)
    ImageControlsButtonAppearance,
#endif
    InnerSpinButtonAppearance,
    MenuListButtonAppearance,
    MenuListAppearance,
    MeterAppearance,
    ProgressBarAppearance,
    PushButtonAppearance,
    SearchFieldCancelButtonAppearance,
    SearchFieldAppearance,
    SearchFieldResultsButtonAppearance,
    SearchFieldResultsDecorationAppearance,
    SliderThumbHorizontalAppearance,
    SliderThumbVerticalAppearance,
    SliderTrackHorizontalAppearance,
    SliderTrackVerticalAppearance,
    SquareButtonAppearance,
    ListBoxAppearance,
    TextAreaAppearance,
    TextFieldAppearance,
    CheckboxAppearance,
    RadioAppearance,
    SwitchThumbAppearance,
    SwitchTrackAppearance
>;

} // namespace WebCore

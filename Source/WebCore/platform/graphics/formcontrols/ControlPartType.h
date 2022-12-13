/*
 * Copyright (C) 2008-2022 Apple Inc. All Rights Reserved.
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

#include <wtf/EnumTraits.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

// Must follow CSSValueKeywords.in order
enum class ControlPartType : uint8_t {
    NoControl,
    Auto,
    Checkbox,
    Radio,
    PushButton,
    SquareButton,
    Button,
    DefaultButton,
    Listbox,
    Menulist,
    MenulistButton,
    Meter,
    ProgressBar,
    SliderHorizontal,
    SliderVertical,
    SearchField,
#if ENABLE(APPLE_PAY)
    ApplePayButton,
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    Attachment,
    BorderlessAttachment,
#endif
    TextArea,
    TextField,
    // Internal-only Values
    CapsLockIndicator,
#if ENABLE(INPUT_TYPE_COLOR)
    ColorWell,
#endif
#if ENABLE(SERVICE_CONTROLS)
    ImageControlsButton,
#endif
    InnerSpinButton,
#if ENABLE(DATALIST_ELEMENT)
    ListButton,
#endif
    SearchFieldDecoration,
    SearchFieldResultsDecoration,
    SearchFieldResultsButton,
    SearchFieldCancelButton,
    SliderThumbHorizontal,
    SliderThumbVertical
};

WTF::TextStream& operator<<(WTF::TextStream&, ControlPartType);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ControlPartType> {
    using values = EnumValues<
        WebCore::ControlPartType,

        WebCore::ControlPartType::NoControl,
        WebCore::ControlPartType::Auto,
        WebCore::ControlPartType::Checkbox,
        WebCore::ControlPartType::Radio,
        WebCore::ControlPartType::PushButton,
        WebCore::ControlPartType::SquareButton,
        WebCore::ControlPartType::Button,
        WebCore::ControlPartType::DefaultButton,
        WebCore::ControlPartType::Listbox,
        WebCore::ControlPartType::Menulist,
        WebCore::ControlPartType::MenulistButton,
        WebCore::ControlPartType::Meter,
        WebCore::ControlPartType::ProgressBar,
        WebCore::ControlPartType::SliderHorizontal,
        WebCore::ControlPartType::SliderVertical,
        WebCore::ControlPartType::SearchField,
#if ENABLE(APPLE_PAY)
        WebCore::ControlPartType::ApplePayButton,
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
        WebCore::ControlPartType::Attachment,
        WebCore::ControlPartType::BorderlessAttachment,
#endif
        WebCore::ControlPartType::TextArea,
        WebCore::ControlPartType::TextField,
        WebCore::ControlPartType::CapsLockIndicator,
#if ENABLE(INPUT_TYPE_COLOR)
        WebCore::ControlPartType::ColorWell,
#endif
#if ENABLE(SERVICE_CONTROLS)
        WebCore::ControlPartType::ImageControlsButton,
#endif
        WebCore::ControlPartType::InnerSpinButton,
#if ENABLE(DATALIST_ELEMENT)
        WebCore::ControlPartType::ListButton,
#endif
        WebCore::ControlPartType::SearchFieldDecoration,
        WebCore::ControlPartType::SearchFieldResultsDecoration,
        WebCore::ControlPartType::SearchFieldResultsButton,
        WebCore::ControlPartType::SearchFieldCancelButton,
        WebCore::ControlPartType::SliderThumbHorizontal,
        WebCore::ControlPartType::SliderThumbVertical
    >;
};

} // namespace WTF

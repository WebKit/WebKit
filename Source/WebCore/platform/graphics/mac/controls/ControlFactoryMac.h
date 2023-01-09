/*
 * Copyright (C) 2022-2023 Apple Inc. All Rights Reserved.
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

#if PLATFORM(MAC)

#import "ControlFactory.h"
#import "WebControlView.h"

namespace WebCore {

class ControlFactoryMac final : public ControlFactory {
public:
    using ControlFactory::ControlFactory;

    NSView *drawingView(const FloatRect&, const ControlStyle&) const;

private:
    std::unique_ptr<PlatformControl> createPlatformButton(ButtonPart&) final;
#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<PlatformControl> createPlatformColorWell(ColorWellPart&) final;
#endif
    std::unique_ptr<PlatformControl> createPlatformInnerSpinButton(InnerSpinButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMenuList(MenuListPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMenuListButton(MenuListButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMeter(MeterPart&) final;
    std::unique_ptr<PlatformControl> createPlatformProgressBar(ProgressBarPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSearchField(SearchFieldPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSliderThumb(SliderThumbPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSliderTrack(SliderTrackPart&) final;
    std::unique_ptr<PlatformControl> createPlatformTextArea(TextAreaPart&) final;
    std::unique_ptr<PlatformControl> createPlatformTextField(TextFieldPart&) final;
    std::unique_ptr<PlatformControl> createPlatformToggleButton(ToggleButtonPart&) final;

    NSButtonCell *buttonCell() const;
    NSButtonCell *defaultButtonCell() const;
    NSButtonCell *checkboxCell() const;
    NSButtonCell *radioCell() const;
    NSLevelIndicatorCell *levelIndicatorCell() const;
    NSPopUpButtonCell *popUpButtonCell() const;
    NSSearchFieldCell *searchFieldCell() const;
    NSSliderCell *sliderCell() const;
    NSTextFieldCell *textFieldCell() const;

    mutable RetainPtr<WebControlView> m_drawingView;

    mutable RetainPtr<NSButtonCell> m_buttonCell;
    mutable RetainPtr<NSButtonCell> m_defaultButtonCell;
    mutable RetainPtr<NSButtonCell> m_checkboxCell;
    mutable RetainPtr<NSButtonCell> m_radioCell;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicatorCell;
    mutable RetainPtr<NSPopUpButtonCell> m_popUpButtonCell;
    mutable RetainPtr<NSSearchFieldCell> m_searchFieldCell;
    mutable RetainPtr<NSSliderCell> m_sliderCell;
    mutable RetainPtr<NSTextFieldCell> m_textFieldCell;
};

} // namespace WebCore

#endif // PLATFORM(MAC)

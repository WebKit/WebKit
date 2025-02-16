/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "ControlFactoryAdwaita.h"

#include "ButtonControlAdwaita.h"
#include "ButtonPart.h"
#include "ColorWellPart.h"
#include "InnerSpinButtonAdwaita.h"
#include "InnerSpinButtonPart.h"
#include "MenuListAdwaita.h"
#include "MenuListButtonPart.h"
#include "MenuListPart.h"
#include "NotImplemented.h"
#include "ProgressBarAdwaita.h"
#include "SearchFieldPart.h"
#include "SliderThumbAdwaita.h"
#include "SliderThumbPart.h"
#include "SliderTrackAdwaita.h"
#include "TextAreaPart.h"
#include "TextFieldAdwaita.h"
#include "TextFieldPart.h"
#include "ToggleButtonAdwaita.h"
#include "ToggleButtonPart.h"


#if USE(THEME_ADWAITA)

namespace WebCore {
using namespace WebCore::Adwaita;

RefPtr<ControlFactory> ControlFactory::create()
{
    return adoptRef(new ControlFactoryAdwaita());
}

ControlFactoryAdwaita& ControlFactoryAdwaita::shared()
{
    return static_cast<ControlFactoryAdwaita&>(ControlFactory::shared());
}

#if ENABLE(APPLE_PAY)
std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformApplePayButton(ApplePayButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformButton(ButtonPart& part)
{
    return makeUnique<ButtonControlAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformColorWell(ColorWellPart& part)
{
    return makeUnique<ButtonControlAdwaita>(part, *this);
}

#if ENABLE(SERVICE_CONTROLS)
std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformImageControlsButton(ImageControlsButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformInnerSpinButton(InnerSpinButtonPart& part)
{
    return makeUnique<InnerSpinButtonAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformMenuList(MenuListPart& part)
{
    return makeUnique<MenuListAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformMenuListButton(MenuListButtonPart& part)
{
    return makeUnique<MenuListAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformMeter(MeterPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformProgressBar(ProgressBarPart& part)
{
    return makeUnique<ProgressBarAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSearchField(SearchFieldPart& part)
{
    return makeUnique<TextFieldAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSearchFieldResults(SearchFieldResultsPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSliderThumb(SliderThumbPart& part)
{
    return makeUnique<SliderThumbAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSliderTrack(SliderTrackPart& part)
{
    return makeUnique<SliderTrackAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSwitchThumb(SwitchThumbPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformSwitchTrack(SwitchTrackPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformTextArea(TextAreaPart& part)
{
    return makeUnique<TextFieldAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformTextField(TextFieldPart& part)
{
    return makeUnique<TextFieldAdwaita>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryAdwaita::createPlatformToggleButton(ToggleButtonPart& part)
{
    return makeUnique<ToggleButtonAdwaita>(part, *this);
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)

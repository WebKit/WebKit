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

#import "config.h"
#import "ControlFactoryIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "NotImplemented.h"

namespace WebCore {

std::unique_ptr<ControlFactory> ControlFactory::createControlFactory()
{
    return makeUnique<ControlFactoryIOS>();
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformButton(ButtonPart&)
{
    notImplemented();
    return nullptr;
}

#if ENABLE(INPUT_TYPE_COLOR)
std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformColorWell(ColorWellPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformInnerSpinButton(InnerSpinButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformMenuList(MenuListPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformMenuListButton(MenuListButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformMeter(MeterPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformProgressBar(ProgressBarPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformSearchField(SearchFieldPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformSearchFieldResults(SearchFieldResultsPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformSliderThumb(SliderThumbPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformSliderTrack(SliderTrackPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformTextArea(TextAreaPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformTextField(TextFieldPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryIOS::createPlatformToggleButton(ToggleButtonPart&)
{
    notImplemented();
    return nullptr;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)

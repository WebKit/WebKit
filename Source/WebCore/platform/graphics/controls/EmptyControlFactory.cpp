/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EmptyControlFactory.h"

#include "NotImplemented.h"
#include "PlatformControl.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyControlFactory);

#if ENABLE(APPLE_PAY)
std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformApplePayButton(ApplePayButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformButton(ButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformColorWell(ColorWellPart&)
{
    notImplemented();
    return nullptr;
}

#if ENABLE(SERVICE_CONTROLS)
std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformImageControlsButton(ImageControlsButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformInnerSpinButton(InnerSpinButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformMenuList(MenuListPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformMenuListButton(MenuListButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformMeter(MeterPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformProgressBar(ProgressBarPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSearchField(SearchFieldPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSearchFieldResults(SearchFieldResultsPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSliderThumb(SliderThumbPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSliderTrack(SliderTrackPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSwitchThumb(SwitchThumbPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformSwitchTrack(SwitchTrackPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformTextArea(TextAreaPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformTextField(TextFieldPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> EmptyControlFactory::createPlatformToggleButton(ToggleButtonPart&)
{
    notImplemented();
    return nullptr;
}

} // namespace WebCore

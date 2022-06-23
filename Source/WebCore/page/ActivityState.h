/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/OptionSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

struct ActivityState {
    enum Flag {
        WindowIsActive = 1 << 0,
        IsFocused = 1 << 1,
        IsVisible = 1 << 2,
        IsVisibleOrOccluded = 1 << 3,
        IsInWindow = 1 << 4,
        IsVisuallyIdle = 1 << 5,
        IsAudible = 1 << 6,
        IsLoading = 1 << 7,
        IsCapturingMedia = 1 << 8,
        IsConnectedToHardwareConsole = 1 << 9,
    };

    static constexpr OptionSet<Flag> allFlags() { return { WindowIsActive, IsFocused, IsVisible, IsVisibleOrOccluded, IsInWindow, IsVisuallyIdle, IsAudible, IsLoading, IsCapturingMedia, IsConnectedToHardwareConsole }; }
};

enum class ActivityStateForCPUSampling {
    NonVisible,
    VisibleNonActive,
    VisibleAndActive
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, OptionSet<ActivityState::Flag>);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ActivityState::Flag> {
    using values = EnumValues<
        WebCore::ActivityState::Flag,
        WebCore::ActivityState::Flag::WindowIsActive,
        WebCore::ActivityState::Flag::IsFocused,
        WebCore::ActivityState::Flag::IsVisible,
        WebCore::ActivityState::Flag::IsVisibleOrOccluded,
        WebCore::ActivityState::Flag::IsInWindow,
        WebCore::ActivityState::Flag::IsVisuallyIdle,
        WebCore::ActivityState::Flag::IsAudible,
        WebCore::ActivityState::Flag::IsLoading,
        WebCore::ActivityState::Flag::IsCapturingMedia,
        WebCore::ActivityState::Flag::IsConnectedToHardwareConsole
    >;
};

} // namespace WTF

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>

namespace WebKit {

enum class GestureType : uint8_t {
    Loupe,
    OneFingerTap,
    TapAndAHalf,
    DoubleTap,
    OneFingerDoubleTap,
    OneFingerTripleTap,
    TwoFingerSingleTap,
    PhraseBoundary
};

enum class SelectionTouch : uint8_t {
    Started,
    Moved,
    Ended,
    EndedMovingForward,
    EndedMovingBackward,
    EndedNotMoving
};

enum class GestureRecognizerState : uint8_t {
    Possible,
    Began,
    Changed,
    Ended,
    Cancelled,
    Failed
};

enum class SheetAction : uint8_t {
    Copy,
    SaveImage,
    PauseAnimation,
    PlayAnimation
};

enum SelectionFlags : uint8_t {
    WordIsNearTap = 1 << 0,
    SelectionFlipped = 1 << 1,
    PhraseBoundaryChanged = 1 << 2,
};

enum class RespectSelectionAnchor : bool {
    No,
    Yes
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::GestureRecognizerState> {
    using values = EnumValues<
        WebKit::GestureRecognizerState,
        WebKit::GestureRecognizerState::Possible,
        WebKit::GestureRecognizerState::Began,
        WebKit::GestureRecognizerState::Changed,
        WebKit::GestureRecognizerState::Ended,
        WebKit::GestureRecognizerState::Cancelled,
        WebKit::GestureRecognizerState::Failed
    >;
};

template<> struct EnumTraits<WebKit::GestureType> {
    using values = EnumValues<
        WebKit::GestureType,
        WebKit::GestureType::Loupe,
        WebKit::GestureType::OneFingerTap,
        WebKit::GestureType::TapAndAHalf,
        WebKit::GestureType::DoubleTap,
        WebKit::GestureType::OneFingerDoubleTap,
        WebKit::GestureType::OneFingerTripleTap,
        WebKit::GestureType::TwoFingerSingleTap,
        WebKit::GestureType::PhraseBoundary
    >;
};

template<> struct EnumTraits<WebKit::SelectionFlags> {
    using values = EnumValues<
        WebKit::SelectionFlags,
        WebKit::SelectionFlags::WordIsNearTap,
        WebKit::SelectionFlags::SelectionFlipped,
        WebKit::SelectionFlags::PhraseBoundaryChanged
    >;
};

template<> struct EnumTraits<WebKit::SelectionTouch> {
    using values = EnumValues<
        WebKit::SelectionTouch,
        WebKit::SelectionTouch::Started,
        WebKit::SelectionTouch::Moved,
        WebKit::SelectionTouch::Ended,
        WebKit::SelectionTouch::EndedMovingForward,
        WebKit::SelectionTouch::EndedMovingBackward,
        WebKit::SelectionTouch::EndedNotMoving
    >;
};

} // namespace WTF

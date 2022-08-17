/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScrollLogicalPosition.h"

namespace WebCore {

ScrollAlignment toScrollAlignmentForInlineDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode, bool isLTR)
{
    switch (position.value_or(ScrollLogicalPosition::Nearest)) {
    case ScrollLogicalPosition::Start: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
        case WritingMode::BottomToTop: {
            return isLTR ? ScrollAlignment::alignLeftAlways : ScrollAlignment::alignRightAlways;
        }
        case WritingMode::LeftToRight:
        case WritingMode::RightToLeft: {
            return isLTR ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignBottomAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignLeftAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
        case WritingMode::BottomToTop: {
            return isLTR ? ScrollAlignment::alignRightAlways : ScrollAlignment::alignLeftAlways;
        }
        case WritingMode::LeftToRight:
        case WritingMode::RightToLeft: {
            return isLTR ? ScrollAlignment::alignBottomAlways : ScrollAlignment::alignTopAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignRightAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

ScrollAlignment toScrollAlignmentForBlockDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode)
{
    switch (position.value_or(ScrollLogicalPosition::Start)) {
    case ScrollLogicalPosition::Start: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
            return ScrollAlignment::alignTopAlways;
        case WritingMode::BottomToTop:
            return ScrollAlignment::alignBottomAlways;
        case WritingMode::LeftToRight:
            return ScrollAlignment::alignLeftAlways;
        case WritingMode::RightToLeft:
            return ScrollAlignment::alignRightAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignTopAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
            return ScrollAlignment::alignBottomAlways;
        case WritingMode::BottomToTop:
            return ScrollAlignment::alignTopAlways;
        case WritingMode::LeftToRight:
            return ScrollAlignment::alignRightAlways;
        case WritingMode::RightToLeft:
            return ScrollAlignment::alignLeftAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignBottomAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

}

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

#ifndef WKGestureTypes_h
#define WKGestureTypes_h

namespace WebKit {

typedef enum {
    WKGestureLoupe,
    WKGestureOneFingerTap,
    WKGestureTapAndAHalf,
    WKGestureDoubleTap,
    WKGestureTapAndHalf,
    WKGestureDoubleTapInUneditable,
    WKGestureOneFingerTapInUneditable,
    WKGestureOneFingerTapSelectsAll,
    WKGestureOneFingerDoubleTap,
    WKGestureOneFingerTripleTap,
    WKGestureTwoFingerSingleTap,
    WKGestureTwoFingerRangedSelectGesture,
    WKGestureTapOnLinkWithGesture,
    WKGestureMakeWebSelection
} WKGestureType;

typedef enum {
    WKSelectionTouchStarted,
    WKSelectionTouchMoved,
    WKSelectionTouchEnded,
    WKSelectionTouchEndedMovingForward,
    WKSelectionTouchEndedMovingBackward,
    WKSelectionTouchEndedNotMoving
} WKSelectionTouch;

typedef enum {
    WKGestureRecognizerStatePossible,
    WKGestureRecognizerStateBegan,
    WKGestureRecognizerStateChanged,
    WKGestureRecognizerStateEnded,
    WKGestureRecognizerStateCancelled,
    WKGestureRecognizerStateFailed,
    WKGestureRecognizerStateRecognized = WKGestureRecognizerStateEnded
} WKGestureRecognizerState;

typedef enum {
    WKSheetActionCopy,
    WKSheetActionSaveImage
} WKSheetActions;

typedef enum {
    WKNone = 0,
    WKWordIsNearTap = 1,
    WKIsBlockSelection = 2
} WKSelectionFlags;

typedef enum {
    WKHandleTop,
    WKHandleRight,
    WKHandleBottom,
    WKHandleLeft
} WKHandlePosition;
} // namespace WebKit

#endif // WKGestureTypes_h

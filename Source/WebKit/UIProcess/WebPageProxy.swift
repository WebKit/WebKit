// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)

import Foundation
internal import WebKit_Internal
internal import WebCore_Private

extension WebKit.WebPageProxy {
    @MainActor
    func selectWithGesture(
        at point: WebCore.IntPoint,
        type: WebKit.GestureType,
        state: WebKit.GestureRecognizerState,
        isInteractingWithFocusedElement: Bool
    ) async {
        await withCheckedContinuation { continuation in
            selectWithGesture(
                point,
                type,
                state,
                isInteractingWithFocusedElement,
                consuming: .init({ _, _, _, _ in continuation.resume() }, WTF.ThreadLikeAssertion(WTF.CurrentThreadLike()))
            )
        }
    }

    @MainActor
    func selectPosition(at point: WebCore.IntPoint, isInteractingWithFocusedElement: Bool) async {
        await withCheckedContinuation { continuation in
            selectPositionAtPoint(
                point,
                isInteractingWithFocusedElement,
                consuming: .init({ continuation.resume() }, WTF.ThreadLikeAssertion(WTF.CurrentThreadLike()))
            )
        }
    }

    @MainActor
    func selectText(
        at point: WebCore.IntPoint,
        by granularity: WebCore.TextGranularity,
        isInteractingWithFocusedElement: Bool
    ) async {
        await withCheckedContinuation { continuation in
            selectTextWithGranularityAtPoint(
                point,
                granularity,
                isInteractingWithFocusedElement,
                consuming: .init({ continuation.resume() }, WTF.ThreadLikeAssertion(WTF.CurrentThreadLike()))
            )
        }
    }

    @MainActor
    @discardableResult
    func updateSelection(
        withExtentPoint point: WebCore.IntPoint,
        by granularity: WebCore.TextGranularity,
        isInteractingWithFocusedElement: Bool,
        source: WebKit.TextInteractionSource,
    ) async -> Bool {
        await withCheckedContinuation { continuation in
            updateSelectionWithExtentPointAndBoundary(
                point,
                granularity,
                isInteractingWithFocusedElement,
                source,
                consuming: .init({ continuation.resume(returning: $0) }, WTF.ThreadLikeAssertion(WTF.CurrentThreadLike()))
            )
        }
    }

    private borrowing func editorStateCopy() -> WebKit.EditorState {
        unsafe __editorStateUnsafe().pointee
    }

    var editorState: WebKit.EditorState {
        unsafe editorStateCopy()
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)

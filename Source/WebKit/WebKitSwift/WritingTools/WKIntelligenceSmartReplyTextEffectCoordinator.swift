// Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if ENABLE_WRITING_TOOLS

import Foundation
import WebKit
@_spi(Private) import WebKit

#if USE_APPLE_INTERNAL_SDK
@_spiOnly import WritingTools
#else
@_spiOnly import WritingTools_SPI
#endif

#if os(macOS)
#if USE_APPLE_INTERNAL_SDK
@_weakLinked internal import WritingToolsUI_Private._WTTextEffectView
#else
@_weakLinked internal import WritingToolsUI_Private_SPI
#endif // USE_APPLE_INTERNAL_SDK
#endif // os(macOS)

// MARK: Implementation

// The Smart Replies flow is:
//
// 1. WT calls the delegate methods `willBegin...` and `didBegin...`
// 2. WT sends a `.compositionRestart` action to the delegate
// 3. WT creates a text placeholder, and creates a pondering effect themselves to add to the placeholder.
// 4. WT generates the response (which takes some time)
// 5. WT removes the text placeholder
// 6. WT calls the delegate method `didReceiveText...`
//
// Then, for each subsequent questionnaire revision update:
//
// 7. WT sends a `.compositionRestart` action to the delegate
// 8. WT generates the response (which takes some time)
// 9. WT calls the delegate method `didReceiveText...`

@objc
@implementation
extension WKIntelligenceSmartReplyTextEffectCoordinator {
    private struct ReplacementOperationRequest {
        let processedRange: Range<Int>
        let characterDelta: Int
        let operation: (() async -> Void)
    }

    @nonobjc
    final private let delegate: (any WKIntelligenceTextEffectCoordinatorDelegate)
    @nonobjc
    final private lazy var viewManager = IntelligenceTextEffectViewManager(
        source: self,
        contentView: self.delegate.view(forIntelligenceTextEffectCoordinator: self)
    )

    // If there are still pending replacements/animations when the user has accepted or rejected the Writing Tools
    // suggestions, they first need to all be flushed out and invoked so that the state is not incomplete, and then
    // the acceptance/rejection can properly occur.
    @nonobjc
    final private var onFlushCompletion: (() async -> Void)? = nil

    @nonobjc
    final private var processedRangeOffset = 0
    @nonobjc
    final private var contextRange: Range<Int>? = nil

    @nonobjc
    final private var replacementQueue: [ReplacementOperationRequest] = []

    var hasActiveEffects: Bool {
        viewManager.hasActiveEffects
    }

    // The initializer is required to be `public`, but the class itself is `internal` so this is not API.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public required init(delegate: any WKIntelligenceTextEffectCoordinatorDelegate) {
        self.delegate = delegate
    }

    @objc(startAnimationForRange:completion:)
    func startAnimation(for range: NSRange) async {
        self.viewManager.assertPonderingEffectIsInactive()
        self.viewManager.assertReplacementEffectIsInactive()

        guard let contextRange = Range(range) else {
            assertionFailure("Intelligence text effect coordinator: Unable to create Swift.Range from NSRange \(range)")
            return
        }

        let chunk = IntelligenceTextEffectChunk.Pondering(range: contextRange)
        let effect = PlatformIntelligencePonderingTextEffect(chunk: chunk as IntelligenceTextEffectChunk)

        await self.viewManager.setActivePonderingEffect(effect)
    }

    @objc(requestReplacementWithProcessedRange:finished:characterDelta:operation:completion:)
    func requestReplacement(
        withProcessedRange processedRange: NSRange,
        finished: Bool,
        characterDelta: Int,
        operation: @MainActor @Sendable @escaping (@MainActor @Sendable @escaping () -> Void) -> Void
    ) async {
        guard let range = Range(processedRange) else {
            assertionFailure("Intelligence text effect coordinator: Unable to create Swift.Range from NSRange \(processedRange)")
            return
        }

        // The "context range" for a Smart Replies composition session changes each time the text is replaced,
        // since there is new "original" text with each subsequent revision.
        self.contextRange = range

        let asyncBlock = async(operation)
        let request = Self.ReplacementOperationRequest(processedRange: range, characterDelta: characterDelta, operation: asyncBlock)

        self.replacementQueue.append(request)

        if self.replacementQueue.count == 1 {
            await self.startReplacementAnimation(using: request)
        }
    }

    func flushReplacements() async {
        assert(self.onFlushCompletion == nil)

        // If the replacement queue is empty, there's no effects pending completion and nothing to flush,
        // so no need to create a completion block, and instead just invoke `removeActiveEffects` immediately.

        if self.replacementQueue.isEmpty {
            await self.removeActiveEffects()
            return
        }

        // This can't be performed immediately since a replacement animation may be ongoing, and they are not interruptible.
        // So instead, the completion of this async method is stored in state, so that when the current replacement is complete,
        // the actual flush can occur.

        await withCheckedContinuation { continuation in
            self.onFlushCompletion = {
                await self.removeActiveEffects()
                continuation.resume()
            }
        }
    }

    func restoreSelectionAcceptedReplacements(_ acceptedReplacements: Bool) async {
        guard let contextRange = self.contextRange else {
            return
        }

        let range = acceptedReplacements ? contextRange.lowerBound..<(contextRange.upperBound + self.processedRangeOffset) : contextRange
        await self.delegate.intelligenceTextEffectCoordinator(self, setSelectionFor: NSRange(range))
    }

    func hideEffects() async {
        await self.viewManager.hideEffects()
    }

    func showEffects() async {
        await self.viewManager.showEffects()
    }

    @nonobjc
    final private func removeActiveEffects() async {
        await self.viewManager.removeActiveEffects()
    }

    @nonobjc
    final private func startReplacementAnimation(using request: ReplacementOperationRequest) async {
        self.viewManager.assertReplacementEffectIsInactive()

        if !request.processedRange.isEmpty {
            // An artificial pondering effect needs to first be created each time a new response is received.
            // This is because in the platform effect view, when a replacement effect gets created, the underlying text becomes hidden
            // for a non-instantaneous amount of time while the replacement is performed. So, a pondering effect has to be ongoing when
            // this happens to avoid the user from seeing the text become briefly hidden.
            await startAnimation(for: NSRange(request.processedRange))
        }

        let chunk = IntelligenceTextEffectChunk.Replacement(
            range: request.processedRange,
            finished: true, // This is always true for Smart Replies.
            characterDelta: request.characterDelta,
            replacement: request.operation
        )

        let effect = PlatformIntelligenceReplacementTextEffect(chunk: chunk as IntelligenceTextEffectChunk)

        // Start the replacement effect while the pondering effect is still ongoing, so that it can perform
        // the async replacement without it being visible to the user and without any flickering.
        await self.viewManager.setActiveReplacementEffect(effect)

        // Smart Replies always replaces the entire context range, so `processedRangeOffset` is not additive, unlike proofreading/composition.
        self.processedRangeOffset = request.characterDelta
    }
}

// MARK: WKIntelligenceReplacementTextEffectCoordinator + IntelligenceTextEffectViewManager.Delegate conformance

extension WKIntelligenceSmartReplyTextEffectCoordinator: IntelligenceTextEffectViewManagerDelegate {
    func updateTextChunkVisibility(_ chunk: IntelligenceTextEffectChunk, visible: Bool, force: Bool) async {
        if chunk is IntelligenceTextEffectChunk.Pondering && visible && !force {
            // Typically, if `chunk` is part of a pondering effect, this delegate method will get called with `visible == true`
            // once the pondering effect is removed. However, instead of performing that logic here, it is done in `setActivePonderingEffect`
            // instead.
            //
            // This effectively makes this function synchronous in this case.
            return
        }

        guard !self.viewManager.suppressEffectView else {
            // If the effect view is currently suppressed, WTUI will still request making the text not visible since it does not know
            // it is hidden. However, this needs to not happen since the effect view is hidden and the underlying text needs to remain visible.
            return
        }

        await self.delegate.intelligenceTextEffectCoordinator(
            self,
            updateTextVisibilityFor: NSRange(chunk.range),
            visible: visible,
            identifier: chunk.id
        )
    }
}

// MARK: WKIntelligenceReplacementTextEffectCoordinator + PlatformIntelligenceTextEffectViewSource conformance

extension WKIntelligenceSmartReplyTextEffectCoordinator: PlatformIntelligenceTextEffectViewSource {
    func textPreview(for chunk: IntelligenceTextEffectChunk) async -> PlatformTextPreview? {
        let previews = await self.delegate.intelligenceTextEffectCoordinator(self, textPreviewsFor: NSRange(chunk.range))
        return platformTextPreview(from: previews)
    }

    func updateTextChunkVisibility(_ chunk: Chunk, visible: Bool) async {
        await self.updateTextChunkVisibility(chunk, visible: visible, force: false)
    }

    func performReplacementAndGeneratePreview(
        for chunk: IntelligenceTextEffectChunk,
        effect: PlatformIntelligenceReplacementTextEffect<IntelligenceTextEffectChunk>
    ) async -> (PlatformTextPreview?, remainder: PlatformContentPreview?) {
        // FIXME: Implement the remainder text effect for Smart Replies.

        guard let chunk = chunk as? IntelligenceTextEffectChunk.Replacement else {
            fatalError()
        }

        let characterDelta = chunk.characterDelta

        await chunk.replacement()
        chunk.range = chunk.range.lowerBound..<(chunk.range.upperBound + characterDelta)

        let previews = await self.delegate.intelligenceTextEffectCoordinator(self, textPreviewsFor: NSRange(chunk.range))

        let platformPreview = platformTextPreview(from: previews, suggestionRects: [])
        return (platformPreview, remainder: nil)
    }

    func replacementEffectWillBegin(_ effect: PlatformIntelligenceReplacementTextEffect<IntelligenceTextEffectChunk>) async {
        await self.viewManager.setActivePonderingEffect(nil)
    }

    func replacementEffectDidComplete(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async {
        guard effect.chunk is Chunk.Replacement else {
            assertionFailure("Intelligence text effect coordinator: Replacement effect chunk is not Chunk.Replacement")
            return
        }

        await self.viewManager.setActiveReplacementEffect(nil)

        // Perform similar logic as the replacement coordinator with regards to flushing the replacements.

        self.replacementQueue.removeFirst()

        if let next = self.replacementQueue.first {
            await self.startReplacementAnimation(using: next)
        } else if let onFlushCompletion = self.onFlushCompletion {
            await onFlushCompletion()
            self.onFlushCompletion = nil
        } else {
            await self.restoreSelectionAcceptedReplacements(true)
        }
    }
}

// MARK: Misc. helper functions

/// Converts a block with a completion handler into an async block.
private func async(_ block: @MainActor @Sendable @escaping (@MainActor @Sendable @escaping () -> Void) -> Void) -> (() async -> Void) {
    { @MainActor in
        await withCheckedContinuation { continuation in
            block(continuation.resume)
        }
    }
}

#endif // ENABLE_WRITING_TOOLS

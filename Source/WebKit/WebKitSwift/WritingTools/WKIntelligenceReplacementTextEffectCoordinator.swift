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

@objc
@implementation
extension WKIntelligenceReplacementTextEffectCoordinator {
    private struct ReplacementOperationRequest {
        let processedRange: Range<Int>
        let finished: Bool
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

    @nonobjc
    final private var processedRangeOffset = 0
    @nonobjc
    final private var contextRange: Range<Int>? = nil

    // Maintain a replacement operation queue to ensure that no matter how many batches of replacements are received,
    // there is only ever one ongoing effect at a time.
    @nonobjc
    final private var replacementQueue: [ReplacementOperationRequest] = []

    // If there are still pending replacements/animations when the user has accepted or rejected the Writing Tools
    // suggestions, they first need to all be flushed out and invoked so that the state is not incomplete, and then
    // the acceptance/rejection can properly occur.
    @nonobjc
    final private var onFlushCompletion: (() async -> Void)? = nil

    var hasActiveEffects: Bool {
        viewManager.hasActiveEffects
    }

    class func characterDelta(forReceivedSuggestions suggestions: [WTTextSuggestion]) -> Int {
        suggestions.reduce(0) { partialResult, suggestion in
            partialResult + (suggestion.replacement.count - suggestion.originalRange.length)
        }
    }

    // The initializer is required to be `public`, but the class itself is `internal` so this is not API.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public required init(delegate: any WKIntelligenceTextEffectCoordinatorDelegate) {
        self.delegate = delegate
    }

    @objc(startAnimationForRange:completion:)
    func startAnimation(for range: NSRange) async {
        self.reset()

        self.viewManager.assertPonderingEffectIsInactive()
        self.viewManager.assertReplacementEffectIsInactive()

        guard let contextRange = Range(range) else {
            assertionFailure("Intelligence text effect coordinator: Unable to create Swift.Range from NSRange \(range)")
            return
        }

        self.contextRange = contextRange

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

        let asyncBlock = async(operation)
        let request = Self.ReplacementOperationRequest(
            processedRange: range,
            finished: finished,
            characterDelta: characterDelta,
            operation: asyncBlock
        )

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
            assertionFailure()
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
    final private func startReplacementAnimation(
        using request: WKIntelligenceReplacementTextEffectCoordinator.ReplacementOperationRequest
    ) async {
        self.viewManager.assertReplacementEffectIsInactive()

        let processedRange = request.processedRange
        let characterDelta = request.characterDelta

        let processedRangeRelativeToCurrentText =
            (processedRange.lowerBound + self.processedRangeOffset)..<(processedRange.upperBound + self.processedRangeOffset)

        let chunk = IntelligenceTextEffectChunk.Replacement(
            range: processedRangeRelativeToCurrentText,
            finished: request.finished,
            characterDelta: characterDelta,
            replacement: request.operation
        )

        let effect = PlatformIntelligenceReplacementTextEffect(chunk: chunk as IntelligenceTextEffectChunk)

        // Start the replacement effect while the pondering effect is still ongoing, so that it can perform
        // the async replacement without it being visible to the user and without any flickering.
        await self.viewManager.setActiveReplacementEffect(effect)

        self.processedRangeOffset += characterDelta
    }

    @nonobjc
    final private func reset() {
        self.viewManager.reset()

        self.processedRangeOffset = 0
        self.contextRange = nil
        self.replacementQueue = []
    }
}

// MARK: WKIntelligenceReplacementTextEffectCoordinator + IntelligenceTextEffectViewManager.Delegate conformance

extension WKIntelligenceReplacementTextEffectCoordinator: IntelligenceTextEffectViewManagerDelegate {
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

extension WKIntelligenceReplacementTextEffectCoordinator: PlatformIntelligenceTextEffectViewSource {
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
        guard let chunk = chunk as? IntelligenceTextEffectChunk.Replacement else {
            fatalError()
        }

        guard let contextRange = self.contextRange else {
            assertionFailure(
                "Intelligence text effect coordinator: Invariant failed (replacement effect will begin without a context range)"
            )
            return (nil, nil)
        }

        // Create a preview of the original text, covering the range that starts from the end of this replacement chunk to the end
        // of the entire context range, with any prior replacement offsets taken into account.

        let remainderRangeBeforeReplacement = chunk.range.upperBound..<(contextRange.upperBound + self.processedRangeOffset)
        let contentPreview = await self.delegate.intelligenceTextEffectCoordinator(
            self,
            contentPreviewFor: NSRange(remainderRangeBeforeReplacement)
        )
        let remainderPreview = PlatformContentPreview(
            previewImage: contentPreview.previewImage,
            presentationFrame: contentPreview.presentationFrame
        )

        let characterDelta = chunk.characterDelta

        await chunk.replacement()
        chunk.range = chunk.range.lowerBound..<(chunk.range.upperBound + characterDelta)

        // At this point, the resolved end of the entire context range is `contextRange.upperBound + self.processedRangeOffset + characterDelta`
        // until `processedRangeOffset` gets updated after the replacement effect actually starts.

        // If there is an active pondering effect ongoing that predated the replacement, adjust its range to account
        // for the replacement character delta. This ensures that when the pondering effect ends, the semantic chunk
        // remains the same so that the text visibility is restored for the updated range.
        //
        // Additionally, the range's start offset can be truncated to now start at the end of the replacement range,
        // since the range [activePonderingEffect.chunk.range.lowerBound, chunk.range.upperBound] will be covered by
        // the replacement range so there's no need for the pondering effect to also try to affect it.
        self.viewManager.updateActivePonderingEffect { oldPonderingEffectRange in
            chunk.range.upperBound..<(oldPonderingEffectRange.upperBound + characterDelta)
        }

        let previews = await self.delegate.intelligenceTextEffectCoordinator(self, textPreviewsFor: NSRange(chunk.range))

        #if canImport(UIKit)
        let suggestionRects: [NSValue] = []
        #else
        let suggestionRects = await self.delegate.intelligenceTextEffectCoordinator(
            self,
            rectsForProofreadingSuggestionsIn: NSRange(chunk.range)
        )
        #endif

        let platformPreview = platformTextPreview(from: previews, suggestionRects: suggestionRects)
        return (platformPreview, remainder: remainderPreview)
    }

    func replacementEffectWillBegin(_ effect: PlatformIntelligenceReplacementTextEffect<IntelligenceTextEffectChunk>) async {
        guard let chunk = effect.chunk as? IntelligenceTextEffectChunk.Replacement else {
            fatalError()
        }

        guard let contextRange = self.contextRange else {
            assertionFailure(
                "Intelligence text effect coordinator: Invariant failed (replacement effect will begin without a context range)"
            )
            return
        }

        // Hide the remaining text because the remainder text animation will be happening instead.
        //
        // The `upperBound` must include the sum of `self.processedRangeOffset + chunk.offset` because by this point, the
        // backing text has now been replaced, but `self.processedRangeOffset` hasn't been updated yet by `startReplacementAnimation`

        let lowerBound = effect.chunk.range.upperBound
        let upperBound = contextRange.upperBound + self.processedRangeOffset + chunk.characterDelta

        let rangeToHide = lowerBound..<upperBound

        await self.delegate.intelligenceTextEffectCoordinator(
            self,
            updateTextVisibilityFor: NSRange(rangeToHide),
            visible: false,
            identifier: effect.chunk.id
        )

        // Stop the current pondering effect, and then create a new pondering effect once the replacement effect is complete.
        await self.viewManager.setActivePonderingEffect(nil)
    }

    @discardableResult
    private func flushRemainingReplacementsIfNeeded() async -> Bool {
        guard let onFlushCompletion = self.onFlushCompletion else {
            return false
        }

        // Iterate through all replacements in the queue (except for the first one, which will have been completed by this point,
        // but not yet removed from the queue), and immediately apply their operations and update the offset state.

        for request in self.replacementQueue.dropFirst() {
            await request.operation()
            self.processedRangeOffset += request.characterDelta
        }

        await onFlushCompletion()

        self.replacementQueue = []
        self.onFlushCompletion = nil

        return true
    }

    func replacementEffectDidComplete(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async {
        guard let contextRange = self.contextRange else {
            assertionFailure(
                "Intelligence text effect coordinator: Invariant failed (replacement effect completed without a context range)"
            )
            return
        }

        guard let chunk = effect.chunk as? Chunk.Replacement else {
            assertionFailure("Intelligence text effect coordinator: Replacement effect chunk is not Chunk.Replacement")
            return
        }

        // At this point, the text has been replaced, and the effect's chunk's range has been updated to account for the latest character delta.
        let rangeAfterReplacement = chunk.range

        // Inform the coordinator the active replacement effect is over, and then inform the delegate to decorate the replacements if needed.
        await self.viewManager.setActiveReplacementEffect(nil)
        await self.delegate.intelligenceTextEffectCoordinator(self, decorateReplacementsFor: NSRange(rangeAfterReplacement))

        // If this is the last chunk, that means that there will be no subsequent replacements, and no replacements other than
        // this one are in the replacement queue.
        //
        // Therefore, the entire animation is over and the selection can be restored to the context range.

        if chunk.finished {
            self.replacementQueue.removeFirst()

            if let onFlushCompletion = self.onFlushCompletion {
                // At this point, the last chunk has been received, and the session has ended, so the flush completion
                // handler can now be invoked now that the animation is fully complete. There is no need to additionally
                // restore the selection here since the selection would have already been restored when the session ended,
                // if reverted, or the selection should not be restored at all if the session has already ended
                // and has been accepted.

                await onFlushCompletion()
                self.onFlushCompletion = nil
            } else {
                // At this point, the last chunk has been received, and the animation is now over, but the session has
                // yet to end. The selection should be restored assuming it has been accepted; once the session does end,
                // the selection will at that point get adjusted depending on the user's action.

                await self.restoreSelectionAcceptedReplacements(true)
            }

            return
        }

        // Now that the coordinator is in-between replacements, if a flush has previously been requested, flush out
        // all the remaining replacements from the queue as fast as possible and without any effects.
        //
        // If the replacements are flushed, there's no need to continue adding effects for the unprocessed range.

        let didFlush = await self.flushRemainingReplacementsIfNeeded()
        guard !didFlush else {
            return
        }

        // Add a new pondering effect, from the end of the most recently replaced range to the end of the context range, adjusted
        // for the offset relative to the original text.

        let endOfContextRangeRelativeToCurrentText = contextRange.upperBound + self.processedRangeOffset
        let unprocessedRangeChunk = Self.Chunk.Pondering(range: rangeAfterReplacement.upperBound..<endOfContextRangeRelativeToCurrentText)
        let ponderEffectForUnprocessedRange = PlatformIntelligencePonderingTextEffect(chunk: unprocessedRangeChunk as Chunk)

        // When all text has been processed, the unprocessed range will be empty, and no pondering effect need be created.
        if !unprocessedRangeChunk.range.isEmpty {
            await self.viewManager.setActivePonderingEffect(ponderEffectForUnprocessedRange)
        }

        // Now that the first replacement is complete, remove it from the queue, and start the next one in line.

        self.replacementQueue.removeFirst()

        if let next = self.replacementQueue.first {
            await self.startReplacementAnimation(using: next)
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

#if canImport(UIKit)
func platformTextPreview(from source: UITargetedPreview, suggestionRects: [NSValue] = []) -> PlatformTextPreview {
    source
}
#else
func platformTextPreview(from source: [_WKTextPreview], suggestionRects: [NSValue] = []) -> PlatformTextPreview {
    source.map {
        // Misannotated WritingToolsUI symbol which never actually returns `nil`.
        // swift-format-ignore: NeverForceUnwrap
        _WTTextPreview(
            snapshotImage: $0.previewImage,
            presentationFrame: $0.presentationFrame,
            backgroundColor: nil,
            clippingPath: nil,
            scale: 1,
            candidateRects: suggestionRects
        )!
    }
}
#endif

#endif // ENABLE_WRITING_TOOLS

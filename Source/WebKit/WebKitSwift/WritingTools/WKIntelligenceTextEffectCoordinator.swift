//
// Copyright (C) 2024 Apple Inc. All rights reserved.
//

#if canImport(WritingTools)

import Foundation
import WebKit
import WebKitSwift
@_spi(Private) import WebKit
@_spiOnly import WritingTools

#if os(macOS)
@_weakLinked internal import WritingToolsUI_Private._WTTextEffectView
#endif

// MARK: Implementation

@_objcImplementation extension WKIntelligenceTextEffectCoordinator {
    private struct ReplacementOperationRequest {
        let processedRange: Range<Int>
        let finished: Bool
        let characterDelta: Int
        let operation: (() async -> Void)
    }

    @nonobjc final private let delegate: (any WKIntelligenceTextEffectCoordinatorDelegate)
    @nonobjc final private var effectView: PlatformIntelligenceTextEffectView<WKIntelligenceTextEffectCoordinator>? = nil

    @nonobjc final private var processedRangeOffset = 0
    @nonobjc final private var contextRange: Range<Int>? = nil

    // Use the corresponding setter functions instead of setting these directly.
    @nonobjc final private var activePonderingEffect: PlatformIntelligencePonderingTextEffect<Chunk>? = nil
    @nonobjc final private var activeReplacementEffect: PlatformIntelligenceReplacementTextEffect<Chunk>? = nil

    // Maintain a replacement operation queue to ensure that no matter how many batches of replacements are received,
    // there is only ever one ongoing effect at a time.
    @nonobjc final private var replacementQueue: [ReplacementOperationRequest] = []

    // If there are still pending replacements/animations when the user has accepted or rejected the Writing Tools
    // suggestions, they first need to all be flushed out and invoked so that the state is not incomplete, and then
    // the acceptance/rejection can properly occur.
    @nonobjc final private var onFlushCompletion: (() async -> Void)? = nil

    @nonobjc final private var suppressEffectView = false

    @objc(hasActiveEffects)
    public var hasActiveEffects: Bool {
        self.activePonderingEffect != nil || self.activeReplacementEffect != nil
    }

    @objc(characterDeltaForReceivedSuggestions:)
    public class func characterDelta(forReceivedSuggestions suggestions: [WTTextSuggestion]) -> Int {
        suggestions.reduce(0) { partialResult, suggestion in
            partialResult + (suggestion.replacement.count - suggestion.originalRange.length)
        }
    }

    @objc(initWithDelegate:)
    public init(delegate: any WKIntelligenceTextEffectCoordinatorDelegate) {
        self.delegate = delegate
    }

    @objc(startAnimationForRange:completion:)
    public func startAnimation(for range: NSRange) async {
        self.reset()

        assert(self.activePonderingEffect == nil, "Intelligence text effect coordinator: cannot start a new animation while a pondering effect is already active")
        assert(self.activeReplacementEffect == nil, "Intelligence text effect coordinator: cannot start a new animation while a replacement effect is already active")

        guard let contextRange = Range(range) else {
            assertionFailure("Intelligence text effect coordinator: Unable to create Swift.Range from NSRange \(range)")
            return
        }

        self.contextRange = contextRange

        let chunk = Self.Chunk.Pondering(range: contextRange)
        let effect = PlatformIntelligencePonderingTextEffect(chunk: chunk as Chunk)

        await self.setActivePonderingEffect(effect)
    }

    @objc(requestReplacementWithProcessedRange:finished:characterDelta:operation:completion:)
    public func requestReplacement(withProcessedRange processedRange: NSRange, finished: Bool, characterDelta: Int, operation: @escaping (@escaping () -> Void) -> Void) async {
        guard let range = Range(processedRange) else {
            assertionFailure("Intelligence text effect coordinator: Unable to create Swift.Range from NSRange \(processedRange)")
            return
        }

        let asyncBlock = async(operation)
        let request = Self.ReplacementOperationRequest(processedRange: range, finished: finished, characterDelta: characterDelta, operation: asyncBlock)

        self.replacementQueue.append(request)

        if self.replacementQueue.count == 1 {
            await self.startReplacementAnimation(using: request)
        }
    }

    @objc(flushReplacementsWithCompletion:)
    public func flushReplacements() async {
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

    @objc(restoreSelectionAcceptedReplacements:completion:)
    public func restoreSelection(acceptedReplacements: Bool) async {
        guard let contextRange = self.contextRange else {
            assertionFailure()
            return
        }

        let range = acceptedReplacements ? contextRange.lowerBound..<(contextRange.upperBound + self.processedRangeOffset) : contextRange;
        await self.delegate.intelligenceTextEffectCoordinator(self, setSelectionFor: NSRange(range))
    }

    @objc(hideEffectsWithCompletion:)
    public func hideEffects() async {
        guard !self.suppressEffectView else {
            return
        }

        // On macOS, the effect view is incapable of scrolling with the web view. Therefore, the effects need to be suppressed
        // when scrolling, resizing, or zooming.
        //
        // Consequently, since the effects will now be hidden, it is critical that the underlying text visibility is restored to be visible.

        self.suppressEffectView = true

        if let activePonderingEffect = self.activePonderingEffect {
            await self.delegate.intelligenceTextEffectCoordinator(self, updateTextVisibilityFor: NSRange(activePonderingEffect.chunk.range), visible: true, identifier: activePonderingEffect.chunk.id)
        }

        if let activeReplacementEffect = self.activeReplacementEffect {
            await self.delegate.intelligenceTextEffectCoordinator(self, updateTextVisibilityFor: NSRange(activeReplacementEffect.chunk.range), visible: true, identifier: activeReplacementEffect.chunk.id)
        }

        self.effectView?.isHidden = true
    }

    @objc(showEffectsWithCompletion:)
    public func showEffects() async {
        // FIXME: Ideally, after scrolling/resizing/zooming, the effects would be restored and visible.
    }

    @nonobjc final private func removeActiveEffects() async {
        if self.activePonderingEffect != nil {
            await self.setActivePonderingEffect(nil)
        }

        if self.activeReplacementEffect != nil {
            await self.setActiveReplacementEffect(nil)
        }
    }

    @nonobjc final private func startReplacementAnimation(using request: WKIntelligenceTextEffectCoordinator.ReplacementOperationRequest) async {
        assert(self.activeReplacementEffect == nil, "Intelligence text effect coordinator: cannot start a new replacement animation while one is already active")

        let processedRange = request.processedRange
        let characterDelta = request.characterDelta

        let processedRangeRelativeToCurrentText = (processedRange.lowerBound + self.processedRangeOffset)..<(processedRange.upperBound + self.processedRangeOffset)

        let chunk = Self.Chunk.Replacement(
            range: processedRangeRelativeToCurrentText,
            rangeAfterReplacement: processedRangeRelativeToCurrentText.lowerBound..<(processedRangeRelativeToCurrentText.upperBound + characterDelta),
            finished: request.finished,
            replacement: request.operation
        )

        let effect = PlatformIntelligenceReplacementTextEffect(chunk: chunk as Chunk)

        // Start the replacement effect while the pondering effect is still ongoing, so that it can perform
        // the async replacement without it being visible to the user and without any flickering.
        await self.setActiveReplacementEffect(effect)

        self.processedRangeOffset += characterDelta
    }

    @nonobjc final private func setupViewIfNeeded() {
        guard self.effectView == nil else {
            return
        }

        let contentView = self.delegate.view(for: self)
        let effectView = PlatformIntelligenceTextEffectView(source: self)

#if os(iOS)
        effectView.isUserInteractionEnabled = false
        effectView.frame = contentView.frame
        contentView.superview!.addSubview(effectView)
#else
        effectView.frame = contentView.bounds
        contentView.addSubview(effectView)
#endif

        if self.suppressEffectView {
            effectView.isHidden = true
        }

        // UIKit expects subviews of the effect view to be added after the effect view is added to its parent.
        effectView.initializeSubviews()

        self.effectView = effectView
    }

    @nonobjc final private func destroyViewIfNeeded() {
        guard self.activePonderingEffect == nil && self.activeReplacementEffect == nil else {
            return
        }

        self.effectView?.removeFromSuperview()
        self.effectView = nil
    }

    @nonobjc final private func setActivePonderingEffect(_ effect: PlatformIntelligencePonderingTextEffect<Chunk>?) async {
        guard (self.activePonderingEffect == nil && effect != nil) || (self.activePonderingEffect != nil && effect == nil) else {
            assertionFailure("Intelligence text effect coordinator: trying to either set a new pondering effect when there is an ongoing one, or trying to remove an effect when there are none.")
            return
        }

        let oldEffect = self.activePonderingEffect
        self.activePonderingEffect = effect

        if let effect {
            self.setupViewIfNeeded()
            await self.effectView?.addEffect(effect)
        } else {
            // This needs to be manually invoked rather than relying on the associated delegate method being called. This is
            // because when removing an effect, the delegate method is invoked after the effect removal function ends. Invoking
            // this method manually ensures that the replacement effect doesn't start until the visibility has actually changed
            // and this function terminates.
            //
            // Therefore, the delegate method itself must avoid any work so that it can be synchronous, which is what the platform
            // interfaces expect.
            await self.updateTextChunkVisibility(oldEffect!.chunk, visible: true, force: true)

            await self.effectView?.removeEffect(oldEffect!.id)

            self.destroyViewIfNeeded()
        }
    }

    @nonobjc final private func setActiveReplacementEffect(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>?) async {
        guard (self.activeReplacementEffect == nil && effect != nil) || (self.activeReplacementEffect != nil && effect == nil) else {
            assertionFailure("Intelligence text effect coordinator: trying to either set a new replacement effect when there is an ongoing one, or trying to remove an effect when there are none.")
            return
        }

        let oldEffect = self.activeReplacementEffect
        self.activeReplacementEffect = effect

        if let effect {
            self.setupViewIfNeeded()
            await self.effectView?.addEffect(effect)
        } else {
            await self.effectView?.removeEffect(oldEffect!.id)
            self.destroyViewIfNeeded()
        }
    }

    @nonobjc final private func reset() {
        self.effectView?.removeAllEffects()
        self.effectView?.removeFromSuperview()
        self.effectView = nil

        self.processedRangeOffset = 0
        self.contextRange = nil

        self.activePonderingEffect = nil
        self.activeReplacementEffect = nil

        self.replacementQueue = []

        self.suppressEffectView = false
    }
}

// MARK: WKIntelligenceTextEffectCoordinator + PlatformIntelligenceTextEffectViewSource conformance

extension WKIntelligenceTextEffectCoordinator: PlatformIntelligenceTextEffectViewSource {
    func textPreview(for chunk: Chunk) async -> PlatformTextPreview? {
        let previews = await self.delegate.intelligenceTextEffectCoordinator(self, textPreviewsFor: NSRange(chunk.range))

#if canImport(UIKit)
        return previews
#else
        return previews.map {
            _WTTextPreview(snapshotImage: $0.previewImage, presentationFrame: $0.presentationFrame)
        }
#endif
    }

    private func updateTextChunkVisibility(_ chunk: Chunk, visible: Bool, force: Bool) async {
        if chunk is Chunk.Pondering && visible && !force {
            // Typically, if `chunk` is part of a pondering effect, this delegate method will get called with `visible == true`
            // once the pondering effect is removed. However, instead of performing that logic here, it is done in `setActivePonderingEffect`
            // instead.
            //
            // This effectively makes this function synchronous in this case.
            return
        }

        guard !self.suppressEffectView else {
            // If the effect view is currently suppressed, WTUI will still request making the text not visible since it does not know
            // it is hidden. However, this needs to not happen since the effect view is hidden and the underlying text needs to remain visible.
            return
        }

        await self.delegate.intelligenceTextEffectCoordinator(self, updateTextVisibilityFor: NSRange(chunk.range), visible: visible, identifier: chunk.id)
    }

    func updateTextChunkVisibility(_ chunk: Chunk, visible: Bool) async {
        await self.updateTextChunkVisibility(chunk, visible: visible, force: false)
    }

    func performReplacementAndGeneratePreview(for chunk: Chunk, effect: PlatformIntelligenceReplacementTextEffect<Chunk>, animation: PlatformIntelligenceReplacementTextEffect<Chunk>.AnimationParameters) async -> PlatformTextPreview? {
        guard let chunk = chunk as? Chunk.Replacement else {
            fatalError()
        }

        let characterDelta = chunk.rangeAfterReplacement.upperBound - chunk.range.upperBound

        await chunk.replacement()
        chunk.range = chunk.rangeAfterReplacement

        // If there is an active pondering effect ongoing that predated the replacement, adjust its range to account
        // for the replacement character delta. This ensures that when the pondering effect ends, the semantic chunk
        // remains the same so that the text visibility is restored for the updated range.
        //
        // Additionally, the range's start offset can be truncated to now start at the end of the replacement range,
        // since the range [activePonderingEffect.chunk.range.lowerBound, chunk.range.upperBound] will be covered by
        // the replacement range so there's no need for the pondering effect to also try to affect it.
        if let activePonderingEffect = self.activePonderingEffect {
            activePonderingEffect.chunk.range = chunk.range.upperBound..<(activePonderingEffect.chunk.range.upperBound + characterDelta)
        }

        let previews = await self.delegate.intelligenceTextEffectCoordinator(self, textPreviewsFor: NSRange(chunk.range))

#if canImport(UIKit)
        return previews
#else
        let suggestionRects = await self.delegate.intelligenceTextEffectCoordinator(self, rectsForProofreadingSuggestionsIn: NSRange(chunk.range))
        return previews.map {
            _WTTextPreview(snapshotImage: $0.previewImage, presentationFrame: $0.presentationFrame, backgroundColor: nil, clippingPath: nil, scale: 1, candidateRects: suggestionRects)
        }
#endif
    }

    func replacementEffectWillBegin(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async {
        // Stop the current pondering effect, and then create a new pondering effect once the replacement effect is complete.
        await self.setActivePonderingEffect(nil)
    }

    @discardableResult private func flushRemainingReplacementsIfNeeded() async -> Bool {
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
            assertionFailure("Intelligence text effect coordinator: Invariant failed (replacement effect completed without a context range)")
            return
        }

        guard let chunk = effect.chunk as? Chunk.Replacement else {
            assertionFailure("Intelligence text effect coordinator: Replacement effect chunk is not Chunk.Replacement")
            return
        }

        // At this point, the text has been replaced, and the effect's chunk's range has been updated to account for the latest character delta.
        let rangeAfterReplacement = chunk.range

        // Inform the coordinator the active replacement effect is over, and then inform the delegate to decorate the replacements if needed.
        await self.setActiveReplacementEffect(nil)
        await self.delegate.intelligenceTextEffectCoordinator(self, decorateReplacementsFor: NSRange(rangeAfterReplacement))

        // If this is the last chunk, that means that there will be no subsequent replacements, and no replacements other than
        // this one are in the replacement queue.
        //
        // Therefore, the entire animation is over and the selection can be restored to the context range.

        if chunk.finished {
            self.replacementQueue.removeFirst()
            await self.restoreSelectionAcceptedReplacements(true)
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
            await self.setActivePonderingEffect(ponderEffectForUnprocessedRange)
        }

        // Now that the first replacement is complete, remove it from the queue, and start the next one in line.

        self.replacementQueue.removeFirst()

        if let next = self.replacementQueue.first {
            await self.startReplacementAnimation(using: next)
        }
    }
}

// MARK: WKIntelligenceTextEffectCoordinator.Chunk

extension WKIntelligenceTextEffectCoordinator {
    class Chunk: PlatformIntelligenceTextEffectChunk {
        fileprivate class Pondering: Chunk {
            override init(range: Range<Int>) {
                super.init(range: range)
            }
        }

        fileprivate class Replacement: Chunk {
            let rangeAfterReplacement: Range<Int>
            let finished: Bool
            let replacement: (() async -> Void)

            init(range: Range<Int>, rangeAfterReplacement: Range<Int>, finished: Bool, replacement: @escaping (() async -> Void)) {
                self.rangeAfterReplacement = rangeAfterReplacement
                self.finished = finished
                self.replacement = replacement
                super.init(range: range)
            }
        }

        let id = UUID()

        fileprivate var range: Range<Int>

        private init(range: Range<Int>) {
            self.range = range
        }
    }
}

// MARK: WKIntelligenceTextEffectCoordinator.Chunk + Hashable & Equatable

extension WKIntelligenceTextEffectCoordinator.Chunk: Hashable, Equatable {
    static func == (lhs: WKIntelligenceTextEffectCoordinator.Chunk, rhs: WKIntelligenceTextEffectCoordinator.Chunk) -> Bool {
        lhs.id == rhs.id
    }

    func hash(into hasher: inout Hasher) {
        self.id.hash(into: &hasher)
    }
}

// MARK: Misc. helper functions

/// Converts a block with a completion handler into an async block.
fileprivate func async(_ block: @escaping (@escaping () -> Void) -> Void) -> (() async -> Void) {
    { @MainActor in
        await withCheckedContinuation { continuation in
            block(continuation.resume)
        }
    }
}

#endif

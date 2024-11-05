//
// Copyright (C) 2024 Apple Inc. All rights reserved.
//

import Foundation

#if canImport(WritingTools)

#if canImport(AppKit)
import AppKit
// WritingToolsUI is not present in the base system, but WebKit is, so it must be weak-linked.
// WritingToolsUI need not be soft-linked from WebKitSwift because although WTUI links WebKit, WebKit does not directly link WebKitSwift.
@_weakLinked internal import WritingToolsUI_Private._WTTextEffectView
@_weakLinked internal import WritingToolsUI_Private._WTSweepTextEffect
@_weakLinked internal import WritingToolsUI_Private._WTReplaceTextEffect
#else
internal import UIKit_Private
@_spi(TextEffects) import UIKit
#endif

import WebKitSwift

// MARK: Platform abstraction type aliases

#if canImport(AppKit)
typealias PlatformView = NSView
typealias PlatformBounds = NSRect
typealias PlatformTextPreview = [_WTTextPreview]
#else
typealias PlatformView = UIView
typealias PlatformBounds = CGRect
typealias PlatformTextPreview = UITargetedPreview
#endif

// MARK: Platform abstraction protocols

/// Some arbitrary data which can be translated to and represented by a text preview,
/// and also be able to be identified.
protocol PlatformIntelligenceTextEffectChunk: Identifiable {
}

/// Either a pondering or replacement effect.
@MainActor protocol PlatformIntelligenceTextEffect: Equatable, Identifiable where ID == PlatformIntelligenceTextEffectID {
    associatedtype Chunk: PlatformIntelligenceTextEffectChunk

    var chunk: Chunk { get }

    // Clients should not invoke this function directly.
    func _add<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk
}

extension PlatformIntelligenceTextEffect {
    nonisolated static func ==(lhs: Self, rhs: Self) -> Bool {
        lhs.id == rhs.id
    }
}

/// A combination source+delegate protocol that clients conform to to control behavior and yield information to the effect view.
@MainActor protocol PlatformIntelligenceTextEffectViewSource: AnyObject {
    associatedtype Chunk: PlatformIntelligenceTextEffectChunk

    /// Transforms an arbitrary chunk into a text preview.
    func textPreview(for chunk: Chunk) async -> PlatformTextPreview?

    /// Controls the visibility of text associated with the specified chunk.
    func updateTextChunkVisibility(_ chunk: Chunk, visible: Bool) async

    /// In the implementation of this method, clients should replace the backing text storage (which mustn't be visible to the user).
    /// Then, a preview of the resulting text should be created.
    ///
    /// Clients must also take the responsibility of animating the remaining text away from the replaced text if needed, using
    /// the provided animation parameters.
    func performReplacementAndGeneratePreview(for chunk: Chunk, effect: PlatformIntelligenceReplacementTextEffect<Chunk>, animation: PlatformIntelligenceReplacementTextEffect<Chunk>.AnimationParameters) async -> PlatformTextPreview?

    /// This function is invoked after an effect has been added and set-up, but before the effect actually begins.
    func replacementEffectWillBegin(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async

    /// This function is invoked once both parts of the replacement effect are complete.
    func replacementEffectDidComplete(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>)
}

// MARK: Platform type adapters.

#if canImport(UIKit)

@MainActor private final class UITextEffectViewSourceAdapter<Wrapped>: UITextEffectViewSource where Wrapped: PlatformIntelligenceTextEffectViewSource {
    private var wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped
    }

    func targetedPreview(for chunk: UITextEffectTextChunk) async -> UITargetedPreview {
        guard let chunk = chunk as? UITextEffectTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to create a targeted preview: parameter was of unexpected type \(type(of: chunk)).")
        }

        guard let preview = await self.wrapped.textPreview(for: chunk.wrapped) else {
            fatalError("Failed to create a targeted preview: unable to create a preview from the given chunk.")
        }

        return preview
    }

    func updateTextChunkVisibilityForAnimation(_ chunk: UITextEffectTextChunk, visible: Bool) async {
        guard let chunk = chunk as? UITextEffectTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to update text chunk visibility: parameter was of unexpected type \(type(of: chunk)).")
        }

        await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: visible)
    }
}

@MainActor private final class UIReplacementTextEffectDelegateAdapter<Wrapped>: UITextEffectView.ReplacementTextEffect.Delegate where Wrapped: PlatformIntelligenceTextEffectViewSource {
    private let wrapped: Wrapped
    private weak var view: PlatformIntelligenceTextEffectView<Wrapped>?

    init(wrapping wrapped: Wrapped, view: PlatformIntelligenceTextEffectView<Wrapped>) {
        self.wrapped = wrapped
        self.view = view
    }

    func replacementEffectDidComplete(_ effect: UITextEffectView.ReplacementTextEffect) {
        guard let view = self.view else {
            assertionFailure("Failed to handle completion of replacement effect: view was unexpectedly nil.")
            return
        }

        guard let effect = view.wrappedEffectIDToPlatformEffects[effect.id] as? PlatformIntelligenceReplacementTextEffect<Wrapped.Chunk> else {
            assertionFailure("Failed to handle completion of replacement effect: effect was unexpectedly nil.")
            return
        }

        self.wrapped.replacementEffectDidComplete(effect)
    }

    func performReplacementAndGeneratePreview(for chunk: UITextEffectTextChunk, effect: UITextEffectView.ReplacementTextEffect, animation: UITextEffectView.ReplacementTextEffect.AnimationParameters) async -> UITargetedPreview? {
        guard let view = self.view else {
            assertionFailure("Failed to perform replacement and generate preview: view was unexpectedly nil.")
            return nil
        }

        guard let effect = view.wrappedEffectIDToPlatformEffects[effect.id] as? PlatformIntelligenceReplacementTextEffect<Wrapped.Chunk> else {
            assertionFailure("Failed to perform replacement and generate preview: effect was unexpectedly nil.")
            return nil
        }

        guard let chunk = chunk as? UITextEffectTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to perform replacement and generate preview: parameter was of unexpected type \(type(of: chunk)).")
        }

        let animationParameters = PlatformIntelligenceReplacementTextEffect<Wrapped.Chunk>.AnimationParameters(duration: animation.duration, delay: animation.delay)

        return await self.wrapped.performReplacementAndGeneratePreview(for: chunk.wrapped, effect: effect, animation: animationParameters)
    }
}

private final class UITextEffectTextChunkAdapter<Wrapped>: UITextEffectTextChunk where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped
    }
}

#else

private final class WTTextPreviewAsyncSourceAdapter<Wrapped>: NSObject, _WTTextPreviewAsyncSource where Wrapped: PlatformIntelligenceTextEffectViewSource {
    private let wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped
    }

    func textPreviews(for chunk: _WTTextChunk) async -> [_WTTextPreview]? {
        if let chunk = chunk as? WTPonderingTextChunkAdapter<Wrapped.Chunk> {
            return await self.wrapped.textPreview(for: chunk.wrapped)
        }

        if let chunk = chunk as? WTReplacementTextChunkAdapter<Wrapped.Chunk> {
            return chunk.preview
        }

        fatalError("Failed to create a text preview: parameter was of unexpected type \(type(of: chunk)).")
    }
    
    func textPreview(for rect: CGRect) async -> _WTTextPreview? {
        // FIXME: Implement this function so that subsequent text pieces animate out of the way if needed.
        nil
    }

    func updateIsTextVisible(_ isTextVisible: Bool, for chunk: _WTTextChunk) async {
        if let chunk = chunk as? WTPonderingTextChunkAdapter<Wrapped.Chunk> {
            await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: isTextVisible)
            return
        }

        if let chunk = chunk as? WTReplacementTextChunkAdapter<Wrapped.Chunk> {
            await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: isTextVisible)
            return
        }

        fatalError("Failed to update text chunk visibility: parameter was of unexpected type \(type(of: chunk)).")
    }
}

private final class WTReplacementTextChunkAdapter<Wrapped>: _WTTextChunk where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped
    let preview: PlatformTextPreview?

    init(wrapping wrapped: Wrapped, preview: PlatformTextPreview?) {
        self.wrapped = wrapped
        self.preview = preview

        super.init(chunkWithIdentifier: UUID().uuidString)
    }
}

private final class WTPonderingTextChunkAdapter<Wrapped>: _WTTextChunk where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped

        super.init(chunkWithIdentifier: UUID().uuidString)
    }
}

#endif

// MARK: Platform abstraction types

/// An opaque identifier for effects.
struct PlatformIntelligenceTextEffectID: Hashable {
    private let id = UUID()

    fileprivate init() {
    }
}

/// A platform-agnostic view to control intelligence text effects given a particular source.
@MainActor final class PlatformIntelligenceTextEffectView<Source>: PlatformView where Source: PlatformIntelligenceTextEffectViewSource {
#if canImport(UIKit)
    fileprivate typealias SourceAdapter = UITextEffectViewSourceAdapter<Source>
    fileprivate typealias Wrapped = UITextEffectView
#else
    fileprivate typealias SourceAdapter = WTTextPreviewAsyncSourceAdapter<Source>
    fileprivate typealias Wrapped = _WTTextEffectView
#endif

    fileprivate let source: Source
    fileprivate let wrapped: Wrapped

    private let viewSource: SourceAdapter

#if canImport(UIKit)
    fileprivate var wrappedEffectIDToPlatformEffects: [UITextEffectView.EffectID : any PlatformIntelligenceTextEffect] = [:]
    fileprivate var platformEffectIDToWrappedEffectIDs: [PlatformIntelligenceTextEffectID : UITextEffectView.EffectID] = [:]
#else
    fileprivate var platformEffectIDToWrappedEffectIDs: [PlatformIntelligenceTextEffectID : Set<UUID>] = [:]
#endif

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    /// Create a new text effect view.
    init(source: Source) {
        self.source = source
        self.viewSource = SourceAdapter(wrapping: self.source)

#if canImport(UIKit)
        self.wrapped = Wrapped(source: self.viewSource)
#else
        self.wrapped = Wrapped(asyncSource: self.viewSource)
#endif

        super.init(frame: .zero)

        self.addSubview(self.wrapped)
        self.wrapped.frame = self.bounds
    }

    override var bounds: PlatformBounds {
        get {
            super.bounds
        }
        set {
            super.bounds = newValue
            self.wrapped.frame = newValue
        }
    }

    /// Prepares and adds an effect to be presented within the view.
    @discardableResult func addEffect<Effect>(_ effect: Effect) async -> Effect.ID where Effect: PlatformIntelligenceTextEffect, Effect.Chunk == Source.Chunk {
        await effect._add(to: self)
        return effect.id
    }

    /// Removes the effect with the specified id.
    func removeEffect(_ effectID: PlatformIntelligenceTextEffectID) {
        guard let wrappedEffectIDs = self.platformEffectIDToWrappedEffectIDs.removeValue(forKey: effectID) else {
            return
        }

#if canImport(UIKit)
        self.wrappedEffectIDToPlatformEffects[wrappedEffectIDs] = nil
        self.wrapped.removeEffect(wrappedEffectIDs)
#else
        for wrappedEffectID in wrappedEffectIDs {
            self.wrapped.removeEffect(wrappedEffectID)
        }
#endif
    }

    /// Removes all currently active effects.
    func removeAllEffects() {
        self.wrapped.removeAllEffects()
        self.platformEffectIDToWrappedEffectIDs = [:]

#if canImport(UIKit)
        self.wrappedEffectIDToPlatformEffects = [:]
#endif
    }
}

/// A replacement effect, which essentially involves the original text fading away while at the same time the new text fades in right above it.
@MainActor class PlatformIntelligenceReplacementTextEffect<Chunk>: PlatformIntelligenceTextEffect where Chunk: PlatformIntelligenceTextEffectChunk {
    struct AnimationParameters {
        let duration: TimeInterval
        let delay: TimeInterval
    }

    let id = PlatformIntelligenceTextEffectID()
    let chunk: Chunk

    // This is needed to keep track of when the entire replacement effect has completed,
    // since it is not guaranteed that the source completion handler is always invoked prior
    // to the destination effect completion handler.
    private var hasCompletedPartialWrappedEffect = false

    init(chunk: Chunk) {
        self.chunk = chunk
    }

#if canImport(AppKit)
    private func createEffects<Source>(using view: PlatformIntelligenceTextEffectView<Source>, sourceChunk: WTReplacementTextChunkAdapter<Chunk>, destinationChunk: WTReplacementTextChunkAdapter<Chunk>) where Source : PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        let wrappedDestinationEffect = _WTReplaceTextEffect(chunk: destinationChunk, effectView: view.wrapped)
        wrappedDestinationEffect.isDestination = true
        wrappedDestinationEffect.animateRemovalWhenDone = true

        wrappedDestinationEffect.completion = {
            if self.hasCompletedPartialWrappedEffect {
                view.source.replacementEffectDidComplete(self)
            }

            self.hasCompletedPartialWrappedEffect = true
        }

        let wrappedSourceEffect = _WTReplaceTextEffect(chunk: sourceChunk, effectView: view.wrapped)
        wrappedSourceEffect.animateRemovalWhenDone = false

        wrappedSourceEffect.preCompletion = {
            // This block is invoked after the source effect has been prepared, but before it actually begins.
            // It's intended for the destination effect to be added here, synchronously.

            let destinationEffectID = view.wrapped.add(wrappedDestinationEffect)!
            view.platformEffectIDToWrappedEffectIDs[self.id, default: []].insert(destinationEffectID)
        }

        wrappedSourceEffect.completion = {
            if self.hasCompletedPartialWrappedEffect {
                view.source.replacementEffectDidComplete(self)
            }

            self.hasCompletedPartialWrappedEffect = true
        }

        let sourceEffectID = view.wrapped.add(wrappedSourceEffect)!
        view.platformEffectIDToWrappedEffectIDs[self.id, default: []].insert(sourceEffectID)
    }
#endif

    func _add<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async where Source : PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
#if canImport(UIKit)
        // The UIKit effects interface natively supports async operations such as replacing text, since the source and destination previews
        // are generated before any animation actually begins.

        let chunkAdapter = UITextEffectTextChunkAdapter(wrapping: self.chunk)
        let delegateAdapter = UIReplacementTextEffectDelegateAdapter(wrapping: view.source, view: view)
        let wrappedEffect = UITextEffectView.ReplacementTextEffect(chunk: chunkAdapter, view: view.wrapped, delegate: delegateAdapter)
        view.wrapped.addEffect(wrappedEffect)

        view.wrappedEffectIDToPlatformEffects[wrappedEffect.id] = self
        view.platformEffectIDToWrappedEffectIDs[self.id] = wrappedEffect.id
#else
        // WritingToolsUI usually performs a replacement effect using the following flow:
        //
        //  1. A source effect is created
        //  2. WTUI then requests a text preview of the source text.
        //  3. WTUI requests the relevant text to become invisible.
        //  4. WTUI invokes the 'pre-completion' block associated with the source effect. This gets invokes after the preview is generated
        //     but before the effect actually begins. In this block, it is intended that the client performs the replacement, and then
        //     begins a destination effect.
        //  5. After adding the destination effect, WTUI requests a text preview of the now-replaced text.
        //  6. WTUI then uses this text preview to begin the destination effect.
        //  7. This results in the destination effect beginning immediately after the source effect begins, with no delay.
        //
        // However, if the replacement must happen asynchronously, the destination effect cannot begin immediately after the source effect,
        // since it needs to wait for the text to be replaced so that it can then request a preview of the replaced text. Consequently, the
        // whole replacement effect will not look correct, since the destination effect will lag behind the source effect.
        //
        // To address this gap in the interface of WTUI, the source and destination effect are abstracted into a single replacement effect.
        // When adding this replacement effect, the replacement happens prior to both the source and destination effects starting. Specifically,
        // this effectively works the way the UIKit interface does things. This allows the destination effect to happen immediately when needed,
        // since the preview generation and replacement has already happened.

        // First, a text preview of the source text is manually generated and saved.
        // When WTUI requests the text preview of the source, this preview can be synchronously returned in the delegate method rather than
        // a new preview being generated on-demand.
        let sourcePreview = await view.source.textPreview(for: self.chunk)
        let sourceChunkAdapter = WTReplacementTextChunkAdapter(wrapping: self.chunk, preview: sourcePreview)

        // The replacement is then immediately performed. At this point, the text should be hidden
        // so the user does not see the text update yet.
        //
        // At the moment, this really only works if there is an existing effect ongoing when this happens so that the user would see that
        // effect instead of disappearing text. In practice, a pondering effect should always precede a replacement effect, so this shouldn't
        // be a problem, but strictly speaking this is not a general solution.
        //
        // The UIKit interface does not have this issue since they can just present the source preview anyways while this is happening.

        // FIXME: Don't assume that the text will be hidden at this point by a prior effect.
        // FIXME: Mimic what UIKit does and add the source preview during this time, above the underlying text but below any prior effects.

        // When the FIXMEs are addressed, this will allow replacement effects to be seamlessly added without any prior effects needed.

        let destinationPreview = await view.source.performReplacementAndGeneratePreview(for: self.chunk, effect: self, animation: .init(duration: 0, delay: 0))

        let destinationChunkAdapter = WTReplacementTextChunkAdapter(wrapping: self.chunk, preview: destinationPreview)

        // Inform the view source that all async operations have completed, and the source and destination effects are about to actually begin.
        // This is needed so that clients know when to stop any prior effects, since they should only be stopped when there will be no gap
        // between effects.
        await view.source.replacementEffectWillBegin(self)

        self.createEffects(using: view, sourceChunk: sourceChunkAdapter, destinationChunk: destinationChunkAdapter)
#endif
    }
}

/// An effect which adds a shimmer animation to some text, intended to indicate that some operation is pending.
class PlatformIntelligencePonderingTextEffect<Chunk>: PlatformIntelligenceTextEffect where Chunk: PlatformIntelligenceTextEffectChunk {
#if canImport(UIKit)
    private typealias ChunkAdapter = UITextEffectTextChunkAdapter
#else
    private typealias ChunkAdapter = WTPonderingTextChunkAdapter
#endif

    let id = PlatformIntelligenceTextEffectID()
    let chunk: Chunk

    init(chunk: Chunk) {
        self.chunk = chunk
    }

    func _add<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async where Source : PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        let chunkAdapter = ChunkAdapter(wrapping: self.chunk)

#if canImport(UIKit)
        let wrappedEffect = UITextEffectView.PonderingEffect(chunk: chunkAdapter, view: view.wrapped)
        view.wrapped.addEffect(wrappedEffect)

        view.wrappedEffectIDToPlatformEffects[wrappedEffect.id] = self
        view.platformEffectIDToWrappedEffectIDs[self.id] = wrappedEffect.id
#else
        let wrappedEffect = _WTSweepTextEffect(chunk: chunkAdapter, effectView: view.wrapped)
        view.wrapped.add(wrappedEffect)

        view.platformEffectIDToWrappedEffectIDs[self.id] = [wrappedEffect.identifier]
#endif
    }
}

#endif

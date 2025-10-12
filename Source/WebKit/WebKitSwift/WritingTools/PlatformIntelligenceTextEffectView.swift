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

import Foundation

#if compiler(>=6.0)

#if ENABLE_WRITING_TOOLS

#if canImport(AppKit) && !targetEnvironment(macCatalyst)

import AppKit
// WritingToolsUI is not present in the base system, but WebKit is, so it must be weak-linked.
// WritingToolsUI need not be soft-linked from WebKitSwift because although WTUI links WebKit, WebKit does not directly link WebKitSwift.
#if USE_APPLE_INTERNAL_SDK
@_weakLinked internal import WritingToolsUI_Private._WTTextEffectView
@_weakLinked internal import WritingToolsUI_Private._WTSweepTextEffect
@_weakLinked internal import WritingToolsUI_Private._WTReplaceTextEffect
#else
@_weakLinked internal import WritingToolsUI_Private_SPI
#endif // USE_APPLE_INTERNAL_SDK

#else

#if USE_APPLE_INTERNAL_SDK
@_spi(TextEffects) import UIKit
#else
import UIKit_SPI
#endif // USE_APPLE_INTERNAL_SDK

#endif // canImport(AppKit) && !targetEnvironment(macCatalyst)

// Work around rdar://145157171 by manually importing the cross-import module.
#if canImport(_WebKit_SwiftUI)
internal import _WebKit_SwiftUI
#endif
internal import SwiftUI

// MARK: Platform abstraction type aliases

#if canImport(AppKit) && !targetEnvironment(macCatalyst)
typealias CocoaView = NSView
typealias PlatformBounds = NSRect
typealias PlatformTextPreview = [_WTTextPreview]
#else
typealias CocoaView = UIView
typealias PlatformBounds = CGRect
typealias PlatformTextPreview = UITargetedPreview
#endif

struct PlatformContentPreview {
    let previewImage: CGImage?
    let presentationFrame: CGRect
}

// MARK: Platform abstraction protocols

/// Some arbitrary data which can be translated to and represented by a text preview,
/// and also be able to be identified.
protocol PlatformIntelligenceTextEffectChunk: Identifiable {
}

/// Either a pondering or replacement effect.
@MainActor
protocol PlatformIntelligenceTextEffect<Chunk>: Equatable, Identifiable where ID == PlatformIntelligenceTextEffectID {
    associatedtype Chunk: PlatformIntelligenceTextEffectChunk

    var chunk: Chunk { get }

    // Clients should not invoke this function directly.
    func internalAdd<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async
    where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk
}

extension PlatformIntelligenceTextEffect {
    nonisolated static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.id == rhs.id
    }
}

/// A combination source+delegate protocol that clients conform to to control behavior and yield information to the effect view.
@MainActor
protocol PlatformIntelligenceTextEffectViewSource: AnyObject {
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
    func performReplacementAndGeneratePreview(
        for chunk: Chunk,
        effect: PlatformIntelligenceReplacementTextEffect<Chunk>
    ) async -> (PlatformTextPreview?, remainder: PlatformContentPreview?)

    /// This function is invoked after preparing the replacement effect, but before the effect is added.
    func replacementEffectWillBegin(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async

    /// This function is invoked once both parts of the replacement effect are complete.
    func replacementEffectDidComplete(_ effect: PlatformIntelligenceReplacementTextEffect<Chunk>) async
}

// MARK: Platform type adapters.

#if canImport(UIKit)

@MainActor
private final class UITextEffectViewSourceAdapter<Wrapped>: NSObject, @preconcurrency UITextEffectViewSource
where Wrapped: PlatformIntelligenceTextEffectViewSource {
    private var wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped
    }

    // This method needs to be `@objc`, and the type itself needs to conform to `NSObject`, otherwise this method will always return `true`.
    //
    // This is because internally, UIKit creates a type with a default conformance to this protocol, and a default implementation of this method.
    // The default implementation ostensibly requires the real conforming type to be an `NSObject`, and if not will return `true`. And then, if
    // it is an `NSObject`, it performs a selector check, which requires an `@objc` implementation, else it will fail and once again return `true`.
    @objc
    func canGenerateTargetedPreviewForChunk(_ chunk: UITextEffectTextChunk) async -> Bool {
        if let chunk = chunk as? UIPonderingTextEffectTextChunkAdapter<Wrapped.Chunk> {
            return true
        }

        if let chunk = chunk as? UIReplacementTextEffectTextChunkAdapter<Wrapped.Chunk> {
            return chunk.source != nil
        }

        return false
    }

    func targetedPreview(for chunk: UITextEffectTextChunk) async -> UITargetedPreview {
        if let chunk = chunk as? UIPonderingTextEffectTextChunkAdapter<Wrapped.Chunk> {
            return chunk.preview
        }

        if let chunk = chunk as? UIReplacementTextEffectTextChunkAdapter<Wrapped.Chunk> {
            // The chunk source may be `nil` in the case of a replacement whose source range is an empty range.
            // This force unwrap is safe because UIKit invokes `canGenerateTargetedPreviewForChunk` prior to this call.
            // swift-format-ignore: NeverForceUnwrap
            return chunk.source!
        }

        fatalError("Failed to create a targeted preview: parameter was of unexpected type \(type(of: chunk)).")
    }

    func updateTextChunkVisibilityForAnimation(_ chunk: UITextEffectTextChunk, visible: Bool) async {
        if let chunk = chunk as? UIPonderingTextEffectTextChunkAdapter<Wrapped.Chunk> {
            await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: visible)
        }

        if let chunk = chunk as? UIReplacementTextEffectTextChunkAdapter<Wrapped.Chunk> {
            await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: visible)
        }
    }
}

@MainActor
private final class UIReplacementTextEffectDelegateAdapter<Wrapped>: @preconcurrency UITextEffectView.ReplacementTextEffect.Delegate
where Wrapped: PlatformIntelligenceTextEffectViewSource {
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

        guard let effect = view.wrappedEffectIDToPlatformEffects[effect.id] as? PlatformIntelligenceReplacementTextEffect<Wrapped.Chunk>
        else {
            assertionFailure("Failed to handle completion of replacement effect: effect was unexpectedly nil.")
            return
        }

        Task { @MainActor in
            await self.wrapped.replacementEffectDidComplete(effect)
        }
    }

    func performReplacementAndGeneratePreview(
        for chunk: UITextEffectTextChunk,
        effect: UITextEffectView.ReplacementTextEffect,
        animation: UITextEffectView.ReplacementTextEffect.AnimationParameters
    ) async -> UITargetedPreview? {
        guard let chunk = chunk as? UIReplacementTextEffectTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to perform replacement and generate preview: parameter was of unexpected type \(type(of: chunk)).")
        }

        return chunk.destination
    }
}

private final class UIPonderingTextEffectTextChunkAdapter<Wrapped>: UITextEffectTextChunk
where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped
    let preview: UITargetedPreview

    init(wrapping wrapped: Wrapped, preview: UITargetedPreview) {
        self.wrapped = wrapped
        self.preview = preview
    }
}

private final class UIReplacementTextEffectTextChunkAdapter<Wrapped>: UITextEffectTextChunk
where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped
    let source: UITargetedPreview?
    let destination: UITargetedPreview

    init(wrapping wrapped: Wrapped, source: UITargetedPreview?, destination: UITargetedPreview) {
        self.wrapped = wrapped
        self.source = source
        self.destination = destination
    }
}

#else

@MainActor
private final class WTTextPreviewAsyncSourceAdapter<Wrapped>: NSObject, @preconcurrency _WTTextPreviewAsyncSource
where Wrapped: PlatformIntelligenceTextEffectViewSource {
    private let wrapped: Wrapped

    init(wrapping wrapped: Wrapped) {
        self.wrapped = wrapped
    }

    func textPreviews(for chunk: _WTTextChunk) async -> [_WTTextPreview]? {
        guard let chunk = chunk as? WTTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to update text chunk visibility: parameter was of unexpected type \(type(of: chunk)).")
        }

        return chunk.preview
    }

    func textPreview(for rect: CGRect) async -> _WTTextPreview? {
        // This is implemented manually by the system instead of relying on the WTUI interface.
        nil
    }

    func updateIsTextVisible(_ isTextVisible: Bool, for chunk: _WTTextChunk) async {
        guard let chunk = chunk as? WTTextChunkAdapter<Wrapped.Chunk> else {
            fatalError("Failed to update text chunk visibility: parameter was of unexpected type \(type(of: chunk)).")
        }

        await self.wrapped.updateTextChunkVisibility(chunk.wrapped, visible: isTextVisible)
    }
}

private final class WTTextChunkAdapter<Wrapped>: _WTTextChunk where Wrapped: PlatformIntelligenceTextEffectChunk {
    let wrapped: Wrapped
    let preview: PlatformTextPreview?

    init(wrapping wrapped: Wrapped, preview: PlatformTextPreview?) {
        self.wrapped = wrapped
        self.preview = preview

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
@MainActor
final class PlatformIntelligenceTextEffectView<Source>: CocoaView where Source: PlatformIntelligenceTextEffectViewSource {
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
    fileprivate var wrappedEffectIDToPlatformEffects: [UITextEffectView.EffectID: any PlatformIntelligenceTextEffect] = [:]
    fileprivate var platformEffectIDToWrappedEffectIDs: [PlatformIntelligenceTextEffectID: UITextEffectView.EffectID] = [:]
    #else
    fileprivate var wrappedEffectIDToPlatformEffects: [UUID: any PlatformIntelligenceTextEffect<Source.Chunk>] = [:]
    fileprivate var platformEffectIDToWrappedEffectIDs: [PlatformIntelligenceTextEffectID: Set<UUID>] = [:]
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

        self.wrapped.clipsToBounds = true

        super.init(frame: .zero)
    }

    func initializeSubviews() {
        self.addSubview(self.wrapped)
        self.wrapped.frame = self.bounds
    }

    /// Prepares and adds an effect to be presented within the view.
    @discardableResult
    func addEffect<Effect>(_ effect: Effect) async -> Effect.ID where Effect: PlatformIntelligenceTextEffect, Effect.Chunk == Source.Chunk {
        await effect.internalAdd(to: self)
        return effect.id
    }

    /// Removes the effect with the specified id.
    func removeEffect(_ effectID: PlatformIntelligenceTextEffectID) async {
        guard let wrappedEffectIDs = self.platformEffectIDToWrappedEffectIDs.removeValue(forKey: effectID) else {
            return
        }

        #if canImport(UIKit)
        self.wrappedEffectIDToPlatformEffects[wrappedEffectIDs] = nil
        self.wrapped.removeEffect(wrappedEffectIDs)
        #else
        for wrappedEffectID in wrappedEffectIDs {
            if let platformEffect = self.wrappedEffectIDToPlatformEffects.removeValue(forKey: wrappedEffectID),
                platformEffect is PlatformIntelligencePonderingTextEffect<Source.Chunk>
            {
                // When WTUI starts a pondering effect, it creates a 0.75s opacity CA animation to fade out the text, so it is possible
                // that this is still ongoing by the time `removeEffect` is called. This may lead to issues if subsequent effects start
                // immediately after the effect is removed and the animation has yet to stop.
                //
                // To workaround this, manually try to find the applicable sublayer that WTUI adds the animation to, and remove it directly.
                // FIXME: This is a fragile workaround, and should be removed once WTUI has proper support for removing effects at any point.
                for sublayer in self.wrapped.layer?.sublayers ?? [] {
                    sublayer.removeAnimation(forKey: "opacity")
                }
            }

            self.wrappedEffectIDToPlatformEffects[wrappedEffectID] = nil
            self.wrapped.removeEffect(wrappedEffectID)
        }
        #endif
    }

    /// Removes all currently active effects.
    func removeAllEffects() {
        self.wrapped.removeAllEffects()
        self.platformEffectIDToWrappedEffectIDs = [:]
        self.wrappedEffectIDToPlatformEffects = [:]
    }
}

/// An effect which shifts the remaining text (the text after the currently replaced text) either up or down,
/// depending on if the newly replaced text is taller than the source text.
@MainActor
class PlatformIntelligenceRemainderAffordanceTextEffect<Chunk>: PlatformIntelligenceTextEffect
where Chunk: PlatformIntelligenceTextEffectChunk {
    private enum AnimationKind {
        case contract
        case expand
    }

    struct Previews {
        let source: PlatformTextPreview?
        let destination: PlatformTextPreview
        let remainder: PlatformContentPreview
    }

    let id = PlatformIntelligenceTextEffectID()
    let chunk: Chunk
    let previews: Previews

    init(chunk: Chunk, previews: Previews) {
        self.chunk = chunk
        self.previews = previews
    }

    private static func animation(for kind: AnimationKind) -> SwiftUI.Animation {
        // Empirically derived animation values, specifically ensuring that the text avoids being overlapped by the replacement animation.

        switch kind {
        case .contract: .easeInOut(duration: 0.4).delay(0.5)
        case .expand: .bouncy(duration: 0.4).delay(0.2)
        }
    }

    private func heightDelta() -> Double {
        #if canImport(UIKit)
        let sourceRect = previews.source?.size ?? .zero
        let destRect = previews.destination.size

        let delta = destRect.height - sourceRect.height
        #else
        let sourceRect = (previews.source ?? [])
            .map(\.presentationFrame)
            .reduce(CGRect.zero) { $0.union($1) }

        let destRect = previews.destination
            .map(\.presentationFrame)
            .reduce(CGRect.zero) { $0.union($1) }

        let delta = destRect.size.height - sourceRect.size.height
        #endif

        return delta
    }

    func internalAdd<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async
    where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        guard let remainderPreviewImage = previews.remainder.previewImage else {
            return
        }

        // Compute the difference in height between the original text and the replaced text.
        let delta = heightDelta()

        // Create two rects:
        // 1. A source frame for the remainder of the text content before the text is replaced.
        // 2. A destination frame for the remainder of the text content after the text is replaced.
        //
        // The y-coordinates are adjusted for AppKit to account for the origin being the bottom left instead of top left.

        let remainderRect = previews.remainder.presentationFrame

        #if canImport(UIKit)
        let remainderViewSourceFrameY = remainderRect.origin.y
        #else
        // origin-y-coordinate is flipped in AppKit
        let remainderViewSourceFrameY = view.frame.size.height - remainderRect.size.height - remainderRect.origin.y
        #endif

        let remainderViewSourceFrame = CGRect(
            x: remainderRect.origin.x,
            y: remainderViewSourceFrameY,
            width: remainderRect.size.width,
            height: remainderRect.size.height
        )

        #if canImport(UIKit)
        // shift down if the replaced text is taller than the source text
        let remainderViewDestFrameY = remainderViewSourceFrame.origin.y + delta
        #else
        // shift down if the replaced text is taller than the source text
        let remainderViewDestFrameY = remainderViewSourceFrame.origin.y - delta
        #endif

        let remainderViewDestinationFrame = CGRect(
            x: remainderViewSourceFrame.origin.x,
            y: remainderViewDestFrameY,
            width: remainderViewSourceFrame.size.width,
            height: remainderViewSourceFrame.size.height
        )

        // Create an empty view with the source frame, and set its layer's contents to the image
        // of the remaining text content.

        let remainderView = CocoaView(frame: remainderViewSourceFrame)

        #if canImport(UIKit)
        remainderView.layer.contents = remainderPreviewImage
        #else
        remainderView.wantsLayer = true
        // Safe force unwrap because AppKit creates a layer when `wantsLayer` is `true`.
        // swift-format-ignore: NeverForceUnwrap
        remainderView.layer!.contents = remainderPreviewImage
        #endif

        // Add the newly created view as a subview to the effect view.

        view.addSubview(remainderView)

        // Perform the animation to animate the frame to make room for the replaced text.
        // This will run concurrently with the replacement effect, and it must not ever overlap with the effect.

        let animation = Self.animation(for: delta > 0 ? .expand : .contract)
        let changes = {
            remainderView.frame = remainderViewDestinationFrame
        }

        #if canImport(UIKit)
        UIView.animate(animation, changes: changes)
        #else
        NSAnimationContext.animate(animation, changes: changes)
        #endif
    }
}

/// A replacement effect, which essentially involves the original text fading away while at the same time the new text fades in right above it.
@MainActor
class PlatformIntelligenceReplacementTextEffect<Chunk>: PlatformIntelligenceTextEffect where Chunk: PlatformIntelligenceTextEffectChunk {
    let id = PlatformIntelligenceTextEffectID()
    let chunk: Chunk

    // This is needed to keep track of when the entire replacement effect has completed,
    // since it is not guaranteed that the source completion handler is always invoked prior
    // to the destination effect completion handler.
    private var hasCompletedPartialWrappedEffect = false

    init(chunk: Chunk) {
        self.chunk = chunk
    }

    #if canImport(AppKit) && !targetEnvironment(macCatalyst)
    private func didCompletePartialWrappedEffect<Source>(for source: Source)
    where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        if self.hasCompletedPartialWrappedEffect {
            Task { @MainActor in
                await source.replacementEffectDidComplete(self)
            }
        }

        self.hasCompletedPartialWrappedEffect = true
    }
    #endif

    func internalAdd<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async
    where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        // The WT interfaces expect the replacement operation to be performed synchronously, else the source
        // and destination effects become disjoint and begin at different times.
        //
        // To workaround this, the replacement is performed immediately (with the text hidden prior so that
        // it is not visible to the user) before the actual effects begins. The source preview is generated
        // prior to this, and the destination preview is generated after. This allows the previews to be cached
        // so that they can later be retrieved by the WT interface delegates.

        let sourcePreview = await view.source.textPreview(for: self.chunk)

        await view.source.updateTextChunkVisibility(self.chunk, visible: false)

        let (destinationPreview, remainderPreview) = await view.source.performReplacementAndGeneratePreview(for: self.chunk, effect: self)

        guard let destinationPreview else {
            assertionFailure("Failed to generate destination text preview for replacement effect")
            return
        }

        #if canImport(UIKit)
        let chunkAdapter = UIReplacementTextEffectTextChunkAdapter(
            wrapping: self.chunk,
            source: sourcePreview,
            destination: destinationPreview
        )

        let delegateAdapter = UIReplacementTextEffectDelegateAdapter(wrapping: view.source, view: view)
        let wrappedEffect = UITextEffectView.ReplacementTextEffect(chunk: chunkAdapter, view: view.wrapped, delegate: delegateAdapter)

        await view.source.replacementEffectWillBegin(self)

        view.wrapped.addEffect(wrappedEffect)
        view.wrappedEffectIDToPlatformEffects[wrappedEffect.id] = self
        view.platformEffectIDToWrappedEffectIDs[self.id] = wrappedEffect.id
        #else
        // The WTUI interface on macOS exposes the replacement effect as two separate effects, a source effect
        // and a destination effect. To abstract this disparity between the platforms, the effects are modeled
        // as a single replacement effect, to match the iOS interface and provide a cohesive API.

        let sourceChunkAdapter = WTTextChunkAdapter(wrapping: self.chunk, preview: sourcePreview)
        let destinationChunkAdapter = WTTextChunkAdapter(wrapping: self.chunk, preview: destinationPreview)

        let wrappedDestinationEffect = _WTReplaceTextEffect(chunk: destinationChunkAdapter, effectView: view.wrapped)
        wrappedDestinationEffect.isDestination = true
        wrappedDestinationEffect.animateRemovalWhenDone = true

        wrappedDestinationEffect.completion = {
            // The destination completion handler is invoked right before it starts its opacity animation.
            self.didCompletePartialWrappedEffect(for: view.source)
        }

        let wrappedSourceEffect = _WTReplaceTextEffect(chunk: sourceChunkAdapter, effectView: view.wrapped)
        wrappedSourceEffect.animateRemovalWhenDone = false

        wrappedSourceEffect.preCompletion = {
            // This block is invoked after the source effect has been prepared, but before it actually begins.
            // It's intended for the destination effect to be added here, synchronously.

            // Misannotated WritingToolsUI symbol which never actually returns `nil`.
            // swift-format-ignore: NeverForceUnwrap
            let destinationEffectID = view.wrapped.add(wrappedDestinationEffect)!
            view.wrappedEffectIDToPlatformEffects[destinationEffectID] = self
            view.platformEffectIDToWrappedEffectIDs[self.id, default: []].insert(destinationEffectID)
        }

        wrappedSourceEffect.completion = {
            // The source completion handler is invoked right after it ends its opacity animation.
            self.didCompletePartialWrappedEffect(for: view.source)
        }

        await view.source.replacementEffectWillBegin(self)

        // Misannotated WritingToolsUI symbol which never actually returns `nil`.
        // swift-format-ignore: NeverForceUnwrap
        let sourceEffectID = view.wrapped.add(wrappedSourceEffect)!
        view.wrappedEffectIDToPlatformEffects[sourceEffectID] = self
        view.platformEffectIDToWrappedEffectIDs[self.id, default: []].insert(sourceEffectID)
        #endif

        guard let remainderPreview else {
            return
        }

        let previews = PlatformIntelligenceRemainderAffordanceTextEffect<Chunk>
            .Previews(source: sourcePreview, destination: destinationPreview, remainder: remainderPreview)
        let remainderEffect = PlatformIntelligenceRemainderAffordanceTextEffect(chunk: chunk, previews: previews)
        await remainderEffect.internalAdd(to: view)
    }
}

/// An effect which adds a shimmer animation to some text, intended to indicate that some operation is pending.
class PlatformIntelligencePonderingTextEffect<Chunk>: PlatformIntelligenceTextEffect where Chunk: PlatformIntelligenceTextEffectChunk {
    #if canImport(UIKit)
    private typealias ChunkAdapter = UIPonderingTextEffectTextChunkAdapter
    #else
    private typealias ChunkAdapter = WTTextChunkAdapter
    #endif

    let id = PlatformIntelligenceTextEffectID()
    let chunk: Chunk

    init(chunk: Chunk) {
        self.chunk = chunk
    }

    func internalAdd<Source>(to view: PlatformIntelligenceTextEffectView<Source>) async
    where Source: PlatformIntelligenceTextEffectViewSource, Source.Chunk == Chunk {
        guard let preview = await view.source.textPreview(for: self.chunk) else {
            return
        }

        let chunkAdapter = ChunkAdapter(wrapping: self.chunk, preview: preview)

        #if canImport(UIKit)
        let wrappedEffect = UITextEffectView.PonderingEffect(chunk: chunkAdapter, view: view.wrapped)
        view.wrapped.addEffect(wrappedEffect)

        view.wrappedEffectIDToPlatformEffects[wrappedEffect.id] = self
        view.platformEffectIDToWrappedEffectIDs[self.id] = wrappedEffect.id
        #else
        let wrappedEffect = _WTSweepTextEffect(chunk: chunkAdapter, effectView: view.wrapped)
        view.wrapped.add(wrappedEffect)

        view.wrappedEffectIDToPlatformEffects[wrappedEffect.identifier] = self
        view.platformEffectIDToWrappedEffectIDs[self.id] = [wrappedEffect.identifier]
        #endif
    }
}

#endif // ENABLE_WRITING_TOOLS

#endif // compiler(>=6.0)

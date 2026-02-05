// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE_WRITING_TOOLS && canImport(UIKit)

import OSLog

#if USE_APPLE_INTERNAL_SDK
@_spi(TextEffects) import UIKit
#else
import UIKit_SPI
#endif // USE_APPLE_INTERNAL_SDK

@objc
@implementation
extension WKTextAnimationManager {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "WKTextAnimationManager")

    private final class TextEffectChunk: UITextEffectTextChunk {
        let uuid: UUID

        init(uuid: UUID) {
            self.uuid = uuid
        }
    }

    @MainActor
    private final class Base {
        var currentEffect: UITextEffectView.EffectID?
    }

    // Used to workaround the fact that `@objc @implementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc
    private let base = Base()

    @nonobjc
    private final var currentEffect: UITextEffectView.EffectID? {
        get { base.currentEffect }
        set { base.currentEffect = newValue }
    }

    @nonobjc
    private lazy var effectView = UITextEffectView(source: self)
    @nonobjc
    private var chunkToEffect: [UUID: UITextEffectView.EffectID] = [:]

    private weak var delegate: WKTextAnimationSourceDelegate?

    init(delegate: any WKTextAnimationSourceDelegate) {
        super.init()

        self.delegate = delegate
        delegate.containingViewForTextAnimationType().addSubview(self.effectView)
    }

    @objc(addTextAnimationForAnimationID:withStyleType:)
    func addTextAnimation(forAnimationID uuid: UUID, withStyleType style: WKTextAnimationType) {
        switch style {
        case .initial:
            let newEffect = effectView.addEffect(
                UITextEffectView.PonderingEffect(chunk: TextEffectChunk(uuid: uuid), view: effectView)
            )
            chunkToEffect[uuid] = newEffect

        case .source:
            let newEffect = effectView.addEffect(
                UITextEffectView.ReplacementTextEffect(chunk: TextEffectChunk(uuid: uuid), view: effectView, delegate: self)
            )
            chunkToEffect[uuid] = newEffect

        case .final:
            break
        // Discard `.final` since we don't manually start the 2nd part of the animation on iOS.

        @unknown default:
            fatalError()
        }
    }

    func removeTextAnimation(forAnimationID uuid: UUID) {
        if let effectID = chunkToEffect.removeValue(forKey: uuid) {
            effectView.removeEffect(effectID)
        }
    }
}

@_spi(TextEffects)
extension WKTextAnimationManager: @preconcurrency UITextEffectViewSource {
    // This can be made non-public once the retroactive conformance is removed.
    // It is currently safe due to the fact that WebKitSwift is not itself public.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func targetedPreview(for chunk: UITextEffectTextChunk) async -> UITargetedPreview {
        guard let delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate.")
            return UITargetedPreview(view: UIView(frame: .zero))
        }

        let container = delegate.containingViewForTextAnimationType()
        let defaultPreview = UITargetedPreview(
            view: UIView(frame: .zero),
            parameters: .init(),
            target: UIPreviewTarget(container: container, center: container.center)
        )

        guard let uuidChunk = chunk as? TextEffectChunk else {
            Self.logger.debug("Can't get text preview. Incorrect UITextEffectTextChunk subclass")
            return defaultPreview
        }

        guard let preview = await delegate.targetedPreview(for: uuidChunk.uuid) else {
            Self.logger.debug("Could not generate a UITargetedPreview")
            return defaultPreview
        }

        return preview
    }

    // This can be made non-public once the retroactive conformance is removed.
    // It is currently safe due to the fact that WebKitSwift is not itself public.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func updateTextChunkVisibilityForAnimation(_ chunk: UITextEffectTextChunk, visible: Bool) async {
        guard let uuidChunk = chunk as? TextEffectChunk else {
            Self.logger.debug("Can't update text visibility. Incorrect UITextEffectTextChunk subclass")
            return
        }
        guard let delegate else {
            Self.logger.debug("Can't update Chunk Visibility. Missing delegate.")
            return
        }
        await delegate.updateUnderlyingTextVisibility(forTextAnimationID: uuidChunk.uuid, visible: visible)
    }
}

@_spi(TextEffects)
extension WKTextAnimationManager: @preconcurrency UITextEffectView.ReplacementTextEffect.Delegate {
    // This can be made non-public once the retroactive conformance is removed.
    // It is currently safe due to the fact that WebKitSwift is not itself public.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func performReplacementAndGeneratePreview(
        for chunk: UITextEffectTextChunk,
        effect: UITextEffectView.ReplacementTextEffect,
        animation: UITextEffectView.ReplacementTextEffect.AnimationParameters
    ) async -> UITargetedPreview? {
        guard let uuidChunk = chunk as? TextEffectChunk else {
            Self.logger.debug("Can't get text preview. Incorrect UITextEffectTextChunk subclass")
            return nil
        }

        guard let delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate.")
            return nil
        }

        let preview = await withCheckedContinuation { continuation in
            delegate.callCompletionHandler(forAnimationID: uuidChunk.uuid) { preview in
                continuation.resume(returning: preview)
            }
        }

        return preview
    }

    // This can be made non-public once the retroactive conformance is removed.
    // It is currently safe due to the fact that WebKitSwift is not itself public.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public func replacementEffectDidComplete(_ effect: UITextEffectView.ReplacementTextEffect) {
        effectView.removeEffect(effect.id)

        guard let (animationID, _) = chunkToEffect.first(where: { (_, value) in value == effect.id }) else {
            return
        }

        chunkToEffect[animationID] = nil

        guard let delegate else {
            Self.logger.debug("Missing delegate.")
            return
        }

        delegate.callCompletionHandler(forAnimationID: animationID)
        delegate.replacementEffectDidComplete()
    }
}

#endif // ENABLE_WRITING_TOOLS && canImport(UIKit)

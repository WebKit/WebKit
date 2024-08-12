//
// Copyright (C) 2024 Apple Inc. All rights reserved.
//

#if canImport(WritingTools) && canImport(UIKit)

import OSLog
import WebKit
import WebKitSwift
@_implementationOnly import UIKit_Private
@_spi(TextEffects) import UIKit

@objc public enum WKTextAnimationType: Int {
    case initial
    case source
    case final
}

@objc(WKSTextAnimationManager)
@MainActor public final class TextAnimationManager: NSObject {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "TextAnimationType")
    
    final class TextEffectChunk: UITextEffectTextChunk {
        public let uuid: UUID
        
        public init(uuid: UUID) {
            self.uuid = uuid
        }
    }
    
    private var currentEffect: UITextEffectView.EffectID?
    private lazy var effectView = UITextEffectView(source: self)
    private var chunkToEffect = [UUID: UITextEffectView.EffectID]()
    
    @objc public weak var delegate: WKSTextAnimationSourceDelegate?

    @objc(initWithDelegate:) public init(with delegate: any WKSTextAnimationSourceDelegate) {
        super.init()
        
        self.delegate = delegate
        delegate.containingViewForTextAnimationType().addSubview(self.effectView)
    }
    
    @objc(addTextAnimationForAnimationID:withStyleType:) public func beginEffect(for uuid: UUID, style: WKTextAnimationType) {
        if style == .initial {
            let newEffect = self.effectView.addEffect(UITextEffectView.PonderingEffect(chunk: TextEffectChunk(uuid: uuid), view: self.effectView) as UITextEffectView.TextEffect)
            self.chunkToEffect[uuid] = newEffect
        } else {
            let newEffect = self.effectView.addEffect(UITextEffectView.ReplacementTextEffect(chunk: TextEffectChunk(uuid: uuid), view: self.effectView, delegate:self) as UITextEffectView.TextEffect)
            self.chunkToEffect[uuid] = newEffect
        }
    }
    
    @objc(removeTextAnimationForAnimationID:) public func endEffect(for uuid: UUID) {
        if let effectID = chunkToEffect.removeValue(forKey: uuid) {
            self.effectView.removeEffect(effectID)
        }
    }
}
    
@_spi(TextEffects)
extension TextAnimationManager: UITextEffectViewSource {
    public func targetedPreview(for chunk: UITextEffectTextChunk) async -> UITargetedPreview {
        
        guard let delegate = self.delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate." )
            return UITargetedPreview(view: UIView(frame: .zero))
        }
        
        let defaultPreview = UITargetedPreview(view: UIView(frame: .zero), parameters: UIPreviewParameters(), target: UIPreviewTarget(container: delegate.containingViewForTextAnimationType(), center: delegate.containingViewForTextAnimationType().center))
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
    
    public func updateTextChunkVisibilityForAnimation(_ chunk: UITextEffectTextChunk, visible: Bool) async {
        guard let uuidChunk = chunk as? TextEffectChunk else {
            Self.logger.debug("Can't update text visibility. Incorrect UITextEffectTextChunk subclass")
            return
        }
        guard let delegate = self.delegate else {
            Self.logger.debug("Can't update Chunk Visibility. Missing delegate." )
            return
        }
        await delegate.updateUnderlyingTextVisibility(forTextAnimationID:uuidChunk.uuid, visible: visible)
    }
}

@_spi(TextEffects)
extension TextAnimationManager: UITextEffectView.ReplacementTextEffect.Delegate {
    public func performReplacementAndGeneratePreview(for chunk: UITextEffectTextChunk, effect: UITextEffectView.ReplacementTextEffect, animation: UITextEffectView.ReplacementTextEffect.AnimationParameters) async -> UITargetedPreview? {
        guard let uuidChunk = chunk as? TextEffectChunk else {
            Self.logger.debug("Can't get text preview. Incorrect UITextEffectTextChunk subclass")
            return nil
        }
        guard let delegate = self.delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate." )
            return nil
        }
        guard let preview = await delegate.targetedPreview(for: uuidChunk.uuid) else {
            Self.logger.debug("Could not generate a UITargetedPreview")
            return nil
        }
        return preview
    }
    
    public func replacementEffectDidComplete(_ effect: UITextEffectView.ReplacementTextEffect) {
        self.effectView.removeEffect(effect.id)

        guard let (animationID, _) = chunkToEffect.first(where: { (_, value) in value == effect.id }) else {
            return
        }

        chunkToEffect[animationID] = nil

        guard let delegate = self.delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate.")
            return
        }

        delegate.callCompletionHandler(forAnimationID: animationID)
    }
}

#endif // canImport(WritingTools) && canImport(UIKit)

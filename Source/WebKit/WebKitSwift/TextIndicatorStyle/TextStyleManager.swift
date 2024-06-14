//
// Copyright (C) 2024 Apple Inc. All rights reserved.
//

#if canImport(WritingTools) && canImport(UIKit)

import OSLog
import WebKit
import WebKitSwift
@_implementationOnly import UIKit_Private
@_spi(TextEffects) import UIKit

@objc public enum WKTextIndicatorStyleType: Int {
    case initial
    case source
    case final
}

@objc(WKSTextStyleManager)
@MainActor public final class TextStyleManager: NSObject {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "TextIndicatorStyle")
    
    final class TextEffectChunk: UITextEffectTextChunk {
        public let uuid: UUID
        
        public init(uuid: UUID) {
            self.uuid = uuid
        }
    }
    
    private var currentEffect: UITextEffectView.EffectID?
    private lazy var effectView = UITextEffectView(source: self)
    private var chunkToEffect = [UUID: UITextEffectView.EffectID]()
    
    @objc public weak var delegate: WKSTextStyleSourceDelegate?
    
    @objc(initWithDelegate:) public init(with delegate: any WKSTextStyleSourceDelegate) {
        super.init()
        
        self.delegate = delegate
        delegate.containingViewForTextIndicatorStyle().addSubview(self.effectView)
    }
    
    @objc(addTextIndicatorStyleForID:withStyleType:) public func beginEffect(for uuid: UUID, style: WKTextIndicatorStyleType) {
        if style == .initial {
            let newEffect = self.effectView.addEffect(UITextEffectView.PonderingEffect(chunk: TextEffectChunk(uuid: uuid), view: self.effectView) as UITextEffectView.TextEffect)
            self.chunkToEffect[uuid] = newEffect
        } else {
            let newEffect = self.effectView.addEffect(UITextEffectView.ReplacementTextEffect(chunk: TextEffectChunk(uuid: uuid), view: self.effectView, delegate:self) as UITextEffectView.TextEffect)
            self.chunkToEffect[uuid] = newEffect
        }
    }
    
    @objc(removeTextIndicatorStyleForID:) public func endEffect(for uuid: UUID) {
        if let effectID = chunkToEffect.removeValue(forKey: uuid) {
            self.effectView.removeEffect(effectID)
        }
    }
}
    
@_spi(TextEffects)
extension TextStyleManager: UITextEffectViewSource {
    public func targetedPreview(for chunk: UITextEffectTextChunk) async -> UITargetedPreview {
        
        guard let delegate = self.delegate else {
            Self.logger.debug("Can't obtain Targeted Preview. Missing delegate." )
            return UITargetedPreview(view: UIView(frame: .zero))
        }
        
        let defaultPreview = UITargetedPreview(view: UIView(frame: .zero), parameters:UIPreviewParameters(), target:UIPreviewTarget(container:delegate.containingViewForTextIndicatorStyle(), center:delegate.containingViewForTextIndicatorStyle().center))
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
        await delegate.updateTextIndicatorStyleVisibility(for: uuidChunk.uuid, visible: visible)
    }
}

@_spi(TextEffects)
extension TextStyleManager: UITextEffectView.ReplacementTextEffect.Delegate {
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
        
        // FIXME: remove the effect from the chunkToEffect map rdar://126307144
    }
}

#endif // canImport(WritingTools) && canImport(UIKit)

// Copyright (C) 2025 Apple Inc. All rights reserved.
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

@MainActor
protocol IntelligenceTextEffectViewManagerDelegate {
    func updateTextChunkVisibility(_ chunk: IntelligenceTextEffectChunk, visible: Bool, force: Bool) async
}

@MainActor
class IntelligenceTextEffectViewManager<Source>
where
    Source: IntelligenceTextEffectViewManagerDelegate, Source: PlatformIntelligenceTextEffectViewSource,
    Source.Chunk == IntelligenceTextEffectChunk
{
    init(source: Source? = nil, contentView: CocoaView) {
        self.source = source
        self.contentView = contentView
    }

    private weak var source: Source?
    private let contentView: CocoaView

    private var effectView: PlatformIntelligenceTextEffectView<Source>? = nil

    private var activePonderingEffect: PlatformIntelligencePonderingTextEffect<IntelligenceTextEffectChunk>? = nil
    private var activeReplacementEffect: PlatformIntelligenceReplacementTextEffect<IntelligenceTextEffectChunk>? = nil

    var suppressEffectView = false

    var hasActiveEffects: Bool {
        self.activePonderingEffect != nil || self.activeReplacementEffect != nil
    }

    private func setupViewIfNeeded() {
        guard self.effectView == nil, let source = self.source else {
            return
        }

        let effectView = PlatformIntelligenceTextEffectView(source: source)

        #if os(iOS)
        effectView.isUserInteractionEnabled = false
        effectView.frame = contentView.frame
        contentView.superview?.addSubview(effectView)
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

    private func destroyViewIfNeeded() {
        guard self.activePonderingEffect == nil && self.activeReplacementEffect == nil else {
            return
        }

        self.effectView?.removeFromSuperview()
        self.effectView = nil
    }

    func setActivePonderingEffect(_ effect: PlatformIntelligencePonderingTextEffect<IntelligenceTextEffectChunk>?) async {
        if self.activePonderingEffect == nil && effect == nil {
            return
        }

        if self.activePonderingEffect != nil && effect != nil {
            assertionFailure(
                "Intelligence text effect coordinator: trying to either set a new pondering effect when there is an ongoing one."
            )
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
            //
            // Safe force-unwrap due to the above logic; effect being nil implies activePonderingEffect was originally non-nil.
            // swift-format-ignore: NeverForceUnwrap
            await self.source?.updateTextChunkVisibility(oldEffect!.chunk, visible: true, force: true)

            // Safe force-unwrap due to the above logic; effect being nil implies activePonderingEffect was originally non-nil.
            // swift-format-ignore: NeverForceUnwrap
            await self.effectView?.removeEffect(oldEffect!.id)

            self.destroyViewIfNeeded()
        }
    }

    func setActiveReplacementEffect(_ effect: PlatformIntelligenceReplacementTextEffect<IntelligenceTextEffectChunk>?) async {
        if self.activeReplacementEffect == nil && effect == nil {
            return
        }

        if self.activeReplacementEffect != nil && effect != nil {
            assertionFailure(
                "Intelligence text effect coordinator: trying to either set a new replacement effect when there is an ongoing one."
            )
            return
        }

        let oldEffect = self.activeReplacementEffect
        self.activeReplacementEffect = effect

        if let effect {
            self.setupViewIfNeeded()
            await self.effectView?.addEffect(effect)
        } else {
            // Safe force-unwrap due to the above logic; effect being nil implies activePonderingEffect was originally non-nil.
            // swift-format-ignore: NeverForceUnwrap
            await self.effectView?.removeEffect(oldEffect!.id)
            self.destroyViewIfNeeded()
        }
    }

    func hideEffects() async {
        guard !self.suppressEffectView else {
            return
        }

        // On macOS, the effect view currently doesn't scroll with the web content. Therefore, the effects need to be suppressed
        // when scrolling, resizing, or zooming.
        //
        // Consequently, since the effects will now be hidden, it is critical that the underlying text visibility is restored to be visible.

        self.suppressEffectView = true

        if let activePonderingEffect = self.activePonderingEffect {
            await source?.updateTextChunkVisibility(activePonderingEffect.chunk, visible: true)
        }

        if let activeReplacementEffect = self.activeReplacementEffect {
            await source?.updateTextChunkVisibility(activeReplacementEffect.chunk, visible: true)
        }

        self.effectView?.isHidden = true
    }

    func showEffects() async {
        // FIXME: Ideally, after scrolling/resizing/zooming, the effects would be restored and visible.
    }

    func removeActiveEffects() async {
        if self.activePonderingEffect != nil {
            await self.setActivePonderingEffect(nil)
        }

        if self.activeReplacementEffect != nil {
            await self.setActiveReplacementEffect(nil)
        }
    }

    func updateActivePonderingEffect(transform: (Range<Int>) -> Range<Int>) {
        if let activePonderingEffect = self.activePonderingEffect {
            activePonderingEffect.chunk.range = transform(activePonderingEffect.chunk.range)
        }
    }

    func reset() {
        self.effectView?.removeAllEffects()
        self.effectView?.removeFromSuperview()
        self.effectView = nil

        self.activePonderingEffect = nil
        self.activeReplacementEffect = nil

        self.suppressEffectView = false
    }

    func assertPonderingEffectIsInactive(file: StaticString = #file, line: UInt = #line) {
        assert(
            self.activePonderingEffect == nil,
            "Intelligence text effect coordinator: expected pondering effect to be inactive",
            file: file,
            line: line
        )
    }

    func assertReplacementEffectIsInactive(file: StaticString = #file, line: UInt = #line) {
        assert(
            self.activeReplacementEffect == nil,
            "Intelligence text effect coordinator: expected replacement effect to be inactive",
            file: file,
            line: line
        )
    }
}

#endif // ENABLE_WRITING_TOOLS

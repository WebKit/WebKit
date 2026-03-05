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

#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS

import os
@_weakLinked internal import RealityKit
internal import WebKit_Internal

@objc
@implementation
extension WKSeparatedImageView {
    @nonobjc
    final var viewMode: ViewMode
    @nonobjc
    final var cachedViewModeInfo: (NSString, ViewMode)?
    @nonobjc
    final var desiredViewingModeSpatial: Bool = true {
        didSet {
            #if canImport(RealityFoundation, _version: 387)
            self.portalEntity?.components[ImagePresentationComponent.self]?.desiredViewingMode =
                desiredViewingModeSpatial ? .spatial3D : .mono
            if let imageHash = self.imageHash {
                ImagePresentationCache.shared[imageHash]?.desiredViewingModeSpatial = desiredViewingModeSpatial
            }
            #endif
        }
    }

    @nonobjc
    final var cgImage: CGImage?
    @nonobjc
    final var imageData: Data?
    @nonobjc
    final var imageHash: NSString?

    @nonobjc
    final var computeHashTask: Task<Void, Error>?
    @nonobjc
    final var pickViewModeTask: Task<ViewMode, Error>?
    @nonobjc
    final var generate3DImageTask: Task<Void, Error>?

    #if canImport(RealityFoundation, _version: 387)
    @nonobjc
    final var spatial3DImage: ImagePresentationComponent.Spatial3DImage?
    #endif
    @nonobjc
    final var portalEntity: Entity?
    @nonobjc
    final var contentView: UIView
    @nonobjc
    final var cornerView: UIView
    @nonobjc
    final var spinner: UIActivityIndicatorView?
    @nonobjc
    final var toggleHostingView: UIView?

    @nonobjc
    final var isSeparated: Bool = false
    @nonobjc
    final var isAttached: Bool = false
    @nonobjc
    final var isRenderingIPC: Bool = false

    @nonobjc
    final var updateScheduled = false

    @nonobjc
    final var logPrefix: String = ""

    @nonobjc
    final private var scaleObservation: NSKeyValueObservation?

    @objc
    fileprivate init() {
        self.viewMode = .unknown
        self.contentView = UIView()
        self.cornerView = UIView(
            frame: CGRect(x: 0, y: 0, width: SeparatedImageViewConstants.cornerUISize, height: SeparatedImageViewConstants.cornerUISize)
        )

        super.init(frame: .zero)
        self.logPrefix = String(describing: ObjectIdentifier(self))

        #if USE_APPLE_INTERNAL_SDK
        if let observingLayer = self.layer as? WKObservingLayer {
            observingLayer.layerDelegate = self
        }
        #endif

        addSubview(self.contentView)
        addSubview(self.cornerView)

        scaleObservation = layer.observe(\.transform) { [weak self] _, _ in
            Task { @MainActor [weak self] in
                self?.setNeedsLayout()
            }
        }
    }

    /// Unavailable.
    @objc
    @available(*, unavailable)
    public required dynamic init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    @objc(setSurface:)
    func setSurface(_ surface: IOSurfaceRef?) {
        guard let surface else {
            cleanUp()
            return
        }
        Task {
            // The compiler can't guarantee (yet) this closure won't be called multiple times.
            nonisolated(unsafe) let captured = surface
            await processSurface(captured)
        }
    }

    final override func layoutSubviews() {
        super.layoutSubviews()
        layoutCustomSubtree()
    }
}

#if USE_APPLE_INTERNAL_SDK

extension WKSeparatedImageView: WKObservingLayerDelegate {
    final override class var layerClass: AnyClass {
        WKObservingLayer.self
    }

    nonisolated func layerSeparatedDidChange() {
        Task { @MainActor in
            let isSeparated = layer.isSeparated
            guard isSeparated != self.isSeparated else { return }
            self.isSeparated = isSeparated
            if !isSeparated {
                generate3DImageTask?.cancel()
                generate3DImageTask = nil
                isAttached = false
                isRenderingIPC = false
            }
            scheduleUpdate()
        }
    }

    nonisolated func layerWasCleared() {
        Task { @MainActor in
            cleanUp()
        }
    }
}

@objc
protocol WKObservingLayerDelegate {
    nonisolated func layerSeparatedDidChange()
    nonisolated func layerWasCleared()
}

class WKObservingLayer: CALayer {
    weak var layerDelegate: (any WKObservingLayerDelegate)?

    override var isSeparated: Bool {
        didSet {
            layerDelegate?.layerSeparatedDidChange()
        }
    }

    override var contents: Any? {
        didSet {
            if contents == nil {
                layerDelegate?.layerWasCleared()
            }
        }
    }
}

#endif

extension WKSeparatedImageView {
    func cleanUp() {
        computeHashTask?.cancel()
        pickViewModeTask?.cancel()
        generate3DImageTask?.cancel()

        computeHashTask = nil
        pickViewModeTask = nil
        generate3DImageTask = nil

        cgImage = nil
        contentView.layer.contents = nil
        portalEntity?.removeFromParent()
        portalEntity = nil
        isAttached = false
        isRenderingIPC = false
        #if canImport(RealityFoundation, _version: 387)
        spatial3DImage = nil
        #endif
        imageData = nil
        imageHash = nil
        viewMode = .unknown

        scheduleUpdate()
    }

    @objc(_web_setSubviews:)
    func webSetSubViews(_ subviews: NSArray) {
        // This view currently owns its hierarchy (for the spinner etc...)
    }
}

// MARK: - Extensions

extension Logger {
    static let separatedImage = Logger(subsystem: "com.apple.WebKit", category: "SeparatedImage")
}

#endif

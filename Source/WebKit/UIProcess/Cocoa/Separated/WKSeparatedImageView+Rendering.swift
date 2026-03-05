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

#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS && compiler(>=6.2)

#if canImport(CoreRE)
@_weakLinked internal import CoreRE
#endif
import os
@_weakLinked @_spi(Private) @_spi(RealityKit) internal import RealityKit
@_weakLinked @_spi(Private) @_spi(ForUIKitOnly) internal import SwiftUI
internal import WebKit_Internal

extension WKSeparatedImageView {
    func layoutCustomSubtree() {
        contentView.frame = bounds

        // Un-apply any scale on the layer.
        let scaleX = layer.value(forKeyPath: "transform.scale.x") as? CGFloat ?? 1.0
        let scaleY = layer.value(forKeyPath: "transform.scale.y") as? CGFloat ?? 1.0
        let safeScaleX = scaleX != 0 ? scaleX : 1.0
        let safeScaleY = scaleY != 0 ? scaleY : 1.0
        cornerView.transform = CGAffineTransform(scaleX: 1 / safeScaleX, y: 1 / safeScaleY)

        let centerX =
            if UIView.userInterfaceLayoutDirection(for: semanticContentAttribute) == .rightToLeft {
                SeparatedImageViewConstants.cornerUIMargin / safeScaleX + cornerView.frame.size.width / 2
            } else {
                bounds.width - SeparatedImageViewConstants.cornerUIMargin / safeScaleX - cornerView.frame.size.width / 2
            }
        let centerY = SeparatedImageViewConstants.cornerUIMargin / safeScaleY + cornerView.frame.size.height / 2
        cornerView.center = CGPoint(x: centerX, y: centerY)
    }

    func scheduleUpdate() {
        guard !updateScheduled else { return }
        updateScheduled = true

        Task { @MainActor [weak self] in
            guard let self else { return }
            self.updateScheduled = false
            self.update()
        }
    }

    func update() {
        updatePortal()
        updateContents()
        updateUI()
    }

    func updateContents() {
        contentView.layer.contents = cgImage

        var duration = SeparatedImageViewConstants.longFadeAnimationDuration
        #if canImport(RealityFoundation, _version: 387)
        if let imageHash, ImagePresentationCache.shared[imageHash] != nil {
            duration = SeparatedImageViewConstants.shortFadeAnimationDuration
        }
        #endif
        let contentVisible = portalEntity == nil || !isRenderingIPC
        contentView.updateVisibility(visible: contentVisible, animate: !contentVisible, duration: duration)
    }

    func updatePortal() {
        #if canImport(RealityFoundation, _version: 387)
        if cgImage == nil {
            portalEntity?.removeFromParent()
            portalEntity = nil
            isAttached = false
            isRenderingIPC = false
            return
        }

        guard viewMode == .portal && isSeparated && !isAttached else { return }

        guard let portalEntity else {
            Task {
                await self.requestPortalEntity()
            }
            return
        }

        isAttached = true

        let scaleFactor = Float(bounds.size.height / layer.effectivePointsPerMeter)
        portalEntity.scale = SIMD3(scaleFactor, scaleFactor, 1.0)

        #if canImport(CoreRE)
        if let component = unsafe RECALayerGetCALayerClientComponent(layer) {
            let rootEntity = unsafe Entity.fromCore(REComponentGetEntity(component))
            portalEntity.setParent(rootEntity)
        }
        #endif

        Task { @MainActor [weak self] in
            await self?.nextDisplayLink()
            self?.isRenderingIPC = self?.isAttached ?? false
            self?.scheduleUpdate()
        }
        #endif
    }

    func requestPortalEntity() async {
        #if canImport(RealityFoundation, _version: 387)
        guard generate3DImageTask == nil, isSeparated else { return }
        let task = startImage3DGeneration()
        generate3DImageTask = task
        do {
            let _ = try await task.value
            generate3DImageTask = nil
            scheduleUpdate()
        } catch is CancellationError {
            // The generated3DImageTask was cancelled.
            generate3DImageTask = nil
            spatial3DImage = nil
            Logger.separatedImage.log("\(self.logPrefix) - Cancelled Image Generation.")
        } catch {
            generate3DImageTask = nil
            spatial3DImage = nil
            Logger.separatedImage.error("\(self.logPrefix) - Generation exception: \(error).")
        }
        #endif
    }

    func preparePortalEntity() {
        #if canImport(RealityFoundation, _version: 387)
        guard let spatial3DImage, portalEntity == nil else { return }

        let portalEntity = ModelEntity()
        var ipc = ImagePresentationComponent(spatial3DImage: spatial3DImage)
        ipc.desiredViewingMode = desiredViewingModeSpatial ? .spatial3D : .mono
        #if USE_APPLE_INTERNAL_SDK
        ipc.cornerRadiusInPoints = Float(layer.cornerRadius)
        #endif
        portalEntity.components[ImagePresentationComponent.self] = ipc
        self.portalEntity = portalEntity
        Logger.separatedImage.log("\(self.logPrefix) - Created an IPC (spatial = \(self.desiredViewingModeSpatial)).")

        scheduleUpdate()
        #endif
    }

    func updateUI() {
        #if canImport(RealityFoundation, _version: 387)
        if isSeparated, viewMode == .portal, spatial3DImage == nil, let imageHash, ImagePresentationCache.shared[imageHash] == nil {
            if spinner == nil {
                spinner = WKSeparatedImageView.generationQueueSpinnerView()
                spinner?.alpha = 0
            }
            if let spinner {
                if spinner.superview == nil {
                    cornerView.replaceSubviews(with: spinner)
                }
                spinner.updateVisibility(visible: true, animate: true, duration: SeparatedImageViewConstants.longFadeAnimationDuration)
                spinner.startAnimating()
            }
        }

        if !isSeparated || spatial3DImage != nil, let spinner {
            spinner.updateVisibility(visible: false, animate: true, duration: SeparatedImageViewConstants.longFadeAnimationDuration) {
                [weak self] in
                self?.spinner?.stopAnimating()
            }
        }

        if isSeparated, isRenderingIPC, generate3DImageTask == nil {
            if toggleHostingView == nil {
                toggleHostingView = desiredViewModeToggleView()
                toggleHostingView?.alpha = 0
            }
            if let toggleHostingView {
                if toggleHostingView.superview == nil {
                    cornerView.replaceSubviews(with: toggleHostingView)
                }
                toggleHostingView.updateVisibility(
                    visible: true,
                    animate: true,
                    duration: SeparatedImageViewConstants.longFadeAnimationDuration
                )
            }
        }

        if !isRenderingIPC, let toggleHostingView {
            toggleHostingView.updateVisibility(visible: false, animate: false) { [weak self] in
                self?.toggleHostingView = nil
            }
        }
        #endif
    }

    static func generationQueueSpinnerView() -> UIActivityIndicatorView {
        let spinnerView = UIActivityIndicatorView(style: .medium)
        spinnerView.center = CGPoint(x: SeparatedImageViewConstants.cornerUISize / 2, y: SeparatedImageViewConstants.cornerUISize / 2)
        spinnerView.layer.shadowColor = UIColor.black.cgColor
        spinnerView.layer.shadowOpacity = 0.3
        spinnerView.layer.shadowOffset = CGSize(width: 0, height: 1)
        spinnerView.layer.shadowRadius = 2
        return spinnerView
    }

    func desiredViewModeToggleView() -> UIView {
        let toggleView = _UIHostingView(
            rootView: DesiredViewModeButton(
                isSpatial: desiredViewingModeSpatial,
                onSpatialChange: { [weak self] newValue in
                    self?.desiredViewingModeSpatial = newValue
                }
            )
        )
        toggleView.frame = CGRect(
            x: 0,
            y: 0,
            width: SeparatedImageViewConstants.cornerUISize,
            height: SeparatedImageViewConstants.cornerUISize
        )
        return toggleView
    }

    func nextDisplayLink() async {
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            let target = DisplayLinkTarget(continuation)
            guard let screen = window?.windowScene?.screen,
                let link = screen.displayLink(withTarget: target, selector: #selector(DisplayLinkTarget.handle(_:)))
            else {
                continuation.resume()
                return
            }

            link.add(to: .main, forMode: .common)
        }
    }

    enum ViewMode {
        case unknown
        case small
        case inaesthetic
        case failed
        case portal

        var description: String {
            switch self {
            case .portal:
                "ImagePresentationComponent"
            case .unknown, .small, .inaesthetic, .failed:
                "Simple Image"
            }
        }
    }
}

struct DesiredViewModeButton: View {
    @State
    var isSpatial: Bool
    let onSpatialChange: (Bool) -> Void

    var body: some View {
        Toggle(isOn: $isSpatial) {
            // FIXME: rdar://153905344 (Add a localized accessibility label)
            Image(systemName: "spatial.capture")
                .symbolVariant(isSpatial ? .fill : .none)
        }
        .controlSize(.large)
        .toggleStyle(.button)
        .buttonBorderShape(.circle)
        #if USE_APPLE_INTERNAL_SDK
        .platterVirtualContentOnlyBlur(isSpatial)
        .glassBackgroundEffect(displayMode: isSpatial ? .never : .always)
        #endif
        .offset(z: 4)
        .onChange(of: isSpatial) { _, newValue in
            onSpatialChange(newValue)
        }
    }
}

private final class DisplayLinkTarget: NSObject {
    var continuation: CheckedContinuation<Void, Never>?
    init(_ continuation: CheckedContinuation<Void, Never>) {
        self.continuation = continuation
    }
    @objc
    func handle(_ link: CADisplayLink) {
        continuation?.resume()
        link.invalidate()
        continuation = nil
    }
}

extension CALayer {
    fileprivate var effectivePointsPerMeter: CGFloat {
        let defaultPointsPerMeter: CGFloat = 1360

        var layer: CALayer? = self
        while let currentLayer = layer {
            if let pointsPerMeter = currentLayer.value(forKeyPath: "separatedOptions.pointsPerMeter") as? CGFloat {
                return pointsPerMeter
            }
            layer = currentLayer.superlayer
        }

        return defaultPointsPerMeter
    }
}

extension UIView {
    fileprivate func replaceSubviews(with newSubview: UIView) {
        for subview in subviews {
            subview.removeFromSuperview()
        }
        addSubview(newSubview)
    }

    fileprivate func updateVisibility(visible: Bool, animate: Bool, duration: CGFloat = 0.0, animationCompletion: (() -> Void)? = nil) {
        let targetAlpha: CGFloat = visible ? 1.0 : 0.0
        guard alpha != targetAlpha else {
            return
        }

        if animate {
            UIView.animate(
                withDuration: duration,
                animations: {
                    self.alpha = targetAlpha
                },
                completion: { _ in
                    animationCompletion?()
                }
            )
        } else {
            alpha = targetAlpha
            animationCompletion?()
        }
    }
}

#endif

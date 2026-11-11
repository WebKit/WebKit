// Copyright (C) 2026 Apple Inc. All rights reserved.
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
// THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_WEBXR && WTF_PLATFORM_VISION

import CompositorServices
import Metal
import SwiftUI
import Synchronization
import UIKit
import os

@objc
@implementation
extension WKWebXRImmersiveSpaceHost {
    @nonobjc
    fileprivate nonisolated static let immersiveSpaceID = "com.apple.WebKit.WebXRImmersiveSpace"

    @nonobjc
    private nonisolated static let log = Logger(subsystem: "com.apple.WebKit", category: "XR")

    @nonobjc
    private static var pendingContinuation: CheckedContinuation<SessionResult, Never>?

    @nonobjc
    fileprivate static var requestedFormats = RequestedFormats()

    @nonobjc
    fileprivate static var layerConfigurator: ((__cp_layer_renderer_configuration_t) -> Void)?

    @nonobjc
    private static var connectedWindowScene: UIWindowScene?

    @nonobjc
    private static var isImmersiveSpacePresented = false

    @nonobjc
    fileprivate nonisolated static let scenesToOrderOut = Mutex<Set<String>>([])

    @MainActor
    private struct SessionResult {
        let layerRenderer: LayerRenderer?
        let viewController: UIViewController?
    }

    fileprivate struct RequestedFormats {
        var colorFormat: MTLPixelFormat = .invalid
        var depthFormat: MTLPixelFormat = .invalid
        var usesWebGPU = false

        var resolvedColorFormat: MTLPixelFormat { colorFormat == .invalid ? .bgra8Unorm_srgb : colorFormat }
        var resolvedDepthFormat: MTLPixelFormat { depthFormat == .invalid ? .depth32Float : depthFormat }
    }

    class func startSession(
        colorFormat: MTLPixelFormat,
        depthFormat: MTLPixelFormat,
        usesWebGPU: Bool,
        configureLayer: @escaping (__cp_layer_renderer_configuration_t) -> Void
    ) async -> (LayerRenderer?, UIViewController?) {
        guard pendingContinuation == nil else {
            log.error("Declining WebXR session request: another request is already in flight")
            return (nil, nil)
        }

        guard !isImmersiveSpacePresented, connectedWindowScene == nil else {
            log.error("Declining WebXR session request: an immersive space is already presented")
            return (nil, nil)
        }

        guard
            let request = UISceneSessionActivationRequest(
                hostingDelegateClass: WKWebXRImmersiveSpaceSceneDelegate.self,
                id: immersiveSpaceID
            )
        else {
            log.error("Declining WebXR session request: could not create a scene activation request")
            return (nil, nil)
        }

        requestedFormats = RequestedFormats(colorFormat: colorFormat, depthFormat: depthFormat, usesWebGPU: usesWebGPU)
        layerConfigurator = configureLayer

        let openScenes = Set(UIApplication.shared.connectedScenes.map(\.session.persistentIdentifier))
        scenesToOrderOut.withLock { $0 = openScenes }

        let result = await withCheckedContinuation { (continuation: CheckedContinuation<SessionResult, Never>) in
            pendingContinuation = continuation

            UIApplication.shared.activateSceneSession(for: request) { error in
                log.error("Failed to activate the WebXR immersive space: \(error.localizedDescription, privacy: .public)")
                Task { @MainActor in
                    finishPendingRequest(layerRenderer: nil, viewController: nil)
                }
            }
        }
        return (result.layerRenderer, result.viewController)
    }

    class func endSession() {
        finishPendingRequest(layerRenderer: nil, viewController: nil)

        guard let windowScene = connectedWindowScene else { return }
        isImmersiveSpacePresented = false

        UIApplication.shared.requestSceneSessionDestruction(windowScene.session, options: nil) { error in
            log.error("Failed to destroy the WebXR immersive space: \(error.localizedDescription, privacy: .public)")
        }
    }

    @nonobjc
    fileprivate static func sceneWillConnect(_ windowScene: UIWindowScene) {
        connectedWindowScene = windowScene
    }

    @nonobjc
    fileprivate static func sceneDidDisconnect() {
        connectedWindowScene = nil
        isImmersiveSpacePresented = false
        finishPendingRequest(layerRenderer: nil, viewController: nil)
    }

    @nonobjc
    fileprivate static func didReceive(layerRenderer: LayerRenderer) {
        guard let viewController = connectedWindowScene?.keyWindow?.rootViewController else {
            log.error("Immersive space produced a layer renderer but its scene has no view controller; ending the session")
            endSession()
            return
        }

        log.log("WebXR immersive space is ready, handing the layer renderer to the compositor")
        isImmersiveSpacePresented = true
        finishPendingRequest(layerRenderer: layerRenderer, viewController: viewController)
    }

    @nonobjc
    private static func finishPendingRequest(layerRenderer: LayerRenderer?, viewController: UIViewController?) {
        guard let continuation = pendingContinuation else { return }
        pendingContinuation = nil
        continuation.resume(returning: SessionResult(layerRenderer: layerRenderer, viewController: viewController))
    }
}

@MainActor
final class WKWebXRImmersiveSpaceSceneDelegate: NSObject, UISceneDelegate, UIHostingSceneDelegate {
    nonisolated static var rootScene: some Scene {
        ImmersiveSpace(id: WKWebXRImmersiveSpaceHost.immersiveSpaceID) {
            WKWebXRImmersiveSpaceContent()
        }
        .immersionStyle(selection: .constant(.full), in: .full)
        ._orderOutScenes {
            WKWebXRImmersiveSpaceHost.scenesToOrderOut.withLock { $0 }
        }
    }

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = scene as? UIWindowScene else { return }
        WKWebXRImmersiveSpaceHost.sceneWillConnect(windowScene)
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        WKWebXRImmersiveSpaceHost.sceneDidDisconnect()
    }
}

private struct WKWebXRImmersiveSpaceContent: CompositorContent {
    var body: some CompositorContent {
        CompositorLayer(configuration: WKWebXRLayerConfiguration()) { layerRenderer in
            WKWebXRImmersiveSpaceHost.didReceive(layerRenderer: layerRenderer)
        }
    }
}

private struct WKWebXRLayerConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities, configuration: inout LayerRenderer.Configuration) {
        let formats = WKWebXRImmersiveSpaceHost.requestedFormats

        configuration.colorFormat = formats.resolvedColorFormat

        #if !WTF_PLATFORM_IOS_FAMILY_SIMULATOR
        configuration.layout = .layered
        if formats.usesWebGPU {
            configuration.depthFormat = formats.resolvedDepthFormat
            configuration.depthUsage = [.renderTarget, .shaderRead, .pixelFormatView]
        } else {
            configuration.depthFormat = .depth32Float_stencil8
        }
        #else
        configuration.colorUsage = [.renderTarget, .shaderRead, .pixelFormatView]
        configuration.depthUsage = [.renderTarget, .shaderRead, .pixelFormatView]
        #endif

        let layerConfiguration = configuration as __CP_OBJECT_cp_layer_renderer_configuration
        WKWebXRImmersiveSpaceHost.layerConfigurator?(layerConfiguration)
        configuration = layerConfiguration as LayerRenderer.Configuration
    }
}

#endif // ENABLE_WEBXR && WTF_PLATFORM_VISION

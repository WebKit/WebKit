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

// Hosts the remote immersive space that backs WebXR on macOS.
//
// WebKit asks for a session through -_webView:startXRSessionWithFeatures:colorFormat:
// depthFormat:completionHandler:, and expects back the CompositorServices layer renderer to
// draw into plus the ARKit device supplying tracking data. On macOS both come from a
// SwiftUI RemoteImmersiveSpace presented on a paired headset, so this file bridges the
// AppKit app lifecycle to that scene with NSHostingSceneRepresentation.

#if WTF_PLATFORM_MAC

import AppKit
import ARKit
import Metal
import SwiftUI
import os

#if canImport(CompositorServices_Private)
import CompositorServices_Private
@_spi(Private) import CompositorServices
#else
import CompositorServices
#endif

@available(macOS 26.0, *)
@objc(WebXRImmersiveSpaceHost)
final class WebXRImmersiveSpaceHost: NSObject {
    private static let immersiveSpaceID = "org.webkit.MiniBrowser.WebXRImmersiveSpace"

    private static let log = Logger(subsystem: "org.webkit.MiniBrowser", category: "WebXR")

    /// Completion handler for the in-flight -startXRSession… request, if any.
    @MainActor
    private static var pendingCompletionHandler: ((Any?, NSViewController?, Any?) -> Void)?

    /// Texture formats and usage requested by the page's XRGPUBinding, read by
    /// WebXRLayerConfiguration when the scene builds its layer.
    @MainActor
    fileprivate static var requestedFormats = RequestedFormats()

    @MainActor
    private static var isImmersiveSpacePresented = false
    @MainActor
    private static var hasAddedSceneRepresentation = false

    @MainActor
    private static let sceneRepresentation = NSHostingSceneRepresentation {
        RemoteImmersiveSpace(id: immersiveSpaceID) {
            WebXRImmersiveSpaceContent()
        }
    }

    fileprivate struct RequestedFormats {
        var colorFormat: MTLPixelFormat = .invalid
        var depthFormat: MTLPixelFormat = .invalid
        var usesWebGPU = false

        // WebKit passes MTLPixelFormatInvalid when the session has no XRGPUBinding to
        // describe, which is the WebGL path.
        var resolvedColorFormat: MTLPixelFormat { colorFormat == .invalid ? .bgra8Unorm_srgb : colorFormat }
        var resolvedDepthFormat: MTLPixelFormat { depthFormat == .invalid ? .depth32Float : depthFormat }
    }

    // MARK: - Entry points called from WK2BrowserWindowController

    /// Presents the immersive space and reports the layer renderer and ARKit device back to WebKit.
    ///
    /// Calls the completion handler with nils if the session cannot start.
    @objc(startSessionWithColorFormat:depthFormat:usesWebGPU:completionHandler:)
    @MainActor
    static func startSession(
        colorFormat: MTLPixelFormat,
        depthFormat: MTLPixelFormat,
        usesWebGPU: Bool,
        completionHandler: @escaping (Any?, NSViewController?, Any?) -> Void
    ) {
        guard pendingCompletionHandler == nil else {
            log.error("Declining WebXR session request: another request is already in flight")
            completionHandler(nil, nil, nil)
            return
        }

        if isImmersiveSpacePresented {
            log.error("An immersive space is still presented; dismissing it before starting a new session")
            isImmersiveSpacePresented = false
            Task { @MainActor in
                await sceneRepresentation.environment.dismissImmersiveSpace()
                startSession(
                    colorFormat: colorFormat,
                    depthFormat: depthFormat,
                    usesWebGPU: usesWebGPU,
                    completionHandler: completionHandler
                )
            }
            return
        }

        if !hasAddedSceneRepresentation {
            NSApplication.shared.addSceneRepresentation(sceneRepresentation)
            hasAddedSceneRepresentation = true
        }

        guard sceneRepresentation.environment.supportsRemoteScenes else {
            log.error("Declining WebXR session request: this Mac cannot present remote scenes. Pair an Apple Vision Pro and try again.")
            completionHandler(nil, nil, nil)
            return
        }

        requestedFormats = RequestedFormats(colorFormat: colorFormat, depthFormat: depthFormat, usesWebGPU: usesWebGPU)
        pendingCompletionHandler = completionHandler

        Task { @MainActor in
            switch await sceneRepresentation.environment.openImmersiveSpace(id: immersiveSpaceID) {
            case .opened:
                // WebKit is told about the session from didReceive(layerRenderer:…), which
                // the CompositorLayer renderer closure drives.
                isImmersiveSpacePresented = true
            case .userCancelled:
                log.log("WebXR session request cancelled by the user")
                finishPendingRequest(layerRenderer: nil, remoteDevice: nil)
            case .error:
                log.error("Failed to open the WebXR immersive space")
                finishPendingRequest(layerRenderer: nil, remoteDevice: nil)
            @unknown default:
                log.error("Unexpected result opening the WebXR immersive space")
                finishPendingRequest(layerRenderer: nil, remoteDevice: nil)
            }
        }
    }

    /// Tears the immersive space down.
    ///
    /// Safe to call when nothing is presented.
    @objc(endSession)
    @MainActor
    static func endSession() {
        // A request that never produced a layer renderer still owes WebKit a reply.
        finishPendingRequest(layerRenderer: nil, remoteDevice: nil)

        guard isImmersiveSpacePresented else { return }
        isImmersiveSpacePresented = false

        Task { @MainActor in
            await sceneRepresentation.environment.dismissImmersiveSpace()
        }
    }

    // MARK: - Called from the immersive space scene

    @MainActor
    fileprivate static func didReceive(layerRenderer: LayerRenderer, deviceIdentifier: RemoteDeviceIdentifier?) {
        guard let deviceIdentifier else {
            // Every ARKit data provider on a Mac has to be bound to the headset's device, so
            // without one there would be no pose data and the render loop would stall.
            log.error("Immersive space produced a layer renderer with no remote device; ending the session")
            endSession()
            return
        }

        log.log("WebXR immersive space is ready, handing the layer renderer to WebKit")
        finishPendingRequest(layerRenderer: layerRenderer, remoteDevice: deviceIdentifier.cDevice)
    }

    @MainActor
    private static func finishPendingRequest(layerRenderer: LayerRenderer?, remoteDevice: ar_device_t?) {
        guard let completionHandler = pendingCompletionHandler else { return }
        pendingCompletionHandler = nil

        // There is no view controller for a remote immersive space. WebKit only uses it for
        // visibility propagation and gesture recognizers, both of which are iOS-family only.
        completionHandler(layerRenderer, nil, remoteDevice)
    }
}

// MARK: - Immersive space content

@available(macOS 26.0, *)
private struct WebXRImmersiveSpaceContent: CompositorContent {
    @Environment(\.remoteDeviceIdentifier)
    private var remoteDeviceIdentifier

    var body: some CompositorContent {
        CompositorLayer(configuration: WebXRLayerConfiguration()) { layerRenderer in
            WebXRImmersiveSpaceHost.didReceive(layerRenderer: layerRenderer, deviceIdentifier: remoteDeviceIdentifier)
        }
    }
}

@available(macOS 26.0, *)
private struct WebXRLayerConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities, configuration: inout LayerRenderer.Configuration) {
        let formats = WebXRImmersiveSpaceHost.requestedFormats

        configuration.layout = .layered
        configuration.colorFormat = formats.resolvedColorFormat

        if formats.usesWebGPU {
            // The depth texture is shared with the web process and sampled by the
            // compositor, so it needs to be readable and reinterpretable.
            configuration.depthFormat = formats.resolvedDepthFormat
            configuration.depthUsage = [.renderTarget, .shaderRead, .pixelFormatView]
        } else {
            configuration.depthFormat = .depth32Float_stencil8
        }

        if UserDefaults.standard.bool(forKey: "WebKitXRDisableFoveation") {
            configuration.isFoveationEnabled = false
        }

        #if canImport(CompositorServices_Private)
        if !formats.usesWebGPU {
            // WebGL renders bottom-up.
            cp_layer_renderer_configuration_set_contents_inverted_vertically(configuration.c_configuration, true)
        }

        // CompositorCoordinator reads the layer's completion event as an MTLSharedEvent so
        // it can hand a port to the web process. Without this the event is process-local.
        cp_layer_renderer_configuration_set_use_shared_events(configuration.c_configuration, true)
        #endif
    }
}

#endif // WTF_PLATFORM_MAC

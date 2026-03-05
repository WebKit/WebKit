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

#if ENABLE_SWIFTUI

import Foundation
internal import WebKit_Private
internal import WebKit_Internal

#if os(macOS) && !targetEnvironment(macCatalyst)
@_spiOnly import WebKit_Private._WKContextMenuElementInfo
@_spiOnly import WebKit_Private._WKHitTestResult
#endif

private struct DefaultDialogPresenting: WebPage.DialogPresenting {
}

@MainActor
final class WKUIDelegateAdapter: NSObject, WKUIDelegatePrivate {
    init(dialogPresenter: (any WebPage.DialogPresenting)?) {
        self.dialogPresenter = dialogPresenter ?? DefaultDialogPresenting()
    }

    weak var owner: WebPage? = nil

    #if os(macOS) && !targetEnvironment(macCatalyst)
    var menuBuilder: ((WKContextMenuElementInfoAdapter) -> NSMenu)? = nil
    #endif

    private let dialogPresenter: any WebPage.DialogPresenting

    // MARK: Dialog presentation

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo) async {
        await dialogPresenter.handleJavaScriptAlert(message: message, initiatedBy: .init(frame))
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo
    ) async -> Bool {
        let result = await dialogPresenter.handleJavaScriptConfirm(message: message, initiatedBy: .init(frame))

        return switch result {
        case .ok: true
        case .cancel: false
        }
    }

    func webView(
        _ webView: WKWebView,
        runJavaScriptTextInputPanelWithPrompt prompt: String,
        defaultText: String?,
        initiatedByFrame frame: WKFrameInfo
    ) async -> String? {
        let result = await dialogPresenter.handleJavaScriptPrompt(message: prompt, defaultText: defaultText, initiatedBy: .init(frame))

        return switch result {
        case .ok(let value): value
        case .cancel: nil
        }
    }

    func webView(
        _ webView: WKWebView,
        runOpenPanelWith parameters: WKOpenPanelParameters,
        initiatedByFrame frame: WKFrameInfo
    ) async -> [URL]? {
        let result = await dialogPresenter.handleFileInputPrompt(parameters: parameters, initiatedBy: .init(frame))

        return switch result {
        case .selected(let value): value
        case .cancel: nil
        }
    }

    // MARK: Permissions

    func webView(
        _ webView: WKWebView,
        requestDeviceOrientationAndMotionPermissionFor origin: WKSecurityOrigin,
        initiatedByFrame frame: WKFrameInfo
    ) async -> WKPermissionDecision {
        guard let owner else {
            return .prompt
        }

        return await owner.configuration.deviceSensorAuthorization.decisionHandler(.deviceOrientationAndMotion, .init(frame), origin)
    }

    func webView(
        _ webView: WKWebView,
        decideMediaCapturePermissionsFor origin: WKSecurityOrigin,
        initiatedBy frame: WKFrameInfo,
        type: WKMediaCaptureType
    ) async -> WKPermissionDecision {
        guard let owner else {
            return .prompt
        }

        return await owner.configuration.deviceSensorAuthorization.decisionHandler(.mediaCapture(type), .init(frame), origin)
    }

    // MARK: Context menu support

    #if os(macOS)
    // swift-format-ignore: NoLeadingUnderscores
    @objc(_webView:getContextMenuFromProposedMenu:forElement:userInfo:completionHandler:)
    func _webView(
        _ webView: WKWebView!,
        getContextMenuFromProposedMenu menu: NSMenu!,
        forElement element: _WKContextMenuElementInfo!,
        userInfo: (any NSSecureCoding)!
    ) async -> NSMenu? {
        guard let menuBuilder else {
            return menu
        }

        let info = WKContextMenuElementInfoAdapter(linkURL: element.hitTestResult.absoluteLinkURL)
        return menuBuilder(info)
    }
    #endif

    // swift-format-ignore: NoLeadingUnderscores
    @objc(_webView:geometryDidChange:)
    func _webView(_ webView: WKWebView!, geometryDidChange geometry: WKScrollGeometry) {
        owner?.backingWebView.geometryDidChange(WKScrollGeometryAdapter(geometry))
    }
}

#endif

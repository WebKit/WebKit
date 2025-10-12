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

import CoreTransferable
import Foundation
import Observation
import UniformTypeIdentifiers
@_spi(Private) @_spi(CrossImportOverlay) import WebKit
import os

struct PDF {
    let data: Data
    let title: String?
}

extension PDF: Transferable {
    static var transferRepresentation: some TransferRepresentation {
        DataRepresentation(exportedContentType: .pdf, exporting: \.data)
    }
}

struct OpenRequest: Equatable {
    let request: URLRequest
}

@Observable
@MainActor
final class BrowserViewModel {
    private static let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: String(describing: BrowserViewModel.self))

    private static func decideSensorAuthorization(permission: WebPage.DeviceSensorAuthorization.Permission, frame: WebPage.FrameInfo, origin: WKSecurityOrigin) async -> WKPermissionDecision {
        let mediaCaptureAuthorization = WKPermissionDecision(rawValue: UserDefaults.standard.integer(forKey: AppStorageKeys.mediaCaptureAuthorization))!
        let orientationAndMotionAuthorization = WKPermissionDecision(rawValue: UserDefaults.standard.integer(forKey: AppStorageKeys.orientationAndMotionAuthorization))!

        return switch permission {
        case .deviceOrientationAndMotion: orientationAndMotionAuthorization
        case .mediaCapture: mediaCaptureAuthorization
        @unknown default:
            fatalError()
        }
    }

    init() {
        var configuration = WebPage.Configuration()
        configuration.deviceSensorAuthorization = WebPage.DeviceSensorAuthorization(decisionHandler: Self.decideSensorAuthorization(permission:frame:origin:))

        self.page = WebPage(configuration: configuration, navigationDecider: self.navigationDecider, dialogPresenter: self.dialogPresenter)

        self.navigationDecider.owner = self
        self.dialogPresenter.owner = self
    }

    let page: WebPage

    private let dialogPresenter = DialogPresenter()
    private let navigationDecider = NavigationDecider()

    var displayedURL: String = ""

    var currentOpenRequest: OpenRequest? = nil

    // MARK: PDF properties

    var pdfExporterIsPresented = false {
        didSet {
            if !pdfExporterIsPresented {
                exportedPDF = nil
            }
        }
    }

    private(set) var exportedPDF: PDF? = nil {
        didSet {
            if exportedPDF != nil {
                pdfExporterIsPresented = true
            }
        }
    }

    // MARK: Dialog properties

    var isPresentingDialog = false {
        didSet {
            if !isPresentingDialog {
                currentDialog = nil
            }
        }
    }

    var currentDialog: DialogPresenter.Dialog? = nil {
        didSet {
            if currentDialog != nil {
                isPresentingDialog = true
            }
        }
    }

    var isPresentingFilePicker = false

    var currentFilePicker: DialogPresenter.FilePicker? = nil {
        didSet {
            if currentFilePicker != nil {
                isPresentingFilePicker = true
            }
        }
    }

    // MARK: View model functions

    func openURL(_ url: URL) {
        assert(url.isFileURL)

        let data = try! Data(contentsOf: url)
        page.load(data, mimeType: "text/html", characterEncoding: .utf8, baseURL: URL(string: "about:blank")!)
    }

    func didReceiveNavigationEvent(_ event: WebPage.NavigationEvent) {
        Self.logger.info("Did receive navigation event \(String(describing: event))")

        if event == .committed {
            displayedURL = page.url?.absoluteString ?? ""
        }
    }

    func navigateToSubmittedURL() {
        guard let url = URL(string: displayedURL) else {
            return
        }

        let request = URLRequest(url: url)
        page.load(request)
    }

    func exportAsPDF() {
        Task {
            let data = try await page.exported(as: .pdf)
            exportedPDF = PDF(data: data, title: !page.title.isEmpty ? page.title : nil)
        }
    }

    func didExportPDF(result: Result<URL, any Error>) {
        switch result {
        case let .success(url):
            Self.logger.info("Exported PDF to \(url)")

        case let .failure(error):
            Self.logger.error("Failed to export PDF: \(error)")
        }
    }

    func didImportFiles(result: Result<[URL], any Error>) {
        precondition(currentFilePicker != nil)

        switch result {
        case let .success(urls):
            currentFilePicker!.completion(.selected(urls))

        case .failure:
            currentFilePicker!.completion(.cancel)
        }

        currentFilePicker = nil
    }

    func setCameraCaptureState(_ state: WKMediaCaptureState) {
        Task { @MainActor in
            await page.setCameraCaptureState(state)
        }
    }

    func setMicrophoneCaptureState(_ state: WKMediaCaptureState) {
        Task { @MainActor in
            await page.setMicrophoneCaptureState(state)
        }
    }

    func updateWebPreferences() {
        let preferences = page.backingWebView.configuration.preferences
        for feature in WKPreferences._features() {
            guard let value = UserDefaults.standard.object(forKey: feature.key) as? Bool else {
                continue
            }

            preferences._setEnabled(value, for: feature)
        }
    }
}

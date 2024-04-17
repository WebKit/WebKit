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

#if os(visionOS)

#if canImport(QuickLook, _version: 957)
import OSLog
import WebKitSwift

@_spi(PreviewApplication) import QuickLook

@objc(WKSPreviewWindowController)
public final class PreviewWindowController: NSObject {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "Fullscreen")

    private let item: PreviewItem
    private let previewConfiguration: PreviewApplication.PreviewConfiguration
    private var previewSession: PreviewSession?
    private var isClosing = false

    @objc public weak var delegate: WKSPreviewWindowControllerDelegate?

    @objc(initWithURL:sceneID:) public init(url: URL, sceneID: String) {
        self.item = PreviewItem(url: url, displayName: nil, editingMode: .disabled);

        var configuration = PreviewApplication.PreviewConfiguration()
        configuration.hideDocumentMenu = true
        configuration.showCloseButton = true
        configuration.matchScenePlacementID = sceneID
        self.previewConfiguration = configuration

        super.init()
    }

    @objc public func presentWindow() {
        previewSession = PreviewApplication.open(items: [self.item], selectedItem: nil, configuration: previewConfiguration)

        Task.detached { [weak self] in
            guard let session = self?.previewSession else { return }
            for await event in session.events {
                DispatchQueue.main.async { [weak self] in
                    guard let self = self else { return }
                    switch event {
                    case .didFail(let error):
                        self.isClosing = true
                        self.delegate?.previewWindowControllerDidClose(self)
                        PreviewWindowController.logger.error("Preview open failed with error \(error)")
                    case .didClose:
                        self.isClosing = true
                        self.delegate?.previewWindowControllerDidClose(self)
                    default:
                        break
                    }
                }
            }
        }
    }

    @objc public func dismissWindow() {
        guard !isClosing else { return }

        Task {
            do {
                try await previewSession?.close();
            } catch {
                PreviewWindowController.logger.error("WKSPreviewWindowController.dismissWindow failed: \(error, privacy: .public)")
            }
        }
    }
}

#else
import Foundation
@objc(WKSPreviewWindowController)
public final class PreviewWindowController: NSObject { }
#endif

#endif

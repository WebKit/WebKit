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

#if canImport(AssetViewer)
import WebKitSwift

@_spi(Preview) import AssetViewer

@objc(WKSPreviewWindowController)
public final class PreviewWindowController: NSObject {
    private static let logger = Logger(subsystem: "com.apple.WebKit", category: "Fullscreen")

    var item: QLItem
    var previewApp: AssetViewer.Preview?
    @objc public weak var delegate: WKSPreviewWindowControllerDelegate?

    @objc public init(item: QLItem) {
        self.item = item
        super.init()

        self.previewApp = Preview(response: { response in
            if case .previewSceneClosed = response {
                if let delegate = self.delegate {
                    delegate.previewWindowControllerDidClose()
                }
            }
        })
    }

    @objc public func presentWindow() {
        Task {
            guard let previewApp = self.previewApp else { return }

            do {
                try await previewApp.launch(with: [self.item], options: .none)
            } catch {
                PreviewWindowController.logger.error("WKSPreviewWindowController.presentWindow failed: \(error, privacy: .public)")
            }
        }
    }

    @objc public func dismissWindow() {
        // TODO: rdar://120903037
    }
}

#endif

#endif

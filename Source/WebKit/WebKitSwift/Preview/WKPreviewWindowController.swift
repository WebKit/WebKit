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

#if ENABLE_QUICKLOOK_FULLSCREEN

import OSLog

#if USE_APPLE_INTERNAL_SDK
@_spi(PreviewApplication) internal import QuickLook
#else
// FIXME: rdar://152166678 Rename this back to `QuickLook_SPI`.
internal import QuickLook_SPI_Temp
#endif

@objc @implementation extension WKPreviewWindowController {
    enum UpdateError: Error {
        case newUpdateQueued
    }

    // Used to workaround the fact that `@objc @implementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @MainActor
    private final class Base {
        var previewSession: PreviewSession?
        var item: PreviewItem
        var previewConfiguration: PreviewApplication.PreviewConfiguration
        var windowOpenedContinuation: CheckedContinuation<Void, Error>?

        init(item: PreviewItem, configuration: PreviewApplication.PreviewConfiguration) {
            self.item = item
            self.previewConfiguration = configuration
        }
    }

    @nonobjc private static let logger = Logger(subsystem: "com.apple.WebKit", category: "Fullscreen")

    @nonobjc private let base: Base

    @nonobjc private var isClosing = false
    @nonobjc private var isOpen = false

    @nonobjc private final var previewSession: PreviewSession? {
        get { base.previewSession }
        set { base.previewSession = newValue }
    }

    @nonobjc private final var item: PreviewItem {
        get { base.item }
        set { base.item = newValue }
    }

    @nonobjc private final var previewConfiguration: PreviewApplication.PreviewConfiguration {
        get { base.previewConfiguration }
        set { base.previewConfiguration = newValue }
    }

    @nonobjc private final var windowOpenedContinuation: CheckedContinuation<Void, Error>? {
        get { base.windowOpenedContinuation }
        set { base.windowOpenedContinuation = newValue }
    }

    weak var delegate: WKPreviewWindowControllerDelegate?

    @objc(initWithURL:sceneID:)
    init(url: URL, sceneID: String) {
        let item = PreviewItem(url: url, displayName: nil, editingMode: .disabled)

        var configuration = PreviewApplication.PreviewConfiguration()
        configuration.hideDocumentMenu = true
        configuration.showCloseButton = true
        configuration.matchScenePlacementID = sceneID

        self.base = Base(item: item, configuration: configuration)

        super.init()
    }

    func presentWindow() async {
        let session = PreviewApplication.open(items: [item], selectedItem: nil, configuration: previewConfiguration)
        self.previewSession = session

        for await event in session.events {
            switch event {
            case .didOpen:
                isOpen = true
                windowOpenedContinuation?.resume()
            case .didFail(let error):
                isClosing = true
                delegate?.previewWindowControllerDidClose(self)
                windowOpenedContinuation?.resume(throwing: error)
                Self.logger.error("Preview open failed with error \(error)")
            case .didClose:
                isClosing = true
                delegate?.previewWindowControllerDidClose(self)
            default:
                break
            }
        }
    }
    
    func updateImage(_ url: URL) async {
        item = PreviewItem(url: url, displayName: nil, editingMode: .disabled);
        
        do {
            if !isOpen {
                if let continuation = windowOpenedContinuation {
                    // The Quick Look window isn't ready yet, but there's already been an earlier update queued
                    // Throw that update and queue this one instead
                    continuation.resume(throwing: UpdateError.newUpdateQueued)
                }
                try await withCheckedThrowingContinuation { continuation in
                    windowOpenedContinuation = continuation
                }
            }
            try await previewSession?.update(items: [item])
        } catch UpdateError.newUpdateQueued {
            Self.logger.debug("WKPreviewWindowController.updateImage skipped: newer image update queued");
        } catch {
            Self.logger.error("WKPreviewWindowController.updateImage failed: \(error, privacy: .public)")
        }
    }

    func dismissWindow() async {
        guard !isClosing else {
            return
        }

        do {
            try await previewSession?.close();
        } catch {
            Self.logger.error("WKPreviewWindowController.dismissWindow failed: \(error, privacy: .public)")
        }
    }
}

#endif // ENABLE_QUICKLOOK_FULLSCREEN

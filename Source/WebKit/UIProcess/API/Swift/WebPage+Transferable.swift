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

#if ENABLE_SWIFTUI && compiler(>=6.0)

public import CoreTransferable
import Foundation
import UniformTypeIdentifiers
@_weakLinked internal import SwiftUI
internal import WebKit_Private
internal import WebKit_Private.WKSnapshotConfigurationPrivate

@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WebPage: Transferable {
    /// A specialized configuration of a specific exportable type that can have specific properties unique to the content type.
    public nonisolated struct ExportedContentConfiguration: Sendable {
        /// Represents a specific semantic region of a webpage.
        public struct Region: Sendable {
            private enum Storage {
                case contents
                case rect(CGRect)
            }

            /// A region that corresponds to a rectangle in the page's coordinate system.
            ///
            /// - Parameter rect: The rectangle to use for the region.
            /// - Returns: A ``Region`` that uses the specified rectangle.
            public static func rect(_ rect: CGRect) -> Self {
                .init(storage: .rect(rect))
            }

            /// The entire region of the webpage.
            public static var contents: Self {
                .init(storage: .contents)
            }

            private let storage: Storage

            private init(storage: Storage) {
                self.storage = storage
            }

            fileprivate var rect: CGRect? {
                switch storage {
                case .contents: nil
                case .rect(let value): value
                }
            }

            fileprivate var usesContentsRect: Bool {
                switch storage {
                case .contents: true
                case .rect(_): false
                }
            }
        }

        fileprivate enum Storage {
            case pdf(Region, allowTransparentBackground: Bool)
            case image(Region, allowTransparentBackground: Bool, snapshotWidth: CGFloat?, afterScreenUpdates: Bool)
        }

        /// A configuration of a webpage for a representation as PDF data.
        ///
        /// - Parameters:
        ///   - region: The region of the page used to generate the PDF.
        ///   - allowTransparentBackground: Indicates whether the PDF may have a transparent background.
        /// - Returns: The PDF configuration of this page that will be used when producing its representation.
        public static func pdf(region: Region = .contents, allowTransparentBackground: Bool = false) -> Self {
            .init(storage: .pdf(region, allowTransparentBackground: allowTransparentBackground))
        }

        /// A configuration of a webpage for a representation as image data.
        ///
        /// - Parameters:
        ///   - region: The region of the page used to generate the image.
        ///   - allowTransparentBackground: Indicates whether the image may have a transparent background.
        ///   - snapshotWidth: The width of the captured image, in points.
        ///
        ///     Use this property to scale the generated image to the specified width. The webpage maintains the aspect ratio of the captured content, but scales it to match the width you specify.
        ///
        ///     The default value of this parameter is `nil`, which returns an image whose size matches the original size of the captured region.
        ///
        ///   - afterScreenUpdates: Indicates whether to take the snapshot after incorporating any pending screen updates.
        ///
        /// - Returns: The image configuration of this page that will be used when producing its representation.
        public static func image(
            region: Region = .contents,
            allowTransparentBackground: Bool = false,
            snapshotWidth: CGFloat? = nil,
            afterScreenUpdates: Bool = true
        ) -> Self {
            .init(
                storage: .image(
                    region,
                    allowTransparentBackground: allowTransparentBackground,
                    snapshotWidth: snapshotWidth,
                    afterScreenUpdates: afterScreenUpdates
                )
            )
        }

        fileprivate let storage: Storage

        private init(storage: Storage) {
            self.storage = storage
        }
    }

    // Protocol requirement; no documentation needed.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public nonisolated static var transferRepresentation: some TransferRepresentation {
        // FIXME: Add more exportable types, like .html, .plainText, .richText, .linkPresentationMetadata, etc.
        // FIXME: Ideally, each of these would be conditionalized on content being loaded on the page.

        // The preferred types are roughly in order of descending losslessness.

        DataRepresentation(exportedContentType: .webArchive) {
            try await $0.webArchiveRepresentation()
        }

        DataRepresentation(exportedContentType: .pdf) {
            try await $0.exported(as: .pdf())
        }

        // FIXME: Support all the types that NSImage/UIImage support (this can't use ProxyRepresentation since it needs to be async).
        // FIXME: This is slightly inefficient since it will be converting data to an image and then back to data.
        DataRepresentation(exportedContentType: .image) {
            try await $0.exported(as: .image())
        }

        // FIXME: See if this can somehow be made synchronous, so then it can use `ProxyRepresentation` with `exportingCondition`.
        DataRepresentation(exportedContentType: .url) {
            try await $0.urlRepresentation()
        }
    }

    /// Using the type's `Transferable` conformance implementation, exports a value as binary data,
    /// optionally with a specified configuration for that type of data.
    ///
    /// For example, you can export a 100 pt by 100 pt region of a webpage as a PDF, and allow it to have a transparent background:
    ///
    /// ```swift
    /// let page = WebPage()
    /// // Load web content and wait for navigation to complete.
    ///
    /// let square = CGRect(x: 0, y: 0, width: 100, height: 100)
    /// let pdf = try await page.exported(as: .pdf(region: .rect(square), allowTransparentBackground: true))
    /// ```
    ///
    /// If no configuration is needed, use the ``Transferable`` conformance of ``WebPage`` directly:
    ///
    /// ```swift
    /// let page = WebPage()
    /// // Load web content and wait for navigation to complete.
    ///
    /// let pdf = try await page.exported(as: .pdf)
    /// ```
    ///
    /// - Parameter representation: A configuration for a representation for a specific type of data with optional customizable properties.
    /// - Returns: The data with the specified representation type.
    /// - Throws: An error if the specified representation cannot be created from the page.
    public nonisolated func exported(as representation: ExportedContentConfiguration) async throws -> Data {
        switch representation.storage {
        case .pdf(let region, let allowTransparentBackground):
            try await pdfRepresentation(region: region, allowTransparentBackground: allowTransparentBackground)
        case .image(let region, let allowTransparentBackground, let snapshotWidth, let afterScreenUpdates):
            try await imageRepresentation(
                region: region,
                allowTransparentBackground: allowTransparentBackground,
                snapshotWidth: snapshotWidth,
                afterScreenUpdates: afterScreenUpdates
            )
        }
    }
}

// MARK: Private helper functions

extension WebPage {
    private func webArchiveRepresentation() async throws -> Data {
        try await withCheckedThrowingContinuation { continuation in
            backingWebView.createWebArchiveData {
                continuation.resume(with: $0)
            }
        }
    }

    private func pdfRepresentation(
        region: ExportedContentConfiguration.Region,
        allowTransparentBackground: Bool
    ) async throws -> Data {
        let configuration = WKPDFConfiguration()
        configuration.rect = region.rect
        configuration.allowTransparentBackground = allowTransparentBackground

        return try await backingWebView.pdf(configuration: configuration)
    }

    private func imageRepresentation(
        region: ExportedContentConfiguration.Region,
        allowTransparentBackground: Bool,
        snapshotWidth: CGFloat?,
        afterScreenUpdates: Bool
    ) async throws -> Data {
        let configuration = WKSnapshotConfiguration()
        configuration.rect = region.rect ?? .null

        #if os(macOS)
        // FIXME: This should not be limited to macOS.
        configuration._usesContentsRect = region.usesContentsRect
        #endif

        configuration._usesTransparentBackground = allowTransparentBackground
        configuration.snapshotWidth = snapshotWidth.map {
            NSNumber(value: $0)
        }
        configuration.afterScreenUpdates = afterScreenUpdates

        let snapshot = try await backingWebView.takeSnapshot(configuration: configuration)

        #if os(macOS)
        return try await snapshot.exported(as: .image)
        #else
        guard #_hasSymbol(Image.self) else {
            throw WKError(.unknown)
        }
        let image = Image(uiImage: snapshot)
        return try await image.exported(as: .image)
        #endif
    }

    private func urlRepresentation() async throws -> Data {
        guard let url else {
            throw WKError(.unknown)
        }

        return try await url.exported(as: .url)
    }
}

#endif // ENABLE_SWIFTUI && compiler(>=6.0)

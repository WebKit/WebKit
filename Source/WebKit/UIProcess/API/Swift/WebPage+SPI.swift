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

#if ENABLE_SWIFTUI

import Observation
import WebKit_Private
import WebKit_Internal

// MARK: CrossImportOverlay SPI

extension WebPage {
    #if WTF_PLATFORM_MAC
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func setMenuBuilder(_ menuBuilder: ((WKContextMenuElementInfoAdapter) -> NSMenu)?) {
        backingUIDelegate.menuBuilder = menuBuilder
    }
    #endif // WTF_PLATFORM_MAC

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func backingProperty<Value, BackingValue>(
        _ keyPath: KeyPath<WebPage, Value>,
        backedBy backingKeyPath: KeyPath<WebPageWebView, BackingValue>,
        _ transform: (BackingValue) -> Value
    ) -> Value {
        if observations.contents[keyPath] == nil {
            observations.contents[keyPath] = createObservation(for: keyPath, backedBy: backingKeyPath)
        }

        self.access(keyPath: keyPath)

        let backingValue = backingWebView[keyPath: backingKeyPath]
        return transform(backingValue)
    }

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(CrossImportOverlay)
    public func backingProperty<Value>(_ keyPath: KeyPath<WebPage, Value>, backedBy backingKeyPath: KeyPath<WebPageWebView, Value>) -> Value
    {
        backingProperty(keyPath, backedBy: backingKeyPath) { $0 }
    }
}

// MARK: Testing SPI

extension WebPage {
    /// Represents details about the current editor state.
    @_spi(Testing)
    public struct EditorStateSnapshot: Sendable, Equatable {
        /// The post-layout data associated with an editor state.
        @_spi(Testing)
        public struct PostLayoutData: Sendable, Equatable {
            /// The current selection is bold.
            @_spi(Testing)
            public let bold: Bool

            /// The current selection is italic.
            @_spi(Testing)
            public let italic: Bool

            /// The current selection is underlined.
            @_spi(Testing)
            public let underline: Bool

            /// The text alignment of the current selection.
            @_spi(Testing)
            public let textAlignment: NSTextAlignment

            /// The CSS color string of the current selection.
            @_spi(Testing)
            public let textColor: Swift.String
        }

        /// A type of selection.
        @_spi(Testing)
        public enum SelectionType: Int, Sendable, Equatable {
            /// No selection.
            case none

            /// A caret selection.
            case caret

            /// A range selection.
            case range
        }

        /// The current type of selection.
        @_spi(Testing)
        public let selectionType: SelectionType

        /// The post-layout data of the editor state, if any.
        @_spi(Testing)
        public let postLayoutData: PostLayoutData?
    }

    // SPI for testing.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(Testing)
    public func terminateWebContentProcess() {
        backingWebView._killWebContentProcess()
    }

    /// An indefinite sequence of editor state snapshot changes for this page.
    @_spi(Testing)
    public func editorStateSnapshots() -> some AsyncSequence<EditorStateSnapshot, Never> & Sendable {
        let id = UUID()

        let (stream, continuation) = AsyncStream.makeStream(of: EditorStateSnapshot.self)
        continuation.onTermination = { [weak self] termination in
            guard let self else {
                return
            }
            Task { @MainActor in
                editorStateSnapshotsContinuations[id] = nil
            }
        }

        editorStateSnapshotsContinuations[id] = continuation
        return stream
    }

    #if WTF_PLATFORM_MAC
    // SPI for testing.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(Testing)
    public var smartListsEnabled: Bool {
        get { backingWebView._isSmartListsEnabled() }
        set { backingWebView._setSmartListsEnabled(newValue) }
    }
    #endif // WTF_PLATFORM_MAC

    // SPI for testing.
    // FIXME: This should probably just use the SwiftUI `MagnifyGesture` API.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @_spi(Testing)
    public var magnification: CGFloat {
        get {
            #if WTF_PLATFORM_MAC
            backingWebView.magnification
            #else
            backingWebView.scrollView.zoomScale
            #endif
        }
        set {
            #if WTF_PLATFORM_MAC
            backingWebView.magnification = newValue
            #else
            backingWebView.scrollView.zoomScale = newValue
            #endif
        }
    }
}

extension WebPage.EditorStateSnapshot {
    init(_ dictionary: [AnyHashable: Any]) {
        // The Objective-C interface this is converting from is not able to express at compile-time that this is guaranteed.
        // swift-format-ignore: NeverForceUnwrap
        self.selectionType = SelectionType(rawValue: dictionary["selection-type"] as! SelectionType.RawValue)!

        guard let postLayoutData = dictionary["post-layout-data"] as? Bool, postLayoutData else {
            self.postLayoutData = nil
            return
        }

        // The Objective-C interface this is converting from is not able to express at compile-time that these are guaranteed.
        // swift-format-ignore: NeverForceUnwrap
        self.postLayoutData = .init(
            bold: dictionary["bold"] as! Bool,
            italic: dictionary["italic"] as! Bool,
            underline: dictionary["underline"] as! Bool,
            textAlignment: NSTextAlignment(rawValue: dictionary["text-alignment"] as! Int)!,
            textColor: dictionary["text-color"] as! String
        )
    }
}

#endif // ENABLE_SWIFTUI

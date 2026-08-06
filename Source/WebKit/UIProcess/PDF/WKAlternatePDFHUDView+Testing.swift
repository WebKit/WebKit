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

#if ENABLE_PDF_HUD

import AppKit
import WebKit_Internal
@_weakLinked import SwiftUI

extension WKAlternatePDFHUDView {
    // For testing only, via `NSSelectorFromString`.
    @objc(_performActionForControl:)
    private func performAction(controlName: String) {
        guard let hostingView = subviews.first as? NSHostingView<PDFHUDControls> else {
            fatalError()
        }

        let action = hostingView.rootView.action

        switch controlName {
        case "minus.magnifyingglass":
            action(.zoomOut)
        case "plus.magnifyingglass":
            action(.zoomIn)
        case "preview":
            action(.openInPreview)
        case "arrow.down.circle":
            action(.savePDF)
        default:
            fatalError()
        }
    }

    @objc(_hideForTesting)
    private func hideForTesting() {
        model.isAutoHidden = true
    }
}

#endif // ENABLE_PDF_HUD

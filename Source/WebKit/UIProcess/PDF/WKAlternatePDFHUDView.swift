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
public import Foundation
import WebKit_Internal
@_weakLinked @_spi(Private) import SwiftUI

private struct Controls: View {
    static let hoverMargin: CGFloat = 24
    private static let autoHideDelay: Duration = .seconds(3)

    let showSystemActions: Bool
    let action: (WKPDFHUDViewControlAction) -> Void

    @State
    private var initialHideTimerFired = false

    @State
    private var isHovered = false

    private var isVisible: Bool {
        !initialHideTimerFired || isHovered
    }

    var body: some View {
        ControlGroup {
            Button("Zoom Out", systemImage: "minus.magnifyingglass") {
                action(.zoomOut)
            }

            Button("Zoom In", systemImage: "plus.magnifyingglass") {
                action(.zoomIn)
            }

            if showSystemActions {
                Button {
                    action(.openInPreview)
                } label: {
                    Label {
                        Text("Open in Preview")
                    } icon: {
                        Image(_internalSystemName: "preview")
                    }
                }

                Button("Save PDF", systemImage: "arrow.down.circle") {
                    action(.savePDF)
                }
            }
        }
        .labelStyle(.iconOnly)
        .opacity(isVisible ? 1 : 0)
        .animation(.easeInOut, value: isVisible)
        #if USE_APPLE_INTERNAL_SDK && HAVE_NSGLASSEFFECTVIEW_EFFECT_IS_INTERACTIVE
        .controlGroupStyle(.toolbar)
        #endif
        .padding(Self.hoverMargin)
        .contentShape(.rect)
        .onHover {
            isHovered = $0
        }
        .task {
            try? await Task.sleep(for: Self.autoHideDelay)
            guard !Task.isCancelled else { return }
            initialHideTimerFired = true
        }
    }
}

@objc
@implementation
extension WKAlternatePDFHUDView {
    private static let barVerticalOffset: CGFloat = 40

    let frameIdentifier: UInt64

    init(
        frame: NSRect,
        frameIdentifier: UInt64,
        compositingBordersVisible: Bool,
        actionHandler: @MainActor @Sendable @escaping (WKPDFHUDViewControlAction) -> Void
    ) {
        self.frameIdentifier = frameIdentifier

        super.init(frame: frame)

        let controls = Controls(showSystemActions: !isInRecoveryOS(), action: actionHandler)

        let hostingView = NSHostingView(rootView: controls)
        hostingView.translatesAutoresizingMaskIntoConstraints = false

        addSubview(hostingView)
        hostingView.centerXAnchor.constraint(equalTo: centerXAnchor).isActive = true
        hostingView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -Self.barVerticalOffset + Controls.hoverMargin).isActive = true
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc
    @available(*, unavailable)
    public required dynamic init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func show() {
        // FIXME: Implement `WKAlternatePDFHUDView.show`.
    }
}

#endif // ENABLE_PDF_HUD

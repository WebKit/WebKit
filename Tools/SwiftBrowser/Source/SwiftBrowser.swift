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

import SwiftUI
@_spi(Testing) import WebKit

@main
struct SwiftBrowserApp: App {
    @FocusedValue(BrowserViewModel.self)
    var focusedBrowserViewModel

    @AppStorage(AppStorageKeys.homepage)
    private var homepage = "https://www.webkit.org"

    @State
    private var smartListsEnabled = true

    @State
    private var mostRecentURL: URL? = nil

    private static func addProtocolIfNecessary(to address: String) -> String {
        if address.contains("://") || address.hasPrefix("data:") || address.hasPrefix("about:") {
            return address
        }
        return "http://\(address)"
    }

    var body: some Scene {
        WindowGroup(for: CodableURLRequest.self) { $request in
            BrowserView(
                url: $mostRecentURL,
                smartListsEnabled: smartListsEnabled,
                initialRequest: request.value
            )
        } defaultValue: {
            // FIXME: <https://webkit.org/b/293859> BrowserView does not reflect URL argument passed to SwiftBrowser.app.
            let parsedURL = CommandLine.value(for: "--url").flatMap {
                let withProtocol = Self.addProtocolIfNecessary(to: $0)
                return URL(string: withProtocol)
            }
            let url = parsedURL ?? URL(string: homepage)!
            return CodableURLRequest(.init(url: url))
        }
        .commands {
            CommandGroup(after: .sidebar) {
                Button("Reload Page", systemImage: "arrow.clockwise") {
                    focusedBrowserViewModel!.page.reload()
                }
                .keyboardShortcut("r")
                .disabled(focusedBrowserViewModel == nil)
            }

            CommandGroup(replacing: .importExport) {
                Button("Export as PDF…", systemImage: "arrow.up.document") {
                    focusedBrowserViewModel!.exportAsPDF()
                }
                .disabled(focusedBrowserViewModel == nil)
            }

            TextEditingCommands()

            CommandGroup(after: .textEditing) {
                Toggle(isOn: $smartListsEnabled) {
                    Label("Smart Lists", systemImage: "sparkle.text.clipboard")
                }
                .disabled(focusedBrowserViewModel == nil)
            }
        }

        #if os(macOS)
        Settings {
            SettingsView(currentURL: mostRecentURL)
        }
        #endif
    }
}

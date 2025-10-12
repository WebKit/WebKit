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
import _WebKit_SwiftUI

struct BrowserView: View {
    @Binding
    var url: URL?

    let smartListsEnabled: Bool

    let initialRequest: URLRequest

    @State
    private var viewModel = BrowserViewModel()

    var body: some View {
        ContentView(url: $url, initialRequest: initialRequest)
            .environment(viewModel)
            .onChange(of: smartListsEnabled, initial: true) {
                #if os(macOS)
                viewModel.page.smartListsEnabled = smartListsEnabled
                #endif
            }
            .task {
                // Safety: this is actually safe; false positive is rdar://154775389
                for await unsafe _ in NotificationCenter.default.messages(of: UserDefaults.self, for: .didChange) {
                    viewModel.updateWebPreferences()
                }
            }
            .onAppear(perform: viewModel.updateWebPreferences)
    }
}

#Preview {
    @Previewable @State var viewModel = BrowserViewModel()

    @Previewable @State var url: URL? = nil

    let request = {
        let url = URL(string: "https://www.apple.com")!
        return URLRequest(url: url)
    }()

    BrowserView(url: $url, smartListsEnabled: true, initialRequest: request)
        .environment(viewModel)
}

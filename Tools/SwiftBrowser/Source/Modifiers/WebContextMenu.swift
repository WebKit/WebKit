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

import SwiftUI
import WebKit

#if os(macOS)
private struct WebContextMenu: ViewModifier {
    @Environment(\.openWindow)
    private var openWindow

    @Environment(BrowserViewModel.self)
    private var viewModel

    func body(content: Content) -> some View {
        content
            .webViewContextMenu { element in
                if let url = element.linkURL {
                    Button("Open Link in New Window") {
                        let request = URLRequest(url: url)
                        openWindow(value: CodableURLRequest(request))
                    }
                } else {
                    if let previousItem = viewModel.page.backForwardList.backList.last {
                        Button("Back") {
                            viewModel.page.load(previousItem)
                        }
                    }

                    if let nextItem = viewModel.page.backForwardList.forwardList.first {
                        Button("Forward") {
                            viewModel.page.load(nextItem)
                        }
                    }

                    Button("Reload") {
                        viewModel.page.reload()
                    }
                }
            }
    }
}
#endif // os(macOS)

extension View {
    func webContextMenu() -> some View {
        #if os(macOS)
        modifier(WebContextMenu())
        #else
        self
        #endif
    }
}

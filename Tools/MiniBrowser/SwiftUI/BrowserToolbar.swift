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

#if compiler(>=6.0)

import SwiftUI
@_spi(Private) import WebKit

fileprivate struct ToolbarBackForwardMenuView: View {
    struct LabelConfiguration {
        let text: String
        let systemImage: String
    }

    let list: [WebPage_v0.BackForwardList.Item]
    let label: LabelConfiguration
    let navigateToItem: (WebPage_v0.BackForwardList.Item) -> Void

    var body: some View {
        Menu {
            ForEach(list) { item in
                Button(item.title ?? item.url.absoluteString) {
                    navigateToItem(item)
                }
            }
        } label: {
            Label(label.text, systemImage: label.systemImage)
                .labelStyle(.iconOnly)
        } primaryAction: {
            navigateToItem(list.first!)
        }
        .menuIndicator(.hidden)
        .disabled(list.isEmpty)
    }
}

struct BrowserToolbar: ViewModifier {
    let viewModel: BrowserViewModel

    func body(content: Content) -> some View {
        content.toolbar {
            ToolbarItemGroup(placement: .navigation) {
                ToolbarBackForwardMenuView(
                    list: viewModel.page.backForwardList.backList.reversed(),
                    label: .init(text: "Backward", systemImage: "chevron.backward")
                ) {
                    viewModel.page.load(backForwardItem: $0)
                }

                ToolbarBackForwardMenuView(
                    list: viewModel.page.backForwardList.forwardList,
                    label: .init(text: "Forward", systemImage: "chevron.forward")
                ) {
                    viewModel.page.load(backForwardItem: $0)
                }
            }

            ToolbarItem(placement: .principal) {
                VStack(spacing: 0) {
                    @Bindable var viewModel = viewModel

                    TextField("URL", text: $viewModel.displayedURL)
                        .textContentType(.URL)
                        .onSubmit {
                            viewModel.navigateToSubmittedURL()
                        }
                        .textFieldStyle(.roundedBorder)

                    ProgressView(value: viewModel.page.estimatedProgress, total: 1.0)
                        .padding(.horizontal, 2)
                        .padding(.top, -4)
                        .padding(.bottom, -8)
                }
                .frame(minWidth: 300)
            }
        }
    }
}

#Preview {
    @Previewable @State var viewModel = BrowserViewModel()

    Color.indigo
        .modifier(BrowserToolbar(viewModel: viewModel))
}

#endif

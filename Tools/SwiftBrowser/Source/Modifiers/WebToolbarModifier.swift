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

private struct ToolbarBackForwardMenuView: View {
    struct LabelConfiguration {
        let text: String
        let systemImage: String
        let key: KeyEquivalent
    }

    let list: [WebPage.BackForwardList.Item]
    let label: LabelConfiguration
    let navigateToItem: (WebPage.BackForwardList.Item) -> Void

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
            // Safe because the menu is disabled if `list.isEmpty`
            // swift-format-ignore: NeverForceUnwrap
            navigateToItem(list.first!)
        }
        .menuIndicator(.hidden)
        .disabled(list.isEmpty)
        .keyboardShortcut(label.key)
    }
}

private struct MediaCaptureStateButtonView: View {
    struct LabelConfiguration {
        let activeSystemImage: String
        let mutedSystemImage: String
    }

    let captureState: WKMediaCaptureState
    let configuration: LabelConfiguration
    let action: (WKMediaCaptureState) -> Void

    var body: some View {
        switch captureState {
        case .none:
            EmptyView()

        case .active:
            Button {
                action(.muted)
            } label: {
                Label("Mute", systemImage: configuration.activeSystemImage)
                    .labelStyle(.iconOnly)
            }

        case .muted:
            Button {
                action(.active)
            } label: {
                Label("Unmute", systemImage: configuration.mutedSystemImage)
                    .labelStyle(.iconOnly)
            }

        @unknown default:
            fatalError()
        }
    }
}

private struct WebToolbarModifier: ViewModifier {
    #if os(iOS)
    private static let navigationToolbarItemPlacement = ToolbarItemPlacement.bottomBar
    #else
    private static let navigationToolbarItemPlacement = ToolbarItemPlacement.navigation
    #endif

    @Environment(BrowserViewModel.self)
    private var viewModel

    @Binding
    var findNavigatorIsPresented: Bool

    func body(content: Content) -> some View {
        content
            .toolbar {
                ToolbarItemGroup(placement: Self.navigationToolbarItemPlacement) {
                    ToolbarBackForwardMenuView(
                        list: viewModel.page.backForwardList.backList.reversed(),
                        label: .init(text: "Backward", systemImage: "chevron.backward", key: "[")
                    ) {
                        viewModel.page.load($0)
                    }

                    #if os(iOS)
                    Spacer()
                    #endif

                    ToolbarBackForwardMenuView(
                        list: viewModel.page.backForwardList.forwardList,
                        label: .init(text: "Forward", systemImage: "chevron.forward", key: "]")
                    ) {
                        viewModel.page.load($0)
                    }

                    Spacer()

                    Button {
                        findNavigatorIsPresented.toggle()
                    } label: {
                        Label("Find", systemImage: "magnifyingglass")
                            .labelStyle(.iconOnly)
                    }
                    .keyboardShortcut("f")
                }

                ToolbarItemGroup(placement: .principal) {
                    @Bindable
                    var viewModel = viewModel

                    TextField("URL", text: $viewModel.displayedURL)
                        .textContentType(.URL)
                        .onSubmit {
                            viewModel.navigateToSubmittedURL()
                        }
                        .textFieldStyle(.roundedBorder)
                        .padding(.leading, 4)
                        .frame(minWidth: 300)

                    if viewModel.page.isLoading {
                        Button {
                            viewModel.page.stopLoading()
                        } label: {
                            Image(systemName: "xmark")
                        }
                        .keyboardShortcut(".")
                    } else {
                        Button {
                            viewModel.page.reload()
                        } label: {
                            Image(systemName: "arrow.clockwise")
                        }
                        .keyboardShortcut("r")
                    }

                    MediaCaptureStateButtonView(
                        captureState: viewModel.page.cameraCaptureState,
                        configuration: .init(activeSystemImage: "video.fill", mutedSystemImage: "video.slash.fill"),
                        action: viewModel.setCameraCaptureState(_:)
                    )

                    MediaCaptureStateButtonView(
                        captureState: viewModel.page.microphoneCaptureState,
                        configuration: .init(activeSystemImage: "microphone.fill", mutedSystemImage: "microphone.slash.fill"),
                        action: viewModel.setMicrophoneCaptureState(_:)
                    )
                }

                ToolbarItemGroup {
                    if viewModel.page.isLoading {
                        ProgressView(value: viewModel.page.estimatedProgress, total: 1.0)
                            .progressViewStyle(.circular)
                            .controlSize(.small)
                    }
                }
            }
    }
}

extension View {
    func webToolbar(findNavigatorIsPresented: Binding<Bool>) -> some View {
        modifier(WebToolbarModifier(findNavigatorIsPresented: findNavigatorIsPresented))
    }
}

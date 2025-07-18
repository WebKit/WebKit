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
import UniformTypeIdentifiers
import WebKit
import _WebKit_SwiftUI

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

private struct PrincipalToolbarGroup: View {
    @Environment(BrowserViewModel.self) private var viewModel

    var body: some View {
        HStack {
            @Bindable var viewModel = viewModel

            VStack(spacing: 0) {
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
    }
}

private struct DialogActionsView: View {
    private let dialog: DialogPresenter.Dialog

    @State private var promptText = ""

    init(dialog: DialogPresenter.Dialog) {
        self.dialog = dialog

        if case let .prompt(_, defaultText, _) = dialog.configuration, let defaultText {
            _promptText = State(initialValue: defaultText)
        }
    }

    var body: some View {
        switch dialog.configuration {
        case let .alert(_, dismissAlert):
            Button("Close", action: dismissAlert)

        case let .confirm(_, dismissConfirm):
            Button("OK") {
                dismissConfirm(.ok)
            }

            Button("Cancel", role: .cancel) {
                dismissConfirm(.cancel)
            }

        case let .prompt(_, _, dismissPrompt):
            TextField("Text", text: $promptText)

            Button("OK") {
                dismissPrompt(.ok(promptText))
            }
            .keyboardShortcut(.defaultAction)

            Button("Cancel", role: .cancel) {
                dismissPrompt(.cancel)
            }
        }
    }
}

private struct DialogMessageView: View {
    let dialog: DialogPresenter.Dialog

    var body: some View {
        switch dialog.configuration {
        case let .alert(message, _):
            Text(message)

        case let .confirm(message, _):
            Text(message)

        case let .prompt(message, _, _):
            Text(message)
        }
    }
}

struct ContentView: View {
    @Binding var url: URL?

    let initialRequest: URLRequest

    @State private var findNavigatorIsPresented = false

    @Environment(\.openWindow) private var openWindow
    @Environment(BrowserViewModel.self) private var viewModel

    @AppStorage(AppStorageKeys.scrollBounceBehaviorBasedOnSize) private var scrollBounceBehaviorBasedOnSize: Bool?
    @AppStorage(AppStorageKeys.backgroundHidden) private var backgroundHidden: Bool?

    #if os(iOS)
    private static let navigationToolbarItemPlacement = ToolbarItemPlacement.bottomBar
    #else
    private static let navigationToolbarItemPlacement = ToolbarItemPlacement.navigation
    #endif

    var body: some View {
        NavigationStack {
            @Bindable var viewModel = viewModel

            WebView(viewModel.page)
                .webViewBackForwardNavigationGestures(.enabled)
                .webViewLinkPreviews(.enabled)
                .webViewTextSelection(.enabled)
                .webViewElementFullscreenBehavior(.enabled)
                .findNavigator(isPresented: $findNavigatorIsPresented)
                .task {
                    do {
                        for try await event in viewModel.page.navigations {
                            print(event)
                        }
                    } catch {
                        print(error)
                    }
                }
                .onAppear {
                    viewModel.displayedURL = initialRequest.url!.absoluteString
                    viewModel.navigateToSubmittedURL()
                }
                .onChange(of: viewModel.page.url) { _, newValue in
                    url = newValue
                }
                .onChange(of: viewModel.currentOpenRequest) { _, newValue in
                    guard let newValue else {
                        return
                    }

                    openWindow(value: CodableURLRequest(newValue.request))
                    viewModel.currentOpenRequest = nil
                }
                .navigationTitle(viewModel.page.title)
                #if os(iOS)
                .navigationBarTitleDisplayMode(.inline)
                #endif
                .focusedSceneValue(viewModel)
                .onOpenURL(perform: viewModel.openURL(_:))
                .fileExporter(isPresented: $viewModel.pdfExporterIsPresented, item: viewModel.exportedPDF, defaultFilename: viewModel.exportedPDF?.title, onCompletion: viewModel.didExportPDF(result:))
                .fileImporter(isPresented: $viewModel.isPresentingFilePicker, allowedContentTypes: [.png, .pdf], allowsMultipleSelection: viewModel.currentFilePicker?.allowsMultipleSelection ?? false, onCompletion: viewModel.didImportFiles(result:))
                .alert("\(url?.absoluteString ?? "") says:", isPresented: $viewModel.isPresentingDialog, presenting: viewModel.currentDialog) { dialog in
                    DialogActionsView(dialog: dialog)
                } message: { dialog in
                    DialogMessageView(dialog: dialog)
                }
                .scrollBounceBehavior(scrollBounceBehaviorBasedOnSize == true ? .basedOnSize : .automatic)
                .webViewContentBackground(backgroundHidden == true ? .hidden : .automatic)
#if os(macOS)
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
#endif
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
                        PrincipalToolbarGroup()
                    }
                }
        }
    }
}

#Preview {
    @Previewable @State var viewModel = BrowserViewModel()

    @Previewable @State var url: URL? = nil

    let request = {
        let url = URL(string: "https://www.apple.com")!
        return URLRequest(url: url)
    }()

    ContentView(url: $url, initialRequest: request)
        .environment(viewModel)
}

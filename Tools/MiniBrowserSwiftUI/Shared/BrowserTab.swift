/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import SwiftUI
import _WebKit_SwiftUI

struct BrowserTab : View {
    @StateObject private var state = WebViewState(initialURL: URL(string: "https://webkit.org/")!)

    var body: some View {
        let content = WebView(state: state)
            .webViewNavigationPolicy(onAction: decidePolicy(for:state:))
            .alert(item: $externalNavigation, content: makeExternalNavigationAlert(_:))

#if os(macOS)
        return content
            .edgesIgnoringSafeArea(.all)
            .navigationTitle(state.title.isEmpty ? "MiniBrowserSwiftUI" : state.title)
            .toolbar {
                ToolbarItemGroup(placement: .navigation) {
                    backItem
                    forwardItem
                }
                ToolbarItem(placement: .principal) {
                    urlField
                }
            }
#else
        return content
            // FIXME: This should be `.all`, but on iOS safe area insets do not seem to be respected
            // correctly when embedded in a `NavigationView`.
            .edgesIgnoringSafeArea(.bottom)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItemGroup(placement: .navigationBarLeading) {
                    backItem.labelStyle(IconOnlyLabelStyle())
                    forwardItem.labelStyle(IconOnlyLabelStyle())
                }
                ToolbarItem(placement: .principal) {
                    urlField
                }
            }
#endif
    }

    private var urlField: some View {
        URLField(
            url: state.url,
            isSecure: state.hasOnlySecureContent,
            loadingProgress: state.estimatedProgress,
            onNavigate: onNavigate(to:)
        ) {
            if state.isLoading {
                Button(action: state.stopLoading) {
                    Label("Stop Loading", systemImage: "xmark")
                }
            } else {
                Button(action: state.reload) {
                    Label("Reload", systemImage: "arrow.clockwise")
                }
                .disabled(state.url == nil)
            }
        }
    }

    private var backItem: some View {
        Button(action: state.goBack) {
            Label("Back", systemImage: "chevron.left")
                .frame(minWidth: 20)
        }
        .disabled(!state.canGoBack)
    }

    private var forwardItem: some View {
        Button(action: state.goForward) {
            Label("Forward", systemImage: "chevron.right")
                .frame(minWidth: 20)
        }
        .disabled(!state.canGoForward)
    }

    private func onNavigate(to string: String) {
        switch UserInput(string: string) {
        case .search(let term):
            state.load(URL(searchTerm: term))
        case .url(let url):
            state.load(url)
        case .invalid:
            break
        }
    }

    @State private var externalNavigation: ExternalURLNavigation?
    @Environment(\.openURL) private var openURL

    private func decidePolicy(for action: NavigationAction, state: WebViewState) {
        if let externalURL = action.request.url, !WebView.canHandle(externalURL) {
            externalNavigation = ExternalURLNavigation(
                source: state.url ?? .aboutBlank, destination: externalURL)
            action.decidePolicy(.cancel)
        } else {
            action.decidePolicy(.allow)
        }
    }

    private func makeExternalNavigationAlert(_ navigation: ExternalURLNavigation) -> Alert {
        Alert(title: Text("Allow “\(navigation.source.highLevelDomain)” to open “\(navigation.destination.scheme ?? "")”?"),
              primaryButton: .default(Text("Allow"), action: { openURL(navigation.destination) }),
              secondaryButton: .cancel())
    }

}

private enum UserInput {
    case search(String)
    case url(URL)
    case invalid

    init(string _string: String) {
        let string = _string.trimmingCharacters(in: .whitespaces)

        if string.isEmpty {
            self = .invalid
        } else if !string.contains(where: \.isWhitespace), string.contains("."), let url = URL(string: string) {
            self = .url(url)
        } else {
            self = .search(string)
        }
    }
}

private struct ExternalURLNavigation : Identifiable, Hashable {
    var source: URL
    var destination: URL

    var id: Self { self }
}

struct BrowserTab_Previews: PreviewProvider {
    static var previews: some View {
#if os(macOS)
        BrowserTab()
#else
        NavigationView {
            BrowserTab()
        }
        .navigationViewStyle(StackNavigationViewStyle())
#endif
    }
}

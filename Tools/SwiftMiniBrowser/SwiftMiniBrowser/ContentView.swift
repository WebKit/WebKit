/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
import WebKit

struct ContentView: View {

    var configuration: WKWebViewConfiguration = WKWebViewConfiguration()
    @State var currentModel = WebViewModel()

    init() {
        currentModel.url = URL(string:"https://webkit.org/")
        configuration.allowsInlineMediaPlayback = true
    }

    var body: some View {

        VStack(spacing: 0) {
            WebView(configuration: configuration, model: $currentModel)
                .refreshable {
                    self.currentModel.reload()
                }
            HStack {
                Button(action: currentModel.goBack) {
                    Image(systemName:"arrowshape.backward.fill")
                }
                    .disabled(!currentModel.canGoBack)
                Button(action: currentModel.goForward) {
                    Image(systemName: "arrowshape.forward.fill")
                }
                    .disabled(!currentModel.canGoForward)
                VStack(spacing: 0) {
                    TextField("URL", text: $currentModel.urlString)
                        .keyboardType(.URL)
                        .textContentType(.URL)
                        .autocapitalization(.none)
                        .disableAutocorrection(true)
                        .padding(4)
                        .onSubmit {
                            self.currentModel.loadURL()
                        }
                    ProgressView(value: currentModel.estimatedProgress)
                        .opacity(currentModel.isLoading ? 1 : 0)
                }
                    .border(.secondary, width:2)
                    .background(Color.white)
                Button(action: currentModel.reload) {
                    Image(systemName: "arrow.clockwise")
                }
            }
            .padding()
            .background(.tertiary)
        }
    }
}

#Preview {
    ContentView()
}

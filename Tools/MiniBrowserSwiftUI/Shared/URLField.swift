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

struct URLField<Accessory> : View where Accessory : View {
    init(
        url: URL?,
        isSecure: Bool = false,
        loadingProgress: Double? = nil,
        onEditingChange: @escaping (Bool) -> Void = { _ in },
        onNavigate: @escaping (String) -> Void = { _ in },
        @ViewBuilder accessory: () -> Accessory
    ) {
        self.trailingAccessory = accessory()
        self.externalOnEditingChange = onEditingChange
        self.loadingProgress = loadingProgress
        self.onNavigate = onNavigate
        self.url = url
        self.urlIsSecure = isSecure
        self._text = State(wrappedValue: url?.absoluteString ?? "")
    }

    private var externalOnEditingChange: (Bool) -> Void
    @State private var isEditing = false
    private var loadingProgress: Double?
    private var onNavigate: (String) -> Void
    @State private var text: String = ""
    private var trailingAccessory: Accessory
    private var url: URL?
    private var urlIsSecure: Bool

    var body: some View {
        let content = HStack {
            leadingAccessory

            textField

            if !isEditing {
                trailingAccessory
                    .labelStyle(IconOnlyLabelStyle())
            }
        }
        .buttonStyle(PlainButtonStyle())
        .font(.body)
        .padding(.vertical, 6)
        .padding(.horizontal, 10)
        .background(Background(loadingProgress: loadingProgress))
        .frame(minWidth: 200, idealWidth: 400, maxWidth: 600)
        .onChange(of: url, perform: { _ in urlDidChange() })

#if os(macOS)
        return content
            .overlay(RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.accentColor))
#else
        return content
#endif
    }

    @ViewBuilder
    private var leadingAccessory: some View {
        if isEditing {
            Image(systemName: "globe").foregroundColor(.secondary)
        } else if url?.isWebSearch == true {
            Image(systemName: "magnifyingglass").foregroundColor(.orange)
        } else if urlIsSecure {
            Image(systemName: "lock.fill").foregroundColor(.green)
        } else {
            Image(systemName: "globe").foregroundColor(.secondary)
        }
    }

    private var textField: some View {
        let view = TextField(
            "Search or website address",
            text: $text,
            onEditingChanged: onEditingChange(_:),
            onCommit: onCommit
        )
        .textFieldStyle(PlainTextFieldStyle())
        .disableAutocorrection(true)

#if os(macOS)
        return view
#else
        return view
            .textContentType(.URL)
            .autocapitalization(.none)
#endif
    }

    private func urlDidChange() {
        if !isEditing {
            text = url?.absoluteString ?? ""
        }
    }

    private func onEditingChange(_ isEditing: Bool) {
        self.isEditing = isEditing
        if !isEditing {
            urlDidChange()
        }

        externalOnEditingChange(isEditing)
    }

    private func onCommit() {
        onNavigate(text)
    }
}

private struct Background : View {
    var loadingProgress: Double?

    var body: some View {
        RoundedRectangle(cornerRadius: 6)
            .foregroundColor(Color("URLFieldBackground"))
            .overlay(progress)
    }

    private var progress: some View {
        GeometryReader { proxy in
            if let loadingProgress = loadingProgress {
                ProgressView(value: loadingProgress)
                    .offset(y: proxy.size.height - 4)
            }
        }
    }
}

struct URLField_Previews: PreviewProvider {
    static var previews: some View {
        Group {
            URLField(
                url: nil,
                isSecure: false,
                loadingProgress: nil,
                onEditingChange: { _ in },
                onNavigate: { _ in }
            ) {
                EmptyView()
            }

            URLField(
                url: URL(string: "https://webkit.org")!,
                isSecure: true,
                loadingProgress: nil,
                onEditingChange: { _ in },
                onNavigate: { _ in }
            ) {
                Image(systemName: "arrow.clockwise")
            }

            URLField(
                url: URL(string: "https://webkit.org")!,
                isSecure: true,
                loadingProgress: nil,
                onEditingChange: { _ in },
                onNavigate: { _ in }
            ) {
                Image(systemName: "arrow.clockwise")
            }
            .environment(\.colorScheme, .dark)
        }
        .padding()
        .frame(maxWidth: 400)
        .previewLayout(.sizeThatFits)
    }
}

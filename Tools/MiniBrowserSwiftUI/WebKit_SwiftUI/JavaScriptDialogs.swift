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

/// A view providing the WebKit-default UI for a JavaScript alert.
struct JavaScriptAlert : View {
    private var message: String
    private var completion: () -> Void

    init(message: String, completion: @escaping () -> Void) {
        self.message = message
        self.completion = completion
    }

    var body: some View {
        DialogContainer {
            Text(message)
        } actions: {
            Button("OK", action: completion)
                .keyboardShortcut(.return)
        }
    }
}

/// A view providing the WebKit-default UI for a JavaScript alert.
struct JavaScriptConfirm : View {
    private var message: String
    private var completion: (Bool) -> Void

    init(message: String, completion: @escaping (Bool) -> Void) {
        self.message = message
        self.completion = completion
    }

    var body: some View {
        DialogContainer {
            Text(message)
        } actions: {
            Button("Cancel", action: { completion(false) })
                .keyboardShortcut(".")
            Button("OK", action: { completion(true) })
                .keyboardShortcut(.return)
        }
    }
}

/// A view providing the WebKit-default UI for a JavaScript alert.
struct JavaScriptPrompt : View {
    private var message: String
    private var completion: (String?) -> Void
    @State private var text: String

    init(message: String, defaultText: String = "", completion: @escaping (String?) -> Void) {
        self.message = message
        self._text = State(wrappedValue: defaultText)
        self.completion = completion
    }

    var body: some View {
        DialogContainer {
            Text(message)
            TextField("Your Response", text: $text, onCommit: { completion(text) })
        } actions: {
            Button("Cancel", action: { completion(nil) })
                .keyboardShortcut(".")
            Button("OK", action: { completion(text) })
                .keyboardShortcut(.return)
        }
    }
}

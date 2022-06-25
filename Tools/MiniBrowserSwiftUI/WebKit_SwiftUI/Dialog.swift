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

/// A dialog requested for display by some aspect of web content.
public struct Dialog : Identifiable, Hashable {
    /// An opaque identifier for this dialog.
    public var id = ID()

    /// A configuration describing the purpose and contents of this dialog for display.
    public var configuration: Configuration

    /// Constructs a dialog from its component parts.
    public init(id: ID = ID(), _ configuration: Configuration) {
        self.id = id
        self.configuration = configuration
    }

    /// Constructs a dialog for a JavaScript `alert()`, which contains only a message to the user.
    /// - Parameters:
    ///   - id: An opaque identifier for the dialog.
    ///   - message: A message to display to the user.
    ///   - completion: A block to be invoked when the dialog has been dismissed. The webpage will
    ///   be paused until this block is invoked, and failure to invoke the block, or multiple
    ///   invocations will result in a runtime exception.
    public static func javaScriptAlert(
        id: ID = ID(), _ message: String, completion: @escaping () -> Void
    ) -> Self {
        Dialog(id: id, .javaScriptAlert(message, completion))
    }

    /// Constructs a dialog for a JavaScript `confirm()`, which contains a message to the user and
    /// expects a binary response for the webpage.
    /// - Parameters:
    ///   - id: An opaque identifier for the dialog.
    ///   - message: A message to display to the user.
    ///   - completion: A block to be invoked with the user's decision. The webpage will be paused
    ///   until this block is invoked, and failure to invoke the block, or multiple invocations will
    ///   result in a runtime exception.
    public static func javaScriptConfirm(
        id: ID = ID(), _ message: String, completion: @escaping (Bool) -> Void
    ) -> Self {
        Dialog(id: id, .javaScriptConfirm(message, completion))
    }

    /// Constructs a dialog for a JavaScript `prompt()`, which contains a message to the user and
    /// expects the response of either a string or `nil`.
    /// - Parameters:
    ///   - id: An opaque identifier for the dialog.
    ///   - message: A message to display to the user.
    ///   - defaultText: A default response for the user, provided by the webpage.
    ///   - completion: A block to be invoked with the user's decision. For this reply, `nil` and
    ///   an empty string (`""`) are treated differently. `nil` means that the user selected a
    ///   _cancel_ operation, while any non-`nil` response implies an _ok_ operation. The webpage
    ///   will be paused until this block is invoked, and failure to invoke the block, or multiple
    ///   invocations will result in a runtime exception.
    public static func javaScriptPrompt(
        id: ID = ID(), _ message: String, defaultText: String = "",
        completion: @escaping (String?) -> Void
    ) -> Self {
        Dialog(id: id, .javaScriptPrompt(message, defaultText: defaultText, completion))
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }

    public static func == (lhs: Dialog, rhs: Dialog) -> Bool {
        lhs.id == rhs.id
    }

    /// An opaque identifier for a `Dialog`.
    public struct ID : Hashable {
        private var rawValue = UUID()

        /// Constructs a new, unique identifier.
        public init() {
        }
    }

    /// Describes a specific type of dialog, along with any specific values for that type.
    public enum Configuration {
        /// A dialog triggered by the JavaScript `window.alert()` API.
        /// - Paramaters:
        ///   - 0: A message from the webpage for the user.
        ///   - 1: A block to be invoked when the dialog has been dismissed. The webpage will be
        ///   paused until this block is invoked, and failure to invoke the block, or multiple
        ///   invocations will result in a runtime exception.
        case javaScriptAlert(String, () -> Void)

        /// A dialog triggered by the JavaScript `window.confirm()` API.
        ///   - 0: A message from the webpage for the user.
        ///   - 1: A block to be invoked with a binary decision from the user. The webpage will be
        ///   paused until this block is invoked, and failure to invoke the block, or multiple
        ///   invocations will result in a runtime exception.
        case javaScriptConfirm(String, (Bool) -> Void)

        /// A dialog triggered by the JavaScript `window.prompt()` API.
        /// - Paramaters:
        ///   - 0: A message from the webpage for the user.
        ///   - defaultText: A webpage provided default response for the user.
        ///   - 1: A block to be invoked with the user's decision. For this reply, `nil` and an
        ///   empty string (`""`) are treated differently. `nil` means that the user selected a
        ///   _cancel_ operation, while any non-`nil` response implies an _ok_ operation. The
        ///   webpage will be paused until this block is invoked, and failure to invoke the block,
        ///   or multiple invocations will result in a runtime exception.
        case javaScriptPrompt(String, defaultText: String, (String?) -> Void)
    }
}

/// A view providing the WebKit-default UI for generic webpage dialogs.
struct DialogContainer<Contents, Actions> : View where Contents : View, Actions : View {
    var contents: Contents
    var actions: Actions

    init(@ViewBuilder contents: () -> Contents, @ViewBuilder actions: () -> Actions) {
        self.contents = contents()
        self.actions = actions()
    }

    var body: some View {
        ZStack {
            Color(white: 0, opacity: 0.15)

            VStack(spacing: 0) {
                VStack(alignment: .leading) {
                    contents
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .padding()

                Divider()

                HStack(spacing: 12) {
                    Spacer()
                    actions
                        .buttonStyle(_LinkButtonStyle())
                }
                .padding(.horizontal)
                .padding(.vertical, 12)
            }
            .frame(maxWidth: 300)
            .background(RoundedRectangle(cornerRadius: 10)
                            .foregroundColor(Color.platformBackground)
                            .shadow(radius: 12))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.platformSeparator))
        }
    }
}

#if os(macOS)
private typealias _LinkButtonStyle = LinkButtonStyle
#else
private struct _LinkButtonStyle : ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .foregroundColor(.blue)
    }
}
#endif

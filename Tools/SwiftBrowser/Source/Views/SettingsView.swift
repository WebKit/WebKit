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
import WebKit

private struct PermissionDecisionView: View {
    @Binding var permissionDecision: WKPermissionDecision

    let label: String

    var body: some View {
        Picker(selection: $permissionDecision) {
            Text("Ask").tag(WKPermissionDecision.prompt)
            Text("Grant").tag(WKPermissionDecision.grant)
            Text("Deny").tag(WKPermissionDecision.deny)
        } label: {
            Text(label)
        }
    }
}

struct GeneralSettingsView: View {
    @AppStorage(AppStorageKeys.homepage) private var homepage = "https://www.webkit.org"

    @AppStorage(AppStorageKeys.orientationAndMotionAuthorization) private var orientationAndMotionAuthorization = WKPermissionDecision.prompt
    @AppStorage(AppStorageKeys.mediaCaptureAuthorization) private var mediaCaptureAuthorization = WKPermissionDecision.prompt

    let currentURL: URL?

    var body: some View {
        Form {
            Section {
                TextField("Homepage:", text: $homepage)

                Button("Set to Current Page") {
                    if let currentURL {
                        homepage = currentURL.absoluteString
                    } else {
                        fatalError()
                    }
                }
            }

            Section {
                PermissionDecisionView(
                    permissionDecision: $mediaCaptureAuthorization,
                    label: "Allow sites to access camera:"
                )
                .padding(.top)

                PermissionDecisionView(
                    permissionDecision: $orientationAndMotionAuthorization,
                    label: "Allow sites to access sensors:"
                )
            }
        }
    }
}

struct SettingsView: View {
    let currentURL: URL?

    var body: some View {
        TabView {
            Tab("General", systemImage: "gear") {
                GeneralSettingsView(currentURL: currentURL)
            }
        }
        .scenePadding()
        .frame(maxWidth: 600, minHeight: 100)
    }
}

#Preview {
    SettingsView(currentURL: URL(string: "https://www.apple.com"))
}

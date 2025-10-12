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
@_spi(_) import WebKit
import WebKit_Private._WKFeature

private struct PermissionDecisionView: View {
    @Binding
    var permissionDecision: WKPermissionDecision

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

private struct BinaryValuePicker: View {
    @Binding
    var value: Bool

    let description: String

    let falseLabel: String
    let trueLabel: String

    var body: some View {
        Picker(selection: $value) {
            Text(falseLabel).tag(false)
            Text(trueLabel).tag(true)
        } label: {
            Text(description)
        }
    }
}

private struct GeneralSettingsView: View {
    @AppStorage(AppStorageKeys.homepage)
    private var homepage = "https://www.webkit.org"

    @AppStorage(AppStorageKeys.orientationAndMotionAuthorization)
    private var orientationAndMotionAuthorization = WKPermissionDecision.prompt

    @AppStorage(AppStorageKeys.mediaCaptureAuthorization)
    private var mediaCaptureAuthorization = WKPermissionDecision.prompt

    @AppStorage(AppStorageKeys.scrollBounceBehaviorBasedOnSize)
    private var scrollBounceBehaviorBasedOnSize = false

    @AppStorage(AppStorageKeys.backgroundHidden)
    private var backgroundHidden = false

    @AppStorage(AppStorageKeys.showColorInTabBar)
    private var showColorInTabBar = true

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

            Section {
                BinaryValuePicker(
                    value: $scrollBounceBehaviorBasedOnSize,
                    description: "Scroll Bounce Behavior",
                    falseLabel: "Automatic",
                    trueLabel: "Based on Size"
                )

                BinaryValuePicker(
                    value: $backgroundHidden,
                    description: "Hidden Background Behavior",
                    falseLabel: "Automatic",
                    trueLabel: "Always Hide"
                )
            }

            Section {
                Toggle("Show color in tab bar", isOn: $showColorInTabBar)
                    .padding(.top)
            }
        }
    }
}

private struct FeatureFlagToggle: View {
    @Binding
    var value: Bool

    let feature: _WKFeature

    var body: some View {
        Toggle(isOn: $value) {
            VStack(alignment: .leading) {
                Text(feature.name)
                    .bold(value != feature.defaultValue)

                Text(feature.status.description)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .toggleStyle(.switch)
        .controlSize(.small)
    }
}

private struct FeatureFlagsView: View {
    @Environment(FeatureFlagsModel.self)
    var model

    private var groupedFeatures: FeatureFlagsModel.GroupedFeatures {
        model.groups(filteredBy: model.searchQuery)
    }

    @ViewBuilder
    private var featureList: some View {
        @Bindable
        var model = model

        List {
            ForEach(groupedFeatures, id: \.category.rawValue) { group in
                Section(group.category.description) {
                    ForEach(group.features) { feature in
                        FeatureFlagToggle(
                            value: $model.customizedFeatures[feature.key, default: feature.defaultValue],
                            feature: feature
                        )
                    }
                }
            }
        }
        .listStyle(.inset)
        .searchable(text: $model.searchQuery, placement: .sidebar, prompt: "Search")
    }

    var body: some View {
        Group {
            featureList

            HStack {
                Spacer()
                Button("Reset Feature Flags") {
                    model.customizedFeatures.removeAll()
                }
            }
        }
        .onChange(of: model.customizedFeatures, model.update)
    }
}

struct SettingsView: View {
    let currentURL: URL?

    var body: some View {
        TabView {
            Tab("General", systemImage: "gear") {
                GeneralSettingsView(currentURL: currentURL)
            }

            Tab("Feature Flags", systemImage: "flag.filled.and.flag.crossed") {
                FeatureFlagsView()
                    .environment(FeatureFlagsModel())
            }
        }
        .scenePadding()
    }
}

#Preview {
    SettingsView(currentURL: URL(string: "https://www.apple.com"))
}

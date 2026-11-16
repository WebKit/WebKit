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

import Observation
import WebKit_Private.WKPreferencesPrivate

@Observable
@MainActor
final class FeatureFlagsModel {
    typealias GroupedFeatures = [(category: WebFeatureCategory, features: [_WKFeature])]

    var searchQuery = ""

    var customizedFeatures = FeatureFlagsStorage.shared.resolve(keys: WKPreferences._features())

    private let groups: GroupedFeatures = Dictionary(grouping: WKPreferences._features(), by: \.category)
        .map { (category: $0, features: $1) }
        .filter { $0.category != .none }
        .sorted { $0.category < $1.category }

    func groups(filteredBy query: String) -> GroupedFeatures {
        groups.compactMap { group in
            guard !query.isEmpty else {
                return group
            }

            let filteredFeatures = group.features.filter {
                $0.name.localizedCaseInsensitiveContains(query)
            }

            guard !filteredFeatures.isEmpty else {
                return nil
            }

            return (
                category: group.category,
                features: filteredFeatures
            )
        }
    }

    func update(oldValue: _WKFeature.Collection, newValue: _WKFeature.Collection) {
        FeatureFlagsStorage<_WKFeature>.shared.update(mergingOldEntries: oldValue, with: newValue)
    }
}

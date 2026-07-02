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

import Foundation

protocol FeatureFlag {
    associatedtype Value: Codable & Equatable

    typealias Key = String

    typealias Collection = [Key: Value]

    // This is needed because `_WKFeature` lacks nullability annotations.
    // swift-format-ignore: NeverUseImplicitlyUnwrappedOptionals
    var key: Key! { get }

    var defaultValue: Value { get }
}

@MainActor
struct FeatureFlagsStorage<F> where F: FeatureFlag {
    static var shared: Self { Self() }

    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    func resolve(keys: some Sequence<F>) -> F.Collection {
        keys.reduce(into: [:]) { result, feature in
            guard let value = defaults.object(forKey: feature.key) as? F.Value else {
                return
            }

            result[feature.key] = value
        }
    }

    func update(mergingOldEntries oldEntries: F.Collection, with newEntries: F.Collection) {
        let oldKeys = Set(oldEntries.keys)
        let newKeys = Set(newEntries.keys)

        let commonKeys = oldKeys.intersection(newKeys)

        for key in oldKeys.subtracting(newKeys) {
            precondition(!commonKeys.contains(key))
            defaults.removeObject(forKey: key)
        }

        for key in newKeys.subtracting(oldKeys) {
            precondition(!commonKeys.contains(key))
            defaults.set(newEntries[key], forKey: key)
        }

        for key in commonKeys where oldEntries[key] != newEntries[key] {
            defaults.set(newEntries[key], forKey: key)
        }
    }
}

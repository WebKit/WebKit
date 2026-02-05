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

#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS && compiler(>=6.2)

import os
@_weakLinked internal import RealityKit
internal import WebKit_Internal

extension WKSeparatedImageView {
    func startImage3DGeneration() -> Task<Void, Error> {
        #if canImport(RealityFoundation, _version: 387)
        if let imageHash = self.imageHash, let cachedData = ImagePresentationCache.shared[imageHash] {
            Logger.separatedImage.log("\(self.logPrefix) - Cache Hit for Image Generation.")
            self.spatial3DImage = cachedData.spatial3DImage
            self.desiredViewingModeSpatial = cachedData.desiredViewingModeSpatial
            return Task { [weak self] () -> Void in
                self?.preparePortalEntity()
            }
        }

        return Task { [weak self] () -> Void in
            guard let self else { return }
            return try await self.generate()
        }
        #else
        return Task {}
        #endif
    }

    @concurrent
    func generate() async throws {
        try await Task.sleep(for: SeparatedImageViewConstants.cancellationDelay)

        #if canImport(RealityFoundation, _version: 387)
        let next = await GenerationScheduler.shared.getTurn()
        defer {
            next()
        }

        try Task.checkCancellation()

        guard let imageData = await self.imageData, let imgSource = CGImageSourceCreateWithData(imageData as CFData, nil),
            let spatial3DImage = try? await ImagePresentationComponent.Spatial3DImage(imageSource: imgSource)
        else { return }

        try await Task.sleep(for: SeparatedImageViewConstants.cancellationDelay)

        try await Task { @MainActor [weak self] in
            // The compiler can't guarantee (yet) this closure won't be called multiple times.
            nonisolated(unsafe) let captured = spatial3DImage
            guard let self, let imageHash = self.imageHash else { return }

            self.spatial3DImage = spatial3DImage
            self.preparePortalEntity()

            let start = Date()
            try await captured.generate()
            Logger.separatedImage.log("\(self.logPrefix) - Generation took \(Date().timeIntervalSince(start))")
            ImagePresentationCache.shared[imageHash] =
                ImagePresentationCache.StoredData(spatial3DImage: spatial3DImage, desiredViewingModeSpatial: self.desiredViewingModeSpatial)
        }
        .value
        #endif
    }
}

// Protects access to the tail, used from Tasks.
private actor GenerationScheduler {
    private var tail: Task<Void, Never> = Task {}

    static let shared = GenerationScheduler()
    private init() {}

    func getTurn() async -> @Sendable () -> Void {
        let previous = tail

        let (promise, fulfill) = makeVoidPromise()

        tail = Task {
            await AnalysisActor.shared.isIdle()
            try? await Task.sleep(for: SeparatedImageViewConstants.cancellationDelay)
            await previous.value
            await promise.value
        }

        await previous.value

        return fulfill
    }
}

private func makeVoidPromise() -> (promise: Task<Void, Never>, fulfill: @Sendable () -> Void) {
    let (stream, continuation) = AsyncStream<Void>.makeStream()

    let promise = Task {
        for await _ in stream {}
    }

    return (promise, fulfill: { continuation.finish() })
}

#if canImport(RealityFoundation, _version: 387)
@MainActor
final class ImagePresentationCache {
    class StoredData {
        let spatial3DImage: ImagePresentationComponent.Spatial3DImage
        var desiredViewingModeSpatial: Bool

        init(spatial3DImage: ImagePresentationComponent.Spatial3DImage, desiredViewingModeSpatial: Bool) {
            self.spatial3DImage = spatial3DImage
            self.desiredViewingModeSpatial = desiredViewingModeSpatial
        }
    }

    let cache: NSCache<NSString, StoredData>

    static let shared = ImagePresentationCache()

    private init() {
        let cache = NSCache<NSString, StoredData>()
        cache.countLimit = 15
        self.cache = cache
    }

    subscript(hash: NSString) -> StoredData? {
        get {
            cache.object(forKey: hash)
        }
        set {
            guard let newValue else {
                cache.removeObject(forKey: hash)
                return
            }
            cache.setObject(newValue, forKey: hash)
        }
    }
}
#endif

#endif

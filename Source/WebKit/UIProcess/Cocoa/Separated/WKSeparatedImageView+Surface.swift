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

internal import CryptoKit
internal import CoreGraphics
internal import CoreImage
import os
internal import UniformTypeIdentifiers
internal import WebKit_Internal

extension WKSeparatedImageView {
    func processSurface(_ surface: sending IOSurfaceRef) async {
        guard let image = await SurfaceActor.shared.makeCGImage(from: surface) else {
            Logger.separatedImage.error("Could not get CGImage from surface.")
            return
        }
        cgImage = image
        didReceiveImage()
        scheduleUpdate()
    }

    func didReceiveImage() {
        computeHashTask?.cancel()

        computeHashTask = Task { [weak self] in
            try await Task.sleep(for: SeparatedImageViewConstants.cancellationDelay)

            guard let self, let cgImage, let newImageHash = await computeHash(cgImage) else { return }
            guard self.cgImage != nil, imageHash != newImageHash else { return }

            imageData = nil
            imageHash = newImageHash

            if let (oldImageHash, cachedMode) = cachedViewModeInfo, oldImageHash == newImageHash {
                viewMode = cachedMode
            } else {
                viewMode = .unknown
            }

            if viewMode == .unknown {
                Logger.separatedImage.log("\(self.logPrefix) - New image, generated hash.")
                Task {
                    await self.pickViewMode()
                }
            } else {
                Logger.separatedImage.log("\(self.logPrefix) - Known image (\(self.viewMode.description)).")
            }

            self.scheduleUpdate()
        }
    }

    func ensureImageData() async -> Data? {
        guard let cgImage else { return nil }
        if let imageData { return imageData }
        imageData = await encode(cgImage)
        return imageData
    }

    @concurrent
    func computeHash(_ cgImage: CGImage) async -> NSString? {
        guard let provider = cgImage.dataProvider, let cfData = provider.data else { return nil }
        let digest = SHA256.hash(data: cfData as Data)
        return NSString(string: digest.description)
    }

    @concurrent
    func encode(_ cgImage: CGImage) async -> Data? {
        let data = NSMutableData()
        guard
            let destination = CGImageDestinationCreateWithData(
                data as CFMutableData,
                UTType.bmp.identifier as CFString,
                1,
                nil
            )
        else { return nil }
        CGImageDestinationAddImage(destination, cgImage, nil)
        guard CGImageDestinationFinalize(destination) else { return nil }
        return data as Data
    }
}

// Protects access to the shared CIContext, needs to run outside of the MainActor.
actor SurfaceActor {
    typealias Res = CGImage?
    private let context = CIContext(options: nil)

    static let shared = SurfaceActor()
    private init() {}

    func makeCGImage(from surface: sending IOSurfaceRef) async -> Res {
        let ciImage = CIImage(ioSurface: surface)
        return context.createCGImage(ciImage, from: ciImage.extent.integral)
    }
}

#endif

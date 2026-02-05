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

internal import CommonCrypto
internal import CoreGraphics
internal import CoreImage
import os
import UniformTypeIdentifiers
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

            guard let self, let (data, hash) = await self.computeHash() else { return }

            Task { @MainActor [weak self] in
                try Task.checkCancellation()

                guard let self, self.cgImage != nil, self.imageHash != hash else { return }

                self.imageData = data
                self.imageHash = hash

                if let (oldImageHash, cachedMode) = self.cachedViewModeInfo, oldImageHash == hash {
                    self.viewMode = cachedMode
                } else {
                    self.viewMode = .unknown
                }

                if self.viewMode == .unknown {
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
    }

    @concurrent
    func computeHash() async -> (Data, NSString)? {
        guard let cgImage = await self.cgImage else { return nil }
        return cgImageAsDataWithHash(cgImage)
    }
}

// TODO: rdar://164555610 - https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines
class StreamHasher {
    var hashContext = CC_SHA256_CTX()
    var outputStream: OutputStream

    init(outputStream: OutputStream) {
        self.outputStream = outputStream
        CC_SHA256_Init(&hashContext)
    }
}

func cgImageAsDataWithHash(_ cgImage: CGImage) -> (Data, NSString)? {
    let outputStream = OutputStream(toMemory: ())
    outputStream.open()
    defer {
        outputStream.close()
    }

    let streamHasher = StreamHasher(outputStream: outputStream)
    let streamHasherRef = Unmanaged.passRetained(streamHasher).toOpaque()

    var callbacks = CGDataConsumerCallbacks(
        putBytes: { (info, buffer, count) -> Int in
            guard let info = info else { return 0 }

            let streamHasher = Unmanaged<StreamHasher>.fromOpaque(info).takeUnretainedValue()
            let dataBuffer = buffer.assumingMemoryBound(to: UInt8.self)

            CC_SHA256_Update(&streamHasher.hashContext, dataBuffer, CC_LONG(count))
            return streamHasher.outputStream.write(dataBuffer, maxLength: count)
        },
        releaseConsumer: { info in
            if let info = info {
                Unmanaged<StreamHasher>.fromOpaque(info).release()
            }
        }
    )

    guard let consumer = CGDataConsumer(info: streamHasherRef, cbks: &callbacks),
        let destination = CGImageDestinationCreateWithDataConsumer(consumer, UTType.bmp.identifier as CFString, 1, nil)
    else {
        return nil
    }

    CGImageDestinationAddImage(destination, cgImage, nil)

    if CGImageDestinationFinalize(destination), let finalData = outputStream.property(forKey: .dataWrittenToMemoryStreamKey) as? Data {
        var hash = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
        CC_SHA256_Final(&hash, &streamHasher.hashContext)
        let hashString = hash.map { String(format: "%02x", $0) }.joined()
        return (finalData, NSString(string: hashString))
    }

    return nil
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

/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import Foundation

public class Texture {
    internal let texture: __WGPUTexture
    private let ownership: Ownership

    internal init(_ texture: __WGPUTexture, ownership: Ownership = .Owned) {
        self.texture = texture
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuTextureRelease(texture)
        }
    }

    public func createView(_ descriptor: WGPUTextureViewDescriptor) -> TextureView {
        var localDescriptor = descriptor
        return TextureView(__wgpuTextureCreateView(texture, &localDescriptor))
    }

    public func destroy() {
        __wgpuTextureDestroy(texture)
    }

    public func getDepthOrArrayLayers() -> UInt32 {
        return __wgpuTextureGetDepthOrArrayLayers(texture)
    }

    public func getDimension() -> WGPUTextureDimension {
        return __wgpuTextureGetDimension(texture)
    }

    public func getFormat() -> WGPUTextureFormat {
        return __wgpuTextureGetFormat(texture)
    }

    public func getHeight() -> UInt32 {
        return __wgpuTextureGetHeight(texture)
    }

    public func getMipLevelCount() -> UInt32 {
        return __wgpuTextureGetMipLevelCount(texture)
    }

    public func getSampleCount() -> UInt32 {
        return __wgpuTextureGetSampleCount(texture)
    }

    public func getUsage() -> WGPUTextureUsage {
        return __wgpuTextureGetUsage(texture)
    }

    public func getWidth() -> UInt32 {
        return __wgpuTextureGetWidth(texture)
    }

    public func setLabel(_ label: String) {
        __wgpuTextureSetLabel(texture, label)
    }
}

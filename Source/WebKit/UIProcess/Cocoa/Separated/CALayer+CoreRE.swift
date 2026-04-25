// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS

#if canImport(CoreRE)
@_weakLinked import CoreRE
#endif
@_weakLinked @_spi(Private) @_spi(RealityKit) import RealityKit

extension CALayer {
    @MainActor
    var rootEntity: Entity? {
        #if canImport(CoreRE)
        // Safety: RECALayerGetCALayerClientComponent returns nil for layers without a
        // client component (not separated).
        // REComponentGetEntity returns the owning entity for a valid component.
        // Entity.fromCore creates a managed Entity handle from the core entity reference,
        // which is valid for the lifetime of the separated CALayer.
        // Tracking radar: rdar://174496029
        guard let component = unsafe RECALayerGetCALayerClientComponent(self) else {
            return nil
        }
        return unsafe Entity.fromCore(REComponentGetEntity(component))
        #else
        return nil
        #endif
    }
}

#endif

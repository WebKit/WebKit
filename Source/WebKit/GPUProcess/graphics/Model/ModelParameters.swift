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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreDeformation, _version: 23.0.2) && canImport(ShaderGraph, _version: 159.0.3) && arch(arm64)

import USDKit
import RealityKit

func makeParameters(
    for function: any LowLevelMaterialResource.Function,
    renderContext: any LowLevelRenderContext,
    buffers: [LowLevelBufferSlice] = [],
    textures: [LowLevelTextureResource] = []
) throws -> LowLevelArgumentTable? {
    guard let argumentTableDescriptor = function.argumentTableDescriptor else {
        return nil
    }
    return try renderContext.makeArgumentTable(
        descriptor: argumentTableDescriptor,
        buffers: buffers,
        textures: textures
    )
}

func makeParameters(
    for material: LowLevelMaterialResource,
    renderContext: any LowLevelRenderContext,
    geometryBuffers: [LowLevelBufferSlice] = [],
    geometryTextures: [LowLevelTextureResource] = [],
    surfaceBuffers: [LowLevelBufferSlice] = [],
    surfaceTextures: [LowLevelTextureResource] = [],
    lightingBuffers: [LowLevelBufferSlice] = [],
    lightingTextures: [LowLevelTextureResource] = []
) throws
    -> (
        geometryArguments: LowLevelArgumentTable?,
        surfaceArguments: LowLevelArgumentTable?,
        lightingArguments: LowLevelArgumentTable?
    )
{
    let geometryArguments = try makeParameters(
        for: material.geometry,
        renderContext: renderContext,
        buffers: geometryBuffers,
        textures: geometryTextures
    )
    let surfaceArguments = try makeParameters(
        for: material.surface,
        renderContext: renderContext,
        buffers: surfaceBuffers,
        textures: surfaceTextures
    )
    let lightingArguments = try makeParameters(
        for: material.lighting,
        renderContext: renderContext,
        buffers: lightingBuffers,
        textures: lightingTextures
    )

    return (geometryArguments, surfaceArguments, lightingArguments)
}

#endif

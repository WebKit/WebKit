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
// THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if (ENABLE_MODEL_PROCESS || ENABLE_GPU_PROCESS_MODEL) && canImport(USDKit)

public import CoreGraphics
public import Foundation
import os
import simd
import UniformTypeIdentifiers
@_weakLinked public import USDKit

extension os.Logger {
    fileprivate static let modelStage = Logger(subsystem: "com.apple.WebKit", category: "model")
}

extension USDStage {
    /// Opens the data of a `<model>` element as a stage.
    ///
    /// - Parameters:
    ///   - data: The model's data.
    ///   - mimeType: The model's MIME type, which selects the file format the data is opened in.
    ///   - surfaceColor: For an STL model, the color that replaces the diffuse color of its default material, or `nil` to keep it.
    /// - Throws: An error if the data can't be opened in that file format.
    @_spi(WebKitSwift)
    public init(modelData data: Foundation.Data, mimeType: Swift.String, surfaceColor: CGColor?) throws {
        try self.init(data, type: usdStageType(forMIMEType: mimeType))
        #if canImport(USDKit, _version: 106)
        if let surfaceColor {
            applySurfaceColor(surfaceColor, to: self)
        }
        #endif
    }
}

private func usdStageType(forMIMEType mimeType: String) -> UTType {
    // model/usd and model/vnd.pixar.usd are unofficial types that nothing declares,
    // so UTType(mimeType:) yields only a dyn.* placeholder for them.
    switch mimeType {
    case "model/vnd.usdz+zip": .usdz
    case "model/usd", "model/vnd.pixar.usd": .usd
    case "model/stl": UTType(filenameExtension: "stl") ?? .data
    default: UTType(mimeType: mimeType) ?? .usdz
    }
}

#if canImport(USDKit, _version: 106)

// USDLib's STL importer binds this material to the whole mesh, and authors only its diffuse color.
private let stlMaterialShaderPath = ["Materials", "defaultMaterial", "UsdPreviewSurface", "UsdPreviewSurface"]

private func applySurfaceColor(_ surfaceColor: CGColor, to stage: USDStage) {
    guard let root = stage.defaultPrim?.path else {
        Logger.modelStage.error("Failed to apply the surface color: the stage has no default prim")
        return
    }
    let shaderPath = stlMaterialShaderPath.reduce(root) {
        $0.appending(child: USDToken($1))
    }
    let shader = stage.prim(at: shaderPath)
    guard shader.isValid else {
        Logger.modelStage.error("Failed to apply the surface color: no material at \(String(describing: shaderPath), privacy: .public)")
        return
    }
    guard let diffuseColor = usdPreviewSurfaceColor(from: surfaceColor) else {
        Logger.modelStage.error("Failed to apply the surface color: it can't be converted to linear sRGB")
        return
    }
    shader.makeAttribute(named: USDToken("inputs:diffuseColor"), as: .color3f, custom: false)
        .setValue(USDValue.Vec3f(diffuseColor))
}

private func usdPreviewSurfaceColor(from color: CGColor) -> simd_float3? {
    guard let linearSRGB = CGColorSpace(name: CGColorSpace.linearSRGB),
        let converted = color.converted(to: linearSRGB, intent: .defaultIntent, options: nil),
        let components = converted.components, components.count >= 3
    else {
        return nil
    }
    return simd_float3(Float(components[0]), Float(components[1]), Float(components[2]))
}

#endif // canImport(USDKit, _version: 106)

#endif

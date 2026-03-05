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

import Foundation
import os

#if ENABLE_MODEL_PROCESS

internal import WebKit_Internal

#if canImport(_USDKit_RealityKit)

// FIXME: radar://141774327
@_weakLinked @_spi(Eryx) internal import _USDKit_RealityKit

extension Logger {
    fileprivate static let usdStageConverter = Logger(subsystem: "com.apple.WebKit", category: "USDStageConverter")
}

@objc
@implementation
extension WKUSDStageConverter {
    @objc(convert:)
    class func convert(_ data: Data) -> Data? {
        let stage: UsdStage
        do {
            stage = try UsdStage.open(buffer: data)
        } catch {
            Logger.usdStageConverter.error("WKUSDStageConverter: Failed to open stage: \(error)")
            return nil
        }
        do {
            // FIXME: radar://141774327
            return try stage.export(options: .preferSmallMeshFiles)
        } catch {
            Logger.usdStageConverter.error("WKUSDStageConverter: Failed to export to USDZ: \(error)")
            return nil
        }
    }
}

#else

@objc
@implementation
extension WKUSDStageConverter {
    @objc(convert:)
    class func convert(_ data: Data) -> Data? {
        nil
    }
}

#endif // canImport(_USDKit_RealityKit)

#endif // ENABLE_MODEL_PROCESS

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

public class ShaderModule {
    internal let shaderModule: __WGPUShaderModule
    private let instance: Instance
    private let ownership: Ownership

    internal init(_ shaderModule: __WGPUShaderModule, instance: Instance, ownership: Ownership = .Owned) {
        self.shaderModule = shaderModule
        self.instance = instance
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuShaderModuleRelease(shaderModule)
        }
    }

    // FIXME: Make this more Swifty.
    public func getCompilationInfo(_ label: String) async -> (WGPUCompilationInfoRequestStatus, UnsafePointer<WGPUCompilationInfo>?) {
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuShaderModuleGetCompilationInfoWithBlock(shaderModule) { (status, compilationInfo) in
                    continuation.resume(returning: (status, compilationInfo))
                }
            }
        }
    }

    public func setLabel(_ label: String) {
        __wgpuShaderModuleSetLabel(shaderModule, label)
    }
}

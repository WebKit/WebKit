/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebGPU/WebGPU.h>
#include <WebGPU/WebGPUCpp.h>

// The handles of the WebGPU C API for objects of the WebGPU C++ API, for a client that creates the
// objects with the C++ API and gives them to a client of the C API. The objects can come from any
// implementation of the C++ API. Each function returns the handle with a new reference of the C API,
// which the caller releases with the wgpu*Release() function of the handle.

// The WebGPU_Private module of WebGPU.framework includes every private header, so this header
// must also compile as C and Objective-C.
#ifdef __cplusplus

namespace WebGPU {

WEBGPU_EXPORT WGPUInstance retainCAPIHandle(Instance&);
WEBGPU_EXPORT WGPUDevice retainCAPIHandle(Device&);
WEBGPU_EXPORT WGPUTexture retainCAPIHandle(Texture&);

} // namespace WebGPU

#endif // __cplusplus

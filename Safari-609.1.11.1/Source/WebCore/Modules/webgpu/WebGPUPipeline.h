/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGPU)

#include "ContextDestructionObserver.h"
#include "GPUObjectBase.h"
#include "WebGPUShaderModule.h"
#include <wtf/Forward.h>
#include <wtf/Lock.h>

namespace WebCore {

class ScriptExecutionContext;
class GPUErrorScopes;
class WebGPUDevice;

class WebGPUPipeline : public GPUObjectBase, public ContextDestructionObserver {
public:
    virtual ~WebGPUPipeline();

    static HashMap<WebGPUPipeline*, WebGPUDevice*>& instances(const LockHolder&);
    static Lock& instancesMutex();

    virtual bool isRenderPipeline() const { return false; }
    virtual bool isComputePipeline() const { return false; }

    virtual bool isValid() const = 0;

    struct ShaderData {
        RefPtr<WebGPUShaderModule> module;
        String entryPoint;
    };

    virtual bool cloneShaderModules(const WebGPUDevice&) = 0;
    virtual bool recompile(const WebGPUDevice&) = 0;

    void contextDestroyed() final;

protected:
    WebGPUPipeline(WebGPUDevice&, GPUErrorScopes&);
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_WEBGPUPIPELINE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::WebGPUPipeline& pipeline) { return pipeline.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBGPU)

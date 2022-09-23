/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteShaderModuleProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteShaderModuleMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUCompilationInfo.h>
#include <pal/graphics/WebGPU/WebGPUCompilationMessage.h>

namespace WebKit::WebGPU {

RemoteShaderModuleProxy::RemoteShaderModuleProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteShaderModuleProxy::~RemoteShaderModuleProxy()
{
}

void RemoteShaderModuleProxy::compilationInfo(CompletionHandler<void(Ref<PAL::WebGPU::CompilationInfo>&&)>&& callback)
{
    auto sendResult = sendSync(Messages::RemoteShaderModule::CompilationInfo());
    auto [messages] = sendResult.takeReplyOr(Vector<CompilationMessage> { });

    auto backingMessages = messages.map([] (CompilationMessage compilationMessage) {
        return PAL::WebGPU::CompilationMessage::create(WTFMove(compilationMessage.message), compilationMessage.type, compilationMessage.lineNum, compilationMessage.linePos, compilationMessage.offset, compilationMessage.length);
    });
    callback(PAL::WebGPU::CompilationInfo::create(WTFMove(backingMessages)));
}

void RemoteShaderModuleProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteShaderModule::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

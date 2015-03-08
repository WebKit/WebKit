/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebCompiledContentExtension.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include <WebCore/ContentExtensionCompiler.h>

namespace WebKit {

Ref<WebCompiledContentExtension> WebCompiledContentExtension::createFromCompiledContentExtensionData(const WebCore::ContentExtensions::CompiledContentExtensionData& compilerData)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(compilerData.bytecode.size() + compilerData.actions.size());
    memcpy(static_cast<char*>(sharedMemory->data()), compilerData.bytecode.data(), compilerData.bytecode.size());
    memcpy(static_cast<char*>(sharedMemory->data()) + compilerData.bytecode.size(), compilerData.actions.data(), compilerData.actions.size());

    WebCompiledContentExtensionData data;
    data.data = WTF::move(sharedMemory);
    data.bytecodeOffset = 0;
    data.bytecodeSize = compilerData.bytecode.size();
    data.actionsOffset = compilerData.bytecode.size();
    data.actionsSize = compilerData.actions.size();

    return create(WTF::move(data));
}

Ref<WebCompiledContentExtension> WebCompiledContentExtension::create(WebCompiledContentExtensionData&& data)
{
    return adoptRef(*new WebCompiledContentExtension(WTF::move(data)));
}

WebCompiledContentExtension::WebCompiledContentExtension(WebCompiledContentExtensionData&& data)
    : m_data(WTF::move(data))
{
}

WebCompiledContentExtension::~WebCompiledContentExtension()
{
}

const WebCore::ContentExtensions::DFABytecode* WebCompiledContentExtension::bytecode() const
{
    return static_cast<const WebCore::ContentExtensions::DFABytecode*>(m_data.data->data()) + m_data.bytecodeOffset;
}

unsigned WebCompiledContentExtension::bytecodeLength() const
{
    return m_data.bytecodeSize;
}

const WebCore::ContentExtensions::SerializedActionByte* WebCompiledContentExtension::actions() const
{
    return static_cast<const WebCore::ContentExtensions::SerializedActionByte*>(m_data.data->data()) + m_data.actionsOffset;
}

unsigned WebCompiledContentExtension::actionsLength() const
{
    return m_data.actionsSize;
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)

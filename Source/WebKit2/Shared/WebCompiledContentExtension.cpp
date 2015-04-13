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

namespace WebKit {

LegacyContentExtensionCompilationClient::LegacyContentExtensionCompilationClient(WebCore::ContentExtensions::CompiledContentExtensionData& data)
    : m_data(data)
{
}

void LegacyContentExtensionCompilationClient::writeBytecode(Vector<WebCore::ContentExtensions::DFABytecode>&& bytecode)
{
    m_data.bytecode = WTF::move(bytecode);
}

void LegacyContentExtensionCompilationClient::writeActions(Vector<WebCore::ContentExtensions::SerializedActionByte>&& actions)
{
    m_data.actions = WTF::move(actions);
}


Ref<WebCompiledContentExtension> WebCompiledContentExtension::createFromCompiledContentExtensionData(const WebCore::ContentExtensions::CompiledContentExtensionData& compilerData)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(compilerData.bytecode.size() + compilerData.actions.size());
    memcpy(static_cast<char*>(sharedMemory->data()), compilerData.actions.data(), compilerData.actions.size());
    memcpy(static_cast<char*>(sharedMemory->data()) + compilerData.actions.size(), compilerData.bytecode.data(), compilerData.bytecode.size());

    NetworkCache::Data fileData; // We don't have an mmap'd file to keep alive here, so just use an empty Data object.
    WebCompiledContentExtensionData data(WTF::move(sharedMemory), fileData, 0, compilerData.actions.size(), compilerData.actions.size(), compilerData.bytecode.size());

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

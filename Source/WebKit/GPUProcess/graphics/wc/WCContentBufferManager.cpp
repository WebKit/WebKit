/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "WCContentBufferManager.h"

#if USE(GRAPHICS_LAYER_WC)

#include "WCContentBuffer.h"
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

class WCContentBufferManager::ProcessInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ProcessInfo(WCContentBufferManager& manager, WebCore::ProcessIdentifier processIdentifier)
        : m_manager(manager)
        , m_processIdentifier(processIdentifier) { }

    std::optional<WCContentBufferIdentifier> acquireContentBufferIdentifier(WebCore::TextureMapperPlatformLayer* platformLayer)
    {
        // FIXME: TextureMapperGCGLPlatformLayer doesn't support double buffering yet.
        // TextureMapperPlatformLayer can acquire a single WCContentBufferIdentifier.
        auto contentBufferAddResult = m_contentBuffers.ensure(platformLayer, [&] {
            return makeUnique<WCContentBuffer>(m_manager, m_processIdentifier, platformLayer);
        });
        WCContentBuffer* buffer = contentBufferAddResult.iterator->value.get();
        auto identifier = buffer->identifier();
        auto identifierAddResult = m_validIdentifiers.add(identifier, buffer);
        if (!identifierAddResult.isNewEntry)
            return std::nullopt;
        return identifier;
    }

    WCContentBuffer* releaseContentBufferIdentifier(WCContentBufferIdentifier identifier)
    {
        return m_validIdentifiers.take(identifier);
    }

    void removeContentBuffer(WCContentBuffer& contentBuffer)
    {
        m_validIdentifiers.remove(contentBuffer.identifier());
        m_contentBuffers.remove(contentBuffer.platformLayer());
    }

private:
    WCContentBufferManager& m_manager;
    WebCore::ProcessIdentifier m_processIdentifier;
    HashMap<WebCore::TextureMapperPlatformLayer*, std::unique_ptr<WCContentBuffer>> m_contentBuffers;
    HashMap<WCContentBufferIdentifier, WCContentBuffer*> m_validIdentifiers;
};

WCContentBufferManager& WCContentBufferManager::singleton()
{
    static NeverDestroyed<WCContentBufferManager> contentBufferManager;
    return contentBufferManager;
}

std::optional<WCContentBufferIdentifier> WCContentBufferManager::acquireContentBufferIdentifier(WebCore::ProcessIdentifier processIdentifier, WebCore::TextureMapperPlatformLayer* platformLayer)
{
    auto processAddResult = m_processMap.ensure(processIdentifier, [&] {
        return makeUnique<ProcessInfo>(*this, processIdentifier);
    });
    return processAddResult.iterator->value->acquireContentBufferIdentifier(platformLayer);
}

WCContentBuffer* WCContentBufferManager::releaseContentBufferIdentifier(WebCore::ProcessIdentifier processIdentifier, WCContentBufferIdentifier contentBufferIdentifier)
{
    ASSERT(m_processMap.contains(processIdentifier));
    return m_processMap.get(processIdentifier)->releaseContentBufferIdentifier(contentBufferIdentifier);
}

void WCContentBufferManager::removeContentBuffer(WebCore::ProcessIdentifier processIdentifier, WCContentBuffer& contentBuffer)
{
    ASSERT(m_processMap.contains(processIdentifier));
    m_processMap.get(processIdentifier)->removeContentBuffer(contentBuffer);
}

void WCContentBufferManager::removeAllContentBuffersForProcess(WebCore::ProcessIdentifier processIdentifier)
{
    m_processMap.remove(processIdentifier);
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)

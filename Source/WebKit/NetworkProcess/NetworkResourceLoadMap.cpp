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

#include "config.h"
#include "NetworkResourceLoadMap.h"

namespace WebKit {

NetworkResourceLoadMap::MapType::AddResult NetworkResourceLoadMap::add(ResourceLoadIdentifier identifier, Ref<NetworkResourceLoader>&& loader)
{
    auto result = m_loaders.add(identifier, WTFMove(loader));
    ASSERT(result.isNewEntry);
        
    if (result.iterator->value->originalRequest().hasUpload()) {
        if (m_loadersWithUploads.isEmpty())
            m_connectionToWebProcess.setConnectionHasUploads();
        m_loadersWithUploads.add(result.iterator->value.ptr());
    }

    return result;
}

bool NetworkResourceLoadMap::remove(ResourceLoadIdentifier identifier)
{
    auto loader = m_loaders.take(identifier);
    if (!loader)
        return false;

    if ((*loader)->originalRequest().hasUpload()) {
        m_loadersWithUploads.remove(loader->ptr());
        if (m_loadersWithUploads.isEmpty())
            m_connectionToWebProcess.clearConnectionHasUploads();
    }

    return true;
}

NetworkResourceLoader* NetworkResourceLoadMap::get(ResourceLoadIdentifier identifier) const
{
    return m_loaders.get(identifier);
}

} // namespace WebKit

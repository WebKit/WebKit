/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ResourceHandleClient.h"

#include "ResourceHandle.h"
#include "SharedBuffer.h"

#if USE(SOUP)
#include <glib.h>
#endif

namespace WebCore {

ResourceHandleClient::ResourceHandleClient()
#if USE(SOUP)
    : m_buffer(0)
#endif
{
}

ResourceHandleClient::~ResourceHandleClient()
{
#if USE(SOUP)
    if (m_buffer) {
        g_free(m_buffer);
        m_buffer = 0;
    }
#endif
}

void ResourceHandleClient::didReceiveBuffer(ResourceHandle* handle, PassRefPtr<SharedBuffer> buffer, int encodedDataLength)
{
    didReceiveData(handle, buffer->data(), buffer->size(), encodedDataLength);
}

#if USE(SOUP)
char* ResourceHandleClient::getBuffer(int requestedLength, int* actualLength)
{
    *actualLength = requestedLength;

    if (!m_buffer)
        m_buffer = static_cast<char*>(g_malloc(requestedLength));

    return m_buffer;
}
#endif

}

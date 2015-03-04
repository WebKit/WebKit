/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "NetworkCachedata.h"

#if ENABLE(NETWORK_CACHE)

#include <dispatch/dispatch.h>

namespace WebKit {

NetworkCacheData::NetworkCacheData(const uint8_t* data, size_t size)
    : m_dispatchData(adoptDispatch(dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT)))
    , m_size(size)
{
}

NetworkCacheData::NetworkCacheData(DispatchPtr<dispatch_data_t> dispatchData, Backing backing)
{
    if (!dispatchData)
        return;
    const void* data;
    m_dispatchData = adoptDispatch(dispatch_data_create_map(dispatchData.get(), &data, &m_size));
    m_data = static_cast<const uint8_t*>(data);
    m_isMap = m_size && backing == Backing::Map;
}

const uint8_t* NetworkCacheData::data() const
{
    if (!m_data) {
        const void* data;
        size_t size;
        m_dispatchData = adoptDispatch(dispatch_data_create_map(m_dispatchData.get(), &data, &size));
        ASSERT(size == m_size);
        m_data = static_cast<const uint8_t*>(data);
    }
    return m_data;
}

bool NetworkCacheData::isNull() const
{
    return !m_dispatchData;
}

}

#endif

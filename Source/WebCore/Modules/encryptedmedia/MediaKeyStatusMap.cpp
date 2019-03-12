/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaKeyStatusMap.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "JSMediaKeyStatusMap.h"
#include "MediaKeySession.h"
#include "SharedBuffer.h"
#include <wtf/Optional.h>

namespace WebCore {

MediaKeyStatusMap::MediaKeyStatusMap(const MediaKeySession& session)
    : m_session(&session)
{
}

MediaKeyStatusMap::~MediaKeyStatusMap() = default;

void MediaKeyStatusMap::detachSession()
{
    m_session = nullptr;
}

unsigned long MediaKeyStatusMap::size()
{
    if (!m_session)
        return 0;
    return m_session->statuses().size();
}

static bool keyIdsMatch(const SharedBuffer& a, const BufferSource& b)
{
    auto length = a.size();
    if (!length || length != b.length())
        return false;
    return !std::memcmp(a.data(), b.data(), length);
}

bool MediaKeyStatusMap::has(const BufferSource& keyId)
{
    if (!m_session)
        return false;

    auto& statuses = m_session->statuses();
    return std::any_of(statuses.begin(), statuses.end(),
        [&keyId] (auto& it) { return keyIdsMatch(it.first, keyId); });
}

JSC::JSValue MediaKeyStatusMap::get(JSC::ExecState& state, const BufferSource& keyId)
{
    if (!m_session)
        return JSC::jsUndefined();

    auto& statuses = m_session->statuses();
    auto it = std::find_if(statuses.begin(), statuses.end(),
        [&keyId] (auto& it) { return keyIdsMatch(it.first, keyId); });

    if (it == statuses.end())
        return JSC::jsUndefined();
    return convertEnumerationToJS(state, it->second);
}

MediaKeyStatusMap::Iterator::Iterator(MediaKeyStatusMap& map)
    : m_map(map)
{
}

Optional<WTF::KeyValuePair<BufferSource::VariantType, MediaKeyStatus>> MediaKeyStatusMap::Iterator::next()
{
    if (!m_map->m_session)
        return WTF::nullopt;

    auto& statuses = m_map->m_session->statuses();
    if (m_index >= statuses.size())
        return WTF::nullopt;

    auto& pair = statuses[m_index++];
    auto buffer = ArrayBuffer::create(pair.first->data(), pair.first->size());
    return WTF::KeyValuePair<BufferSource::VariantType, MediaKeyStatus> { RefPtr<ArrayBuffer>(WTFMove(buffer)), pair.second };
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

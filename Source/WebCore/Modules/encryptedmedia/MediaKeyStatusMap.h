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

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "BufferSource.h"
#include "MediaKeyStatus.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaKeySession;
class SharedBuffer;

class MediaKeyStatusMap : public RefCounted<MediaKeyStatusMap> {
public:
    using Status = MediaKeyStatus;

    static Ref<MediaKeyStatusMap> create(const MediaKeySession& session)
    {
        return adoptRef(*new MediaKeyStatusMap(session));
    }

    virtual ~MediaKeyStatusMap();

    void detachSession();

    unsigned long size();
    bool has(const BufferSource&);
    JSC::JSValue get(JSC::JSGlobalObject&, const BufferSource&);

    class Iterator {
    public:
        explicit Iterator(MediaKeyStatusMap&);
        std::optional<WTF::KeyValuePair<BufferSource::VariantType, MediaKeyStatus>> next();

    private:
        Ref<MediaKeyStatusMap> m_map;
        size_t m_index { 0 };
    };
    Iterator createIterator() { return Iterator(*this); }

private:
    MediaKeyStatusMap(const MediaKeySession&);

    const MediaKeySession* m_session;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamSource_h
#define MediaStreamSource_h

#if ENABLE(MEDIA_STREAM)

#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaStreamSource : public RefCounted<MediaStreamSource> {
public:
    enum Type {
        TypeAudio,
        TypeVideo
    };

    static PassRefPtr<MediaStreamSource> create(const String& id, Type type, const String& name)
    {
        return adoptRef(new MediaStreamSource(id, type, name));
    }

    const String& id() const { return m_id; }
    Type type() const { return m_type; }
    const String& name() const { return m_name; }

    bool muted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }

private:
    MediaStreamSource(const String& id, Type type, const String& name)
        : m_id(id)
        , m_type(type)
        , m_name(name)
        , m_muted(false)
    {
    }

    String m_id;
    Type m_type;
    String m_name;
    bool m_muted;
};

typedef Vector<RefPtr<MediaStreamSource> > MediaStreamSourceVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamSource_h

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamContainer_h
#define MediaStreamContainer_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStream.h"
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaStreamContainer : public RefCounted<MediaStreamContainer> {
public:
    static PassRefPtr<MediaStreamContainer> create() { return adoptRef(new MediaStreamContainer); }
    virtual ~MediaStreamContainer() { }

    unsigned length() const { return m_streams.size(); }

    PassRefPtr<MediaStream> item(unsigned index) const
    {
        HashMap<String, RefPtr<MediaStream> >::const_iterator i = m_streams.begin();
        for (unsigned j = 0; i != m_streams.end(); ++i, ++j) {
            if (j == index)
                return i->second;
        }
        return PassRefPtr<MediaStream>();
    }

    void add(PassRefPtr<MediaStream> stream)
    {
        RefPtr<MediaStream> s = stream;
        ASSERT(!contains(s));
        m_streams.add(s->label(), s);
    }

    void remove(PassRefPtr<MediaStream> stream)
    {
        RefPtr<MediaStream> s = stream;
        ASSERT(contains(s));
        m_streams.remove(s->label());
    }

    bool contains(PassRefPtr<MediaStream> stream) const
    {
        RefPtr<MediaStream> s = stream;
        return m_streams.contains(s->label());
    }

    PassRefPtr<MediaStream> get(const String& key) const
    {
        return m_streams.get(key);
    }

private:
    MediaStreamContainer() { }

    HashMap<String, RefPtr<MediaStream> > m_streams;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamContainer_h

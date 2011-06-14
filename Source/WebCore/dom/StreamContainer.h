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

#ifndef StreamContainer_h
#define StreamContainer_h

#if ENABLE(MEDIA_STREAM)

#include "Stream.h"
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class StreamContainer : public RefCounted<StreamContainer> {
public:
    static PassRefPtr<StreamContainer> create() { return adoptRef(new StreamContainer); }
    virtual ~StreamContainer() { }

    unsigned length() const { return m_streams.size(); }

    PassRefPtr<Stream> item(unsigned index) const
    {
        HashMap<String, RefPtr<Stream> >::const_iterator i = m_streams.begin();
        for (unsigned j = 0; i != m_streams.end(); ++i, ++j) {
            if (j == index)
                return i->second;
        }
        return PassRefPtr<Stream>();
    }

    void add(PassRefPtr<Stream> stream)
    {
        RefPtr<Stream> s = stream;
        ASSERT(!contains(s));
        m_streams.add(s->label(), s);
    }

    void remove(PassRefPtr<Stream> stream)
    {
        RefPtr<Stream> s = stream;
        ASSERT(contains(s));
        m_streams.remove(s->label());
    }

    bool contains(PassRefPtr<Stream> stream) const
    {
        RefPtr<Stream> s = stream;
        return m_streams.contains(s->label());
    }

    PassRefPtr<Stream> get(const String& key) const
    {
        return m_streams.get(key);
    }

private:
    StreamContainer() { }

    HashMap<String, RefPtr<Stream> > m_streams;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // StreamContainer_h

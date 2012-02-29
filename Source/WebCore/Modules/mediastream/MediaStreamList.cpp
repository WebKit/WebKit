/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#include "config.h"
#include "MediaStreamList.h"

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

PassRefPtr<MediaStreamList> MediaStreamList::create()
{
    return adoptRef(new MediaStreamList());
}

MediaStreamList::MediaStreamList()
{
}

MediaStreamList::~MediaStreamList()
{
}

unsigned MediaStreamList::length() const
{
    return m_streams.size();
}

MediaStream* MediaStreamList::item(unsigned index) const
{
    if (index >= m_streams.size())
        return 0;
    return m_streams[index].get();
}

void MediaStreamList::append(PassRefPtr<MediaStream> stream)
{
    m_streams.append(stream);
}

void MediaStreamList::remove(const MediaStream* stream)
{
    size_t i = m_streams.find(stream);
    if (i != notFound)
        return m_streams.remove(i);
}

bool MediaStreamList::contains(const MediaStream* stream) const
{
    return m_streams.contains(stream);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

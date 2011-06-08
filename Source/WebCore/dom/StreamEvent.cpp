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

#include "config.h"
#include "StreamEvent.h"

#if ENABLE(MEDIA_STREAM)

#include "EventNames.h"
#include "Stream.h"

namespace WebCore {

PassRefPtr<StreamEvent> StreamEvent::create()
{
    return adoptRef(new StreamEvent);
}

PassRefPtr<StreamEvent> StreamEvent::create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<Stream> stream)
{
    return adoptRef(new StreamEvent(type, canBubble, cancelable, stream));
}


StreamEvent::StreamEvent()
{
}

StreamEvent::StreamEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<Stream> stream)
    : Event(type, canBubble, cancelable)
    , m_stream(stream)
{
}

StreamEvent::~StreamEvent()
{
}

void StreamEvent::initStreamEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<Stream> stream)
{
    if (dispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_stream = stream;
}

PassRefPtr<Stream> StreamEvent::stream() const
{
    return m_stream;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)


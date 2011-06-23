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

#ifndef StreamEvent_h
#define StreamEvent_h

#if ENABLE(MEDIA_STREAM)

#include "Event.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

class Stream;

class StreamEvent : public Event {
public:
    virtual ~StreamEvent();

    static PassRefPtr<StreamEvent> create();
    static PassRefPtr<StreamEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<MediaStream>);

    void initStreamEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<MediaStream>);

    // From EventTarget.
    virtual bool isMediaStreamEvent() const { return true; }

    PassRefPtr<MediaStream> stream() const;

private:
    StreamEvent();
    StreamEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<MediaStream>);

    RefPtr<MediaStream> m_stream;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // StreamEvent_h

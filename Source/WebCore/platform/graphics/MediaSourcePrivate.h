/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#ifndef MediaSourcePrivate_h
#define MediaSourcePrivate_h

#if ENABLE(MEDIA_SOURCE)

#include "TimeRanges.h"

namespace WebCore {

class MediaSourcePrivate {
    WTF_MAKE_NONCOPYABLE(MediaSourcePrivate); WTF_MAKE_FAST_ALLOCATED;
public:
    MediaSourcePrivate() { }
    virtual ~MediaSourcePrivate() { }

    enum AddIdStatus { Ok, NotSupported, ReachedIdLimit };
    virtual AddIdStatus addId(const String& id, const String& type, const Vector<String>& codecs) = 0;
    virtual bool removeId(const String& id) = 0;
    virtual PassRefPtr<TimeRanges> buffered(const String& id) = 0;
    virtual bool append(const String& id, const unsigned char* data, unsigned length) = 0;
    virtual double duration() = 0;
    virtual void setDuration(double) = 0;
    virtual bool abort(const String& id) = 0;
    enum EndOfStreamStatus { EosNoError, EosNetworkError, EosDecodeError };
    virtual void endOfStream(EndOfStreamStatus) = 0;
    virtual bool setTimestampOffset(const String& id, double offset) = 0;
};

}

#endif
#endif

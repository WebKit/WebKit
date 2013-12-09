/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef MediaSample_h
#define MediaSample_h

#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class MockSampleBox;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

struct PlatformSample {
    enum {
        None,
        MockSampleBoxType,
        CMSampleBufferType,
    } type;
    union {
        MockSampleBox* mockSampleBox;
        CMSampleBufferRef cmSampleBuffer;
    } sample;
};

class MediaSample : public RefCounted<MediaSample> {
public:
    virtual ~MediaSample() { }

    virtual MediaTime presentationTime() const = 0;
    virtual MediaTime decodeTime() const = 0;
    virtual MediaTime duration() const = 0;
    virtual AtomicString trackID() const = 0;

    enum SampleFlags {
        None = 0,
        IsSync = 1 << 0,
        NonDisplaying = 1 << 1,
    };
    virtual SampleFlags flags() const = 0;
    virtual PlatformSample platformSample() = 0;

    bool isSync() const { return flags() & IsSync; }
    bool isNonDisplaying() const { return flags() & NonDisplaying; }
};

}

#endif

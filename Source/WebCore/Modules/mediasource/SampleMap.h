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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SampleMap_h
#define SampleMap_h

#if ENABLE(MEDIA_SOURCE)

#include <map>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaSample;

class SampleMap {
public:
    typedef std::map<MediaTime, RefPtr<MediaSample>> MapType;
    typedef MapType::iterator iterator;
    typedef MapType::reverse_iterator reverse_iterator;
    typedef std::pair<iterator, iterator> iterator_range;
    typedef std::pair<reverse_iterator, reverse_iterator> reverse_iterator_range;

    void addSample(PassRefPtr<MediaSample>);
    void removeSample(MediaSample*);

    iterator presentationBegin() { return m_presentationSamples.begin(); }
    iterator presentationEnd() { return m_presentationSamples.end(); }
    iterator decodeBegin() { return m_decodeSamples.begin(); }
    iterator decodeEnd() { return m_decodeSamples.end(); }
    reverse_iterator reversePresentationBegin() { return m_presentationSamples.rbegin(); }
    reverse_iterator reversePresentationEnd() { return m_presentationSamples.rend(); }
    reverse_iterator reverseDecodeBegin() { return m_decodeSamples.rbegin(); }
    reverse_iterator reverseDecodeEnd() { return m_decodeSamples.rend(); }

    iterator findSampleContainingPresentationTime(const MediaTime&);
    iterator findSampleAfterPresentationTime(const MediaTime&);
    iterator findSampleWithDecodeTime(const MediaTime&);
    reverse_iterator reverseFindSampleContainingPresentationTime(const MediaTime&);
    reverse_iterator reverseFindSampleBeforePresentationTime(const MediaTime&);
    reverse_iterator reverseFindSampleWithDecodeTime(const MediaTime&);
    reverse_iterator findSyncSamplePriorToPresentationTime(const MediaTime&, const MediaTime& threshold = MediaTime::positiveInfiniteTime());
    reverse_iterator findSyncSamplePriorToDecodeIterator(reverse_iterator);
    iterator findSyncSampleAfterPresentationTime(const MediaTime&, const MediaTime& threshold = MediaTime::positiveInfiniteTime());
    iterator findSyncSampleAfterDecodeIterator(iterator);

    iterator_range findSamplesBetweenPresentationTimes(const MediaTime&, const MediaTime&);
    reverse_iterator_range findDependentSamples(MediaSample*);
    
private:
    MapType m_presentationSamples;
    MapType m_decodeSamples;
};

}

#endif

#endif // SampleMap_h

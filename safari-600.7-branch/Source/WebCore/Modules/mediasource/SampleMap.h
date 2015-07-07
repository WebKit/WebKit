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
class SampleMap;

class PresentationOrderSampleMap {
    friend class SampleMap;
public:
    typedef std::map<MediaTime, RefPtr<MediaSample>> MapType;
    typedef MapType::iterator iterator;
    typedef MapType::reverse_iterator reverse_iterator;
    typedef std::pair<iterator, iterator> iterator_range;

    iterator begin() { return m_samples.begin(); }
    iterator end() { return m_samples.end(); }
    reverse_iterator rbegin() { return m_samples.rbegin(); }
    reverse_iterator rend() { return m_samples.rend(); }

    iterator findSampleWithPresentationTime(const MediaTime&);
    iterator findSampleContainingPresentationTime(const MediaTime&);
    iterator findSampleOnOrAfterPresentationTime(const MediaTime&);
    reverse_iterator reverseFindSampleContainingPresentationTime(const MediaTime&);
    reverse_iterator reverseFindSampleBeforePresentationTime(const MediaTime&);
    iterator_range findSamplesBetweenPresentationTimes(const MediaTime&, const MediaTime&);
    iterator_range findSamplesWithinPresentationRange(const MediaTime&, const MediaTime&);
    iterator_range findSamplesWithinPresentationRangeFromEnd(const MediaTime&, const MediaTime&);

private:
    MapType m_samples;
};

class DecodeOrderSampleMap {
    friend class SampleMap;
public:
    typedef std::pair<MediaTime, MediaTime> KeyType;
    typedef std::map<KeyType, RefPtr<MediaSample>> MapType;
    typedef MapType::iterator iterator;
    typedef MapType::reverse_iterator reverse_iterator;
    typedef std::pair<reverse_iterator, reverse_iterator> reverse_iterator_range;

    iterator begin() { return m_samples.begin(); }
    iterator end() { return m_samples.end(); }
    reverse_iterator rbegin() { return m_samples.rbegin(); }
    reverse_iterator rend() { return m_samples.rend(); }

    iterator findSampleWithDecodeKey(const KeyType&);
    reverse_iterator reverseFindSampleWithDecodeKey(const KeyType&);
    reverse_iterator findSyncSamplePriorToPresentationTime(const MediaTime&, const MediaTime& threshold = MediaTime::positiveInfiniteTime());
    reverse_iterator findSyncSamplePriorToDecodeIterator(reverse_iterator);
    iterator findSyncSampleAfterPresentationTime(const MediaTime&, const MediaTime& threshold = MediaTime::positiveInfiniteTime());
    iterator findSyncSampleAfterDecodeIterator(iterator);
    reverse_iterator_range findDependentSamples(MediaSample*);

private:
    MapType m_samples;
    PresentationOrderSampleMap m_presentationOrder;
};

class SampleMap {
public:
    SampleMap()
        : m_totalSize(0)
    {
    }

    bool empty() const;
    void clear();
    void addSample(PassRefPtr<MediaSample>);
    void removeSample(MediaSample*);
    size_t sizeInBytes() const { return m_totalSize; }

    template<typename I>
    void addRange(I begin, I end);

    DecodeOrderSampleMap& decodeOrder() { return m_decodeOrder; }
    const DecodeOrderSampleMap& decodeOrder() const { return m_decodeOrder; }
    PresentationOrderSampleMap& presentationOrder() { return m_decodeOrder.m_presentationOrder; }
    const PresentationOrderSampleMap& presentationOrder() const { return m_decodeOrder.m_presentationOrder; }

private:
    DecodeOrderSampleMap m_decodeOrder;
    size_t m_totalSize;
};

template<typename I>
void SampleMap::addRange(I begin, I end)
{
    for (I iter = begin; iter != end; ++iter)
        addSample(iter->second);
}

}

#endif

#endif // SampleMap_h

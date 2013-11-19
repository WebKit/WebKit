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

#include "config.h"
#include "SampleMap.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSample.h"

namespace WebCore {

class SampleIsLessThanMediaTimeComparator {
public:
    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value, MediaTime time)
    {
        MediaTime presentationEndTime = value.second->presentationTime() + value.second->duration();
        return presentationEndTime <= time;
    }
    bool operator()(MediaTime time, std::pair<MediaTime, RefPtr<MediaSample>> value)
    {
        MediaTime presentationStartTime = value.second->presentationTime();
        return time < presentationStartTime;
    }
};

class SampleIsGreaterThanMediaTimeComparator {
public:
    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value, MediaTime time)
    {
        MediaTime presentationStartTime = value.second->presentationTime();
        return presentationStartTime > time;
    }
    bool operator()(MediaTime time, std::pair<MediaTime, RefPtr<MediaSample>> value)
    {
        MediaTime presentationEndTime = value.second->presentationTime() + value.second->duration();
        return time >= presentationEndTime;
    }
};

class SampleIsRandomAccess {
public:
    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value)
    {
        return value.second->flags() == MediaSample::IsSync;
    }
};

class SamplePresentationTimeIsWithinRangeComparator {
public:
    bool operator()(std::pair<MediaTime, MediaTime> range, std::pair<MediaTime, RefPtr<MediaSample>> value)
    {
        return range.second < value.first;
    }
    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value, std::pair<MediaTime, MediaTime> range)
    {
        return value.first < range.first;
    }
};

void SampleMap::addSample(PassRefPtr<MediaSample> prpSample)
{
    RefPtr<MediaSample> sample = prpSample;
    ASSERT(sample);
    m_presentationSamples.insert(MapType::value_type(sample->presentationTime(), sample));
    m_decodeSamples.insert(MapType::value_type(sample->decodeTime(), sample));
}

void SampleMap::removeSample(MediaSample* sample)
{
    ASSERT(sample);
    m_presentationSamples.erase(sample->presentationTime());
    m_decodeSamples.erase(sample->decodeTime());
}

SampleMap::iterator SampleMap::findSampleContainingPresentationTime(const MediaTime& time)
{
    return std::equal_range(presentationBegin(), presentationEnd(), time, SampleIsLessThanMediaTimeComparator()).first;
}

SampleMap::iterator SampleMap::findSampleAfterPresentationTime(const MediaTime& time)
{
    return std::lower_bound(presentationBegin(), presentationEnd(), time, SampleIsLessThanMediaTimeComparator());
}

SampleMap::iterator SampleMap::findSampleWithDecodeTime(const MediaTime& time)
{
    return m_decodeSamples.find(time);
}

SampleMap::reverse_iterator SampleMap::reverseFindSampleContainingPresentationTime(const MediaTime& time)
{
    return std::equal_range(reversePresentationBegin(), reversePresentationEnd(), time, SampleIsGreaterThanMediaTimeComparator()).first;
}

SampleMap::reverse_iterator SampleMap::reverseFindSampleBeforePresentationTime(const MediaTime& time)
{
    return std::lower_bound(reversePresentationBegin(), reversePresentationEnd(), time, SampleIsGreaterThanMediaTimeComparator());
}

SampleMap::reverse_iterator SampleMap::reverseFindSampleWithDecodeTime(const MediaTime& time)
{
    SampleMap::iterator found = findSampleWithDecodeTime(time);
    if (found == decodeEnd())
        return reverseDecodeEnd();
    return --reverse_iterator(found);
}

SampleMap::reverse_iterator SampleMap::findSyncSamplePriorToPresentationTime(const MediaTime& time, const MediaTime& threshold)
{
    reverse_iterator reverseCurrentSamplePTS = reverseFindSampleBeforePresentationTime(time);
    if (reverseCurrentSamplePTS == reversePresentationEnd())
        return reverseDecodeEnd();

    reverse_iterator reverseCurrentSampleDTS = reverseFindSampleWithDecodeTime(reverseCurrentSamplePTS->second->decodeTime());

    reverse_iterator foundSample = findSyncSamplePriorToDecodeIterator(reverseCurrentSampleDTS);
    if (foundSample == reverseDecodeEnd())
        return reverseDecodeEnd();
    if (foundSample->second->presentationTime() < time - threshold)
        return reverseDecodeEnd();
    return foundSample;
}

SampleMap::reverse_iterator SampleMap::findSyncSamplePriorToDecodeIterator(reverse_iterator iterator)
{
    return std::find_if(iterator, reverseDecodeEnd(), SampleIsRandomAccess());
}

SampleMap::iterator SampleMap::findSyncSampleAfterPresentationTime(const MediaTime& time, const MediaTime& threshold)
{
    iterator currentSamplePTS = findSampleAfterPresentationTime(time);
    if (currentSamplePTS == presentationEnd())
        return decodeEnd();

    iterator currentSampleDTS = findSampleWithDecodeTime(currentSamplePTS->second->decodeTime());
    
    MediaTime upperBound = time + threshold;
    iterator foundSample = std::find_if(currentSampleDTS, decodeEnd(), SampleIsRandomAccess());
    if (foundSample == decodeEnd())
        return decodeEnd();
    if (foundSample->second->presentationTime() > upperBound)
        return decodeEnd();
    return foundSample;
}

SampleMap::iterator SampleMap::findSyncSampleAfterDecodeIterator(iterator currentSampleDTS)
{
    return std::find_if(currentSampleDTS, decodeEnd(), SampleIsRandomAccess());
}

SampleMap::iterator_range SampleMap::findSamplesBetweenPresentationTimes(const MediaTime& begin, const MediaTime& end)
{
    std::pair<MediaTime, MediaTime> range(begin, end);
    return std::equal_range(presentationBegin(), presentationEnd(), range, SamplePresentationTimeIsWithinRangeComparator());
}

SampleMap::reverse_iterator_range SampleMap::findDependentSamples(MediaSample* sample)
{
    ASSERT(sample);
    reverse_iterator currentDecodeIter = reverseFindSampleWithDecodeTime(sample->decodeTime());
    reverse_iterator nextSyncSample = findSyncSamplePriorToDecodeIterator(currentDecodeIter);
    return reverse_iterator_range(currentDecodeIter, nextSyncSample);
}

}

#endif

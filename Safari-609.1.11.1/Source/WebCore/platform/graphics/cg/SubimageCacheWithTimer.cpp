/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SubimageCacheWithTimer.h"

#if USE(CG)

#include <wtf/Vector.h>

#if CACHE_SUBIMAGES

namespace WebCore {

SubimageCacheWithTimer* SubimageCacheWithTimer::s_cache;

static const Seconds subimageCacheClearDelay { 1_s };
static const int maxSubimageCacheSize = 300;

RetainPtr<CGImageRef> SubimageCacheWithTimer::getSubimage(CGImageRef image, const FloatRect& rect)
{
    return subimageCache().subimage(image, rect);
}

void SubimageCacheWithTimer::clearImage(CGImageRef image)
{
    if (subimageCacheExists())
        subimageCache().clearImageAndSubimages(image);
}

struct SubimageRequest {
    CGImageRef image;
    const FloatRect& rect;
    SubimageRequest(CGImageRef image, const FloatRect& rect) : image(image), rect(rect) { }
};

struct SubimageCacheAdder {
    static unsigned hash(const SubimageRequest& value)
    {
        return SubimageCacheWithTimer::SubimageCacheHash::hash(value.image, value.rect);
    }

    static bool equal(const SubimageCacheWithTimer::SubimageCacheEntry& a, const SubimageRequest& b)
    {
        return a.image == b.image && a.rect == b.rect;
    }

    static void translate(SubimageCacheWithTimer::SubimageCacheEntry& entry, const SubimageRequest& request, unsigned /*hashCode*/)
    {
        entry.image = request.image;
        entry.rect = request.rect;
        entry.subimage = adoptCF(CGImageCreateWithImageInRect(request.image, request.rect));
    }
};

SubimageCacheWithTimer::SubimageCacheWithTimer()
    : m_timer(*this, &SubimageCacheWithTimer::invalidateCacheTimerFired, subimageCacheClearDelay)
{
}

void SubimageCacheWithTimer::invalidateCacheTimerFired()
{
    m_images.clear();
    m_cache.clear();
}

RetainPtr<CGImageRef> SubimageCacheWithTimer::subimage(CGImageRef image, const FloatRect& rect)
{
    m_timer.restart();
    if (m_cache.size() == maxSubimageCacheSize) {
        SubimageCacheEntry entry = *m_cache.begin();
        m_images.remove(entry.image.get());
        m_cache.remove(entry);
    }

    ASSERT(m_cache.size() < maxSubimageCacheSize);
    auto result = m_cache.add<SubimageCacheAdder>(SubimageRequest(image, rect));
    if (result.isNewEntry)
        m_images.add(image);

    return result.iterator->subimage;
}

void SubimageCacheWithTimer::clearImageAndSubimages(CGImageRef image)
{
    if (m_images.contains(image)) {
        Vector<SubimageCacheEntry> toBeRemoved;
        for (const auto& entry : m_cache) {
            if (entry.image.get() == image)
                toBeRemoved.append(entry);
        }

        for (auto& entry : toBeRemoved)
            m_cache.remove(entry);

        m_images.removeAll(image);
    }
}

SubimageCacheWithTimer& SubimageCacheWithTimer::subimageCache()
{
    if (!s_cache)
        s_cache = new SubimageCacheWithTimer;
    return *s_cache;
}

bool SubimageCacheWithTimer::subimageCacheExists()
{
    return !!s_cache;
}

}

#endif

#endif

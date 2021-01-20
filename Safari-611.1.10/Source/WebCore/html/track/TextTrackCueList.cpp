/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "TextTrackCueList.h"

// Checking sorting is too slow for general use; turn it on explicitly when working on this class.
#undef CHECK_SORTING

#ifdef CHECK_SORTING
#define ASSERT_SORTED(begin, end) ASSERT(std::is_sorted(begin, end, cueSortsBefore))
#else
#define ASSERT_SORTED(begin, end) ((void)0)
#endif

namespace WebCore {

static inline bool cueSortsBefore(const RefPtr<TextTrackCue>& a, const RefPtr<TextTrackCue>& b)
{
    if (a->startMediaTime() < b->startMediaTime())
        return true;

    return a->startMediaTime() == b->startMediaTime() && a->endMediaTime() > b->endMediaTime();
}

Ref<TextTrackCueList> TextTrackCueList::create()
{
    return adoptRef(*new TextTrackCueList);
}

unsigned TextTrackCueList::cueIndex(const TextTrackCue& cue) const
{
    ASSERT(m_vector.contains(&cue));
    return m_vector.find(&cue);
}

TextTrackCue* TextTrackCueList::item(unsigned index) const
{
    if (index >= m_vector.size())
        return nullptr;
    return m_vector[index].get();
}

TextTrackCue* TextTrackCueList::getCueById(const String& id) const
{
    for (auto& cue : m_vector) {
        if (cue->id() == id)
            return cue.get();
    }
    return nullptr;
}

TextTrackCueList& TextTrackCueList::activeCues()
{
    if (!m_activeCues)
        m_activeCues = create();

    Vector<RefPtr<TextTrackCue>> activeCuesVector;
    for (auto& cue : m_vector) {
        if (cue->isActive())
            activeCuesVector.append(cue);
    }
    ASSERT_SORTED(activeCuesVector.begin(), activeCuesVector.end());
    m_activeCues->m_vector = WTFMove(activeCuesVector);

    // FIXME: This list of active cues is not updated as cues are added, removed, become active, and become inactive.
    // Instead it is only updated each time this function is called again. That is not consistent with other dynamic DOM lists.
    return *m_activeCues;
}

void TextTrackCueList::add(Ref<TextTrackCue>&& cue)
{
    ASSERT(!m_vector.contains(cue.ptr()));
    ASSERT(cue->startMediaTime() >= MediaTime::zeroTime());
    ASSERT(cue->endMediaTime() >= MediaTime::zeroTime());

    RefPtr<TextTrackCue> cueRefPtr { WTFMove(cue) };
    unsigned insertionPosition = std::upper_bound(m_vector.begin(), m_vector.end(), cueRefPtr, cueSortsBefore) - m_vector.begin();
    ASSERT_SORTED(m_vector.begin(), m_vector.end());
    m_vector.insert(insertionPosition, WTFMove(cueRefPtr));
    ASSERT_SORTED(m_vector.begin(), m_vector.end());
}

void TextTrackCueList::remove(TextTrackCue& cue)
{
    ASSERT_SORTED(m_vector.begin(), m_vector.end());
    m_vector.remove(cueIndex(cue));
    ASSERT_SORTED(m_vector.begin(), m_vector.end());
}

void TextTrackCueList::clear()
{
    m_vector.clear();
    if (m_activeCues)
        m_activeCues->m_vector.clear();
}

void TextTrackCueList::updateCueIndex(const TextTrackCue& cue)
{
    auto cuePosition = m_vector.begin() + cueIndex(cue);
    auto afterCuePosition = cuePosition + 1;

    ASSERT_SORTED(m_vector.begin(), cuePosition);
    ASSERT_SORTED(afterCuePosition, m_vector.end());

    auto reinsertionPosition = std::upper_bound(m_vector.begin(), cuePosition, *cuePosition, cueSortsBefore);
    if (reinsertionPosition != cuePosition)
        std::rotate(reinsertionPosition, cuePosition, afterCuePosition);
    else {
        reinsertionPosition = std::upper_bound(afterCuePosition, m_vector.end(), *cuePosition, cueSortsBefore);
        if (reinsertionPosition != afterCuePosition)
            std::rotate(cuePosition, afterCuePosition, reinsertionPosition);
    }

    ASSERT_SORTED(m_vector.begin(), m_vector.end());
}

} // namespace WebCore

#endif

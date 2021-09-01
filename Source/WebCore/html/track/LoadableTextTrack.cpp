/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include "LoadableTextTrack.h"

#if ENABLE(VIDEO)

#include "HTMLTrackElement.h"
#include "TextTrackCueList.h"
#include "VTTCue.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LoadableTextTrack);

LoadableTextTrack::LoadableTextTrack(HTMLTrackElement& track, const String& kind, const String& label, const String& language)
    : TextTrack(&track.document(), &track, kind, emptyString(), label, language, TrackElement)
    , m_trackElement(&track)
{
}

Ref<LoadableTextTrack> LoadableTextTrack::create(HTMLTrackElement& track, const String& kind, const String& label, const String& language)
{
    auto textTrack = adoptRef(*new LoadableTextTrack(track, kind, label, language));
    textTrack->suspendIfNeeded();
    return textTrack;
}

void LoadableTextTrack::scheduleLoad(const URL& url)
{
    ASSERT(!url.isEmpty());

    if (url == m_url)
        return;

    // When src attribute is changed we need to flush all collected track data
    removeAllCues();

    if (!m_trackElement)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks (continued)

    // 2. Let URL be the track URL of the track element.
    m_url = url;
    
    if (m_loadPending)
        return;
    
    // 3. Asynchronously run the remaining steps, while continuing with whatever task
    // was responsible for creating the text track or changing the text track mode.
    m_trackElement->scheduleTask([this]() mutable {
        SetForScope<bool> loadPending { m_loadPending, true, false };

        if (m_loader)
            m_loader->cancelLoad();

        if (!m_trackElement)
            return;

        // 4.8.10.12.3 Sourcing out-of-band text tracks (continued)

        // 4. Download: If URL is not the empty string, perform a potentially CORS-enabled fetch of URL, with the
        // mode being the state of the media element's crossorigin content attribute, the origin being the
        // origin of the media element's Document, and the default origin behaviour set to fail.
        m_loader = makeUnique<TextTrackLoader>(static_cast<TextTrackLoaderClient&>(*this), m_trackElement->document());
        if (!m_loader->load(m_url, *m_trackElement))
            m_trackElement->didCompleteLoad(HTMLTrackElement::Failure);
    });
}

Element* LoadableTextTrack::element()
{
    return m_trackElement;
}

void LoadableTextTrack::newCuesAvailable(TextTrackLoader& loader)
{
    ASSERT_UNUSED(loader, m_loader.get() == &loader);

    if (!m_cues)
        m_cues = TextTrackCueList::create();    

    for (auto& newCue : m_loader->getNewCues()) {
        newCue->setTrack(this);
        INFO_LOG(LOGIDENTIFIER, newCue.get());
        m_cues->add(WTFMove(newCue));
    }

    if (client())
        client()->textTrackAddCues(*this, *m_cues);
}

void LoadableTextTrack::cueLoadingCompleted(TextTrackLoader& loader, bool loadingFailed)
{
    ASSERT_UNUSED(loader, m_loader.get() == &loader);

    if (!m_trackElement)
        return;

    INFO_LOG(LOGIDENTIFIER);

    m_trackElement->didCompleteLoad(loadingFailed ? HTMLTrackElement::Failure : HTMLTrackElement::Success);
}

void LoadableTextTrack::newRegionsAvailable(TextTrackLoader& loader)
{
    ASSERT_UNUSED(loader, m_loader.get() == &loader);
    for (auto& newRegion : m_loader->getNewRegions()) {
        newRegion->setTrack(this);
        regions()->add(WTFMove(newRegion));
    }
}

void LoadableTextTrack::newStyleSheetsAvailable(TextTrackLoader& loader)
{
    ASSERT_UNUSED(loader, m_loader.get() == &loader);
    m_styleSheets = m_loader->getNewStyleSheets();
}

AtomString LoadableTextTrack::id() const
{
    if (!m_trackElement)
        return emptyAtom();
    return m_trackElement->attributeWithoutSynchronization(idAttr);
}

size_t LoadableTextTrack::trackElementIndex()
{
    ASSERT(m_trackElement);
    ASSERT(m_trackElement->parentNode());

    size_t index = 0;
    for (RefPtr<Node> node = m_trackElement->parentNode()->firstChild(); node; node = node->nextSibling()) {
        if (!node->hasTagName(trackTag) || !node->parentNode())
            continue;
        if (node == m_trackElement)
            return index;
        ++index;
    }
    ASSERT_NOT_REACHED();

    return 0;
}

bool LoadableTextTrack::isDefault() const
{
    return m_trackElement && m_trackElement->hasAttributeWithoutSynchronization(defaultAttr);
}

} // namespace WebCore

#endif

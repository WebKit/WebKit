/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextTrack.h"

#if ENABLE(VIDEO)

#include "CommonAtomStrings.h"
#include "DataCue.h"
#include "Document.h"
#include "Event.h"
#include "SourceBuffer.h"
#include "TextTrackClient.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "VTTRegion.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrack);

static const AtomString& descriptionsKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> descriptions("descriptions"_s);
    return descriptions;
}

static const AtomString& chaptersKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> chapters("chapters"_s);
    return chapters;
}

static const AtomString& metadataKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> metadata("metadata"_s);
    return metadata;
}
    
static const AtomString& forcedKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> forced("forced"_s);
    return forced;
}

TextTrack& TextTrack::captionMenuOffItem()
{
    static TextTrack& off = TextTrack::create(nullptr, "off menu item"_s, emptyAtom(), emptyAtom(), emptyAtom()).leakRef();
    return off;
}

TextTrack& TextTrack::captionMenuAutomaticItem()
{
    static TextTrack& automatic = TextTrack::create(nullptr, "automatic menu item"_s, emptyAtom(), emptyAtom(), emptyAtom()).leakRef();
    return automatic;
}

TextTrack::TextTrack(ScriptExecutionContext* context, const AtomString& kind, const AtomString& id, const AtomString& label, const AtomString& language, TextTrackType type)
    : TrackBase(context, TrackBase::TextTrack, id, label, language)
    , ActiveDOMObject(context)
    , m_trackType(type)
{
    if (kind == captionsAtom())
        m_kind = Kind::Captions;
    else if (kind == chaptersKeyword())
        m_kind = Kind::Chapters;
    else if (kind == descriptionsKeyword())
        m_kind = Kind::Descriptions;
    else if (kind == forcedKeyword())
        m_kind = Kind::Forced;
    else if (kind == metadataKeyword())
        m_kind = Kind::Metadata;
}

Ref<TextTrack> TextTrack::create(Document* document, const AtomString& kind, const AtomString& id, const AtomString& label, const AtomString& language)
{
    auto textTrack = adoptRef(*new TextTrack(document, kind, id, label, language, AddTrack));
    textTrack->suspendIfNeeded();
    return textTrack;
}

TextTrack::~TextTrack()
{
    if (m_cues) {
        m_clients.forEach([this] (auto& client) {
            client.textTrackRemoveCues(*this, *m_cues);
        });
        for (size_t i = 0; i < m_cues->length(); ++i)
            m_cues->item(i)->setTrack(nullptr);
    }
    if (m_regions) {
        for (size_t i = 0; i < m_regions->length(); ++i)
            m_regions->item(i)->setTrack(nullptr);
    }
}

TextTrackList* TextTrack::textTrackList() const
{
    return downcast<TextTrackList>(trackList());
}

void TextTrack::addClient(TextTrackClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
}

void TextTrack::clearClient(TextTrackClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

Document& TextTrack::document() const
{
    ASSERT(scriptExecutionContext());
    return downcast<Document>(*scriptExecutionContext());
}

bool TextTrack::enabled() const
{
    return m_mode != Mode::Disabled;
}

bool TextTrack::isValidKindKeyword(const AtomString& value)
{
    if (value == subtitlesAtom())
        return true;
    if (value == captionsAtom())
        return true;
    if (value == descriptionsKeyword())
        return true;
    if (value == chaptersKeyword())
        return true;
    if (value == metadataKeyword())
        return true;
    if (value == forcedKeyword())
        return true;

    return false;
}

const AtomString& TextTrack::kindKeyword() const
{
    switch (m_kind) {
    case Kind::Captions:
        return captionsAtom();
    case Kind::Chapters:
        return chaptersKeyword();
    case Kind::Descriptions:
        return descriptionsKeyword();
    case Kind::Forced:
        return forcedKeyword();
    case Kind::Metadata:
        return metadataKeyword();
    case Kind::Subtitles:
        return subtitlesAtom();
    }
    ASSERT_NOT_REACHED();
    return subtitlesAtom();
}

void TextTrack::setKind(Kind newKind)
{
    auto oldKind = m_kind;

    // 10.1 kind, on setting:
    // 1. If the value being assigned to this attribute does not match one of the text track kinds,
    // then abort these steps.

    // 2. Update this attribute to the new value.
    m_kind = newKind;

#if ENABLE(MEDIA_SOURCE)
    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.textTracks.
    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
#endif

    if (oldKind != m_kind) {
        ALWAYS_LOG(LOGIDENTIFIER, m_kind);
        m_clients.forEach([this] (auto& client) {
            client.textTrackKindChanged(*this);
        });
    }
}

void TextTrack::setKindKeywordIgnoringASCIICase(StringView keyword)
{
    if (keyword.isNull()) {
        // The missing value default is the subtitles state.
        setKind(Kind::Subtitles);
        return;
    }
    if (equalLettersIgnoringASCIICase(keyword, "captions"_s))
        setKind(Kind::Captions);
    else if (equalLettersIgnoringASCIICase(keyword, "chapters"_s))
        setKind(Kind::Chapters);
    else if (equalLettersIgnoringASCIICase(keyword, "descriptions"_s))
        setKind(Kind::Descriptions);
    else if (equalLettersIgnoringASCIICase(keyword, "forced"_s))
        setKind(Kind::Forced);
    else if (equalLettersIgnoringASCIICase(keyword, "metadata"_s))
        setKind(Kind::Metadata);
    else if (equalLettersIgnoringASCIICase(keyword, "subtitles"_s))
        setKind(Kind::Subtitles);
    else {
        // The invalid value default is the metadata state.
        setKind(Kind::Metadata);
    }
}

void TextTrack::setMode(Mode mode)
{
    // On setting, if the new value isn't equal to what the attribute would currently
    // return, the new value must be processed as follows ...
    if (m_mode == mode)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, mode);

    // If mode changes to disabled, remove this track's cues from the client
    // because they will no longer be accessible from the cues() function.
    if (mode == Mode::Disabled && m_cues) {
        m_clients.forEach([this] (auto& client) {
            client.textTrackRemoveCues(*this, *m_cues);
        });
    }

    if (mode != Mode::Showing && m_cues) {
        for (size_t i = 0; i < m_cues->length(); ++i)
            m_cues->item(i)->removeDisplayTree();
    }

    m_mode = mode;

    m_clients.forEach([this] (auto& client) {
        client.textTrackModeChanged(*this);
    });
}

TextTrackCueList* TextTrack::cues()
{
    // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
    // then the cues attribute must return a live TextTrackCueList object ...
    // Otherwise, it must return null. When an object is returned, the
    // same object must be returned each time.
    // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-cues
    if (m_mode == Mode::Disabled)
        return nullptr;
    return &ensureTextTrackCueList();
}

void TextTrack::removeAllCues()
{
    if (!m_cues)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_clients.forEach([this] (auto& client) {
        client.textTrackRemoveCues(*this, *m_cues);
    });

    for (size_t i = 0; i < m_cues->length(); ++i)
        m_cues->item(i)->setTrack(nullptr);
    
    m_cues->clear();
}

TextTrackCueList* TextTrack::activeCues() const
{
    // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
    // then the activeCues attribute must return a live TextTrackCueList object ...
    // ... whose active flag was set when the script started, in text track cue
    // order. Otherwise, it must return null. When an object is returned, the
    // same object must be returned each time.
    // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-activecues
    if (!m_cues || m_mode == Mode::Disabled)
        return nullptr;
    return &m_cues->activeCues();
}

ExceptionOr<void> TextTrack::addCue(Ref<TextTrackCue>&& cue)
{
    // 4.7.10.12.6 Text tracks exposing in-band metadata
    // The UA will use DataCue to expose only text track cue objects that belong to a text track that has a text
    // track kind of metadata.
    // If a DataCue is added to a TextTrack via the addCue() method but the text track does not have its text
    // track kind set to metadata, throw a InvalidNodeTypeError exception and don't add the cue to the TextTrackList
    // of the TextTrack.
    if (is<DataCue>(cue) && m_kind != Kind::Metadata)
        return Exception { InvalidNodeTypeError };

    INFO_LOG(LOGIDENTIFIER, cue.get());

    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (!cue->startMediaTime().isValid() || !cue->endMediaTime().isValid() || cue->startMediaTime() < MediaTime::zeroTime() || cue->endMediaTime() < MediaTime::zeroTime())
        return { };

    // 4.8.10.12.5 Text track API

    // The addCue(cue) method of TextTrack objects, when invoked, must run the following steps:

    RefPtr cueTrack = cue->track();
    if (cueTrack == this)
        return { };

    // 1. If the given cue is in a text track list of cues, then remove cue from that text track
    // list of cues.
    if (cueTrack)
        cueTrack->removeCue(cue.get());

    // 2. Add cue to the method's TextTrack object's text track's text track list of cues.
    cue->setTrack(this);
    ensureTextTrackCueList().add(cue.copyRef());

    m_clients.forEach([this, cue] (auto& client) {
        client.textTrackAddCue(*this, cue);
    });

    return { };
}

ExceptionOr<void> TextTrack::removeCue(TextTrackCue& cue)
{
    // 4.8.10.12.5 Text track API

    // The removeCue(cue) method of TextTrack objects, when invoked, must run the following steps:

    // 1. If the given cue is not currently listed in the method's TextTrack 
    // object's text track's text track list of cues, then throw a NotFoundError exception.
    if (cue.track() != this)
        return Exception { NotFoundError };
    if (!m_cues)
        return Exception { InvalidStateError };

    INFO_LOG(LOGIDENTIFIER, cue);

    // 2. Remove cue from the method's TextTrack object's text track's text track list of cues.
    m_cues->remove(cue);
    cue.setIsActive(false);
    cue.setTrack(nullptr);
    m_clients.forEach([&] (auto& client) {
        client.textTrackRemoveCue(*this, cue);
    });

    return { };
}

void TextTrack::removeCuesNotInTimeRanges(PlatformTimeRanges& buffered)
{
    ASSERT(shouldPurgeCuesFromUnbufferedRanges());

    if (!m_cues)
        return;

    Vector<Ref<TextTrackCue>> toPurge;
    for (size_t i = 0; i < m_cues->length(); ++i) {
        auto cue = m_cues->item(i);
        ASSERT(cue->track() == this);

        PlatformTimeRanges activeCueRange { cue->startMediaTime(), cue->endMediaTime() };
        activeCueRange.intersectWith(buffered);
        if (!activeCueRange.length())
            toPurge.append(*cue);
    }

    if (!toPurge.size())
        return;

    INFO_LOG(LOGIDENTIFIER, "purging ", toPurge.size());

    for (auto& cue : toPurge)
        removeCue(cue);
}

VTTRegionList& TextTrack::ensureVTTRegionList()
{
    if (!m_regions)
        m_regions = VTTRegionList::create();

    return *m_regions;
}

VTTRegionList* TextTrack::regions()
{
    // If the text track mode of the text track that the TextTrack object
    // represents is not the text track disabled mode, then the regions
    // attribute must return a live VTTRegionList object that represents
    // the text track list of regions of the text track. Otherwise, it must
    // return null. When an object is returned, the same object must be returned
    // each time.
    if (m_mode == Mode::Disabled)
        return nullptr;
    return &ensureVTTRegionList();
}

void TextTrack::addRegion(Ref<VTTRegion>&& region)
{
    auto& regionList = ensureVTTRegionList();

    // 1. If the given region is in a text track list of regions, then remove
    // region from that text track list of regions.
    RefPtr regionTrack = region->track();
    if (regionTrack && regionTrack != this)
        regionTrack->removeRegion(region.get());

    // 2. If the method's TextTrack object's text track list of regions contains
    // a region with the same identifier as region replace the values of that
    // region's width, height, anchor point, viewport anchor point and scroll
    // attributes with those of region.
    RefPtr existingRegion = regionList.getRegionById(region->id());
    if (existingRegion) {
        existingRegion->updateParametersFromRegion(region);
        return;
    }

    // Otherwise: add region to the method's TextTrack object's text track list of regions.
    region->setTrack(this);
    regionList.add(WTFMove(region));
}

ExceptionOr<void> TextTrack::removeRegion(VTTRegion& region)
{
    // 1. If the given region is not currently listed in the method's TextTrack
    // object's text track list of regions, then throw a NotFoundError exception.
    if (region.track() != this)
        return Exception { NotFoundError };

    ASSERT(m_regions);
    m_regions->remove(region);
    region.setTrack(nullptr);
    return { };
}

void TextTrack::cueWillChange(TextTrackCue& cue)
{
    m_clients.forEach([&] (auto& client) {
        // The cue may need to be repositioned in the media element's interval tree, may need to
        // be re-rendered, etc, so remove it before the modification...
        client.textTrackRemoveCue(*this, cue);
    });
}

void TextTrack::cueDidChange(TextTrackCue& cue)
{
    // Make sure the TextTrackCueList order is up-to-date.
    ensureTextTrackCueList().updateCueIndex(cue);

    // ... and add it back again.
    m_clients.forEach([&] (auto& client) {
        client.textTrackAddCue(*this, cue);
    });
}

int TextTrack::trackIndex()
{
    if (!m_trackIndex) {
        if (!textTrackList())
            return 0;

        m_trackIndex = textTrackList()->getTrackIndex(*this);
    }
    return m_trackIndex.value();
}

void TextTrack::invalidateTrackIndex()
{
    m_trackIndex = std::nullopt;
    m_renderedTrackIndex = std::nullopt;
}

bool TextTrack::isRendered()
{
    return (m_kind == Kind::Captions || m_kind == Kind::Subtitles || m_kind == Kind::Forced || m_kind == Kind::Descriptions)
        && m_mode == Mode::Showing;
}

bool TextTrack::isSpoken()
{
    return m_kind == Kind::Descriptions && m_mode == Mode::Showing;
}

TextTrackCueList& TextTrack::ensureTextTrackCueList()
{
    if (!m_cues)
        m_cues = TextTrackCueList::create();
    return *m_cues;
}

int TextTrack::trackIndexRelativeToRenderedTracks()
{
    if (!m_renderedTrackIndex) {
        if (!textTrackList())
            return 0;

        m_renderedTrackIndex = textTrackList()->getTrackIndexRelativeToRenderedTracks(*this);
    }
    return m_renderedTrackIndex.value();
}

RefPtr<TextTrackCue> TextTrack::matchCue(TextTrackCue& cue, TextTrackCue::CueMatchRules match)
{
    if (cue.startMediaTime() < MediaTime::zeroTime() || cue.endMediaTime() < MediaTime::zeroTime())
        return nullptr;
    
    if (!m_cues || !m_cues->length())
        return nullptr;
    
    size_t searchStart = 0;
    size_t searchEnd = m_cues->length();
    
    while (1) {
        ASSERT(searchStart <= m_cues->length());
        ASSERT(searchEnd <= m_cues->length());
        
        RefPtr<TextTrackCue> existingCue;
        
        // Cues in the TextTrackCueList are maintained in start time order.
        if (searchStart == searchEnd) {
            if (!searchStart)
                return nullptr;

            // If there is more than one cue with the same start time, back up to first one so we
            // consider all of them.
            while (searchStart >= 2 && cue.hasEquivalentStartTime(*m_cues->item(searchStart - 2)))
                --searchStart;
            
            bool firstCompare = true;
            while (1) {
                if (!firstCompare)
                    ++searchStart;
                firstCompare = false;
                if (searchStart > m_cues->length())
                    return nullptr;

                existingCue = m_cues->item(searchStart - 1);
                if (!existingCue)
                    return nullptr;

                if (cue.startMediaTime() > (existingCue->startMediaTime() + startTimeVariance()))
                    return nullptr;

                if (existingCue->isEqual(cue, match))
                    return existingCue;
            }
        }
        
        size_t index = (searchStart + searchEnd) / 2;
        existingCue = m_cues->item(index);
        if ((cue.startMediaTime() + startTimeVariance()) < existingCue->startMediaTime() || (match != TextTrackCue::IgnoreDuration && cue.hasEquivalentStartTime(*existingCue) && cue.endMediaTime() > existingCue->endMediaTime()))
            searchEnd = index;
        else
            searchStart = index + 1;
    }
    
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool TextTrack::isMainProgramContent() const
{
    // "Main program" content is intrinsic to the presentation of the media file, regardless of locale. Content such as
    // directors commentary is not "main program" because it is not essential for the presentation. HTML5 doesn't have
    // a way to express this in a machine-reable form, it is typically done with the track label, so we assume that caption
    // tracks are main content and all other track types are not.
    return m_kind == Kind::Captions;
}

bool TextTrack::containsOnlyForcedSubtitles() const
{
    return m_kind == Kind::Forced;
}

const char* TextTrack::activeDOMObjectName() const
{
    return "TextTrack";
}

void TextTrack::setLanguage(const AtomString& language)
{
    // 11.1 language, on setting:
    // 1. If the value being assigned to this attribute is not an empty string or a BCP 47 language
    // tag[BCP47], then abort these steps.
    // BCP 47 validation is done in TrackBase::setLanguage() which is
    // shared between all tracks that support setting language.

    // 2. Update this attribute to the new value.
    TrackBase::setLanguage(language);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.textTracks.
    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
    m_clients.forEach([&] (auto& client) {
        client.textTrackLanguageChanged(*this);
    });
}

void TextTrack::setId(const AtomString& id)
{
    TrackBase::setId(id);
    m_clients.forEach([this] (auto& client) {
        client.textTrackIdChanged(*this);
    });
}

void TextTrack::setLabel(const AtomString& label)
{
    TrackBase::setLabel(label);
    m_clients.forEach([this] (auto& client) {
        client.textTrackLabelChanged(*this);
    });
}

void TextTrack::newCuesAvailable(const TextTrackCueList& list)
{
    m_clients.forEach([&] (auto& client) {
        client.textTrackAddCues(*this, list);
    });
}

} // namespace WebCore

#endif

/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "Document.h"
#include "Event.h"
#include "HTMLMediaElement.h"
#include "SourceBuffer.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "VTTRegion.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrack);

const AtomString& TextTrack::subtitlesKeyword()
{
    static NeverDestroyed<const AtomString> subtitles("subtitles", AtomString::ConstructFromLiteral);
    return subtitles;
}

static const AtomString& captionsKeyword()
{
    static NeverDestroyed<const AtomString> captions("captions", AtomString::ConstructFromLiteral);
    return captions;
}

static const AtomString& descriptionsKeyword()
{
    static NeverDestroyed<const AtomString> descriptions("descriptions", AtomString::ConstructFromLiteral);
    return descriptions;
}

static const AtomString& chaptersKeyword()
{
    static NeverDestroyed<const AtomString> chapters("chapters", AtomString::ConstructFromLiteral);
    return chapters;
}

static const AtomString& metadataKeyword()
{
    static NeverDestroyed<const AtomString> metadata("metadata", AtomString::ConstructFromLiteral);
    return metadata;
}
    
static const AtomString& forcedKeyword()
{
    static NeverDestroyed<const AtomString> forced("forced", AtomString::ConstructFromLiteral);
    return forced;
}

TextTrack* TextTrack::captionMenuOffItem()
{
    static TextTrack& off = TextTrack::create(nullptr, nullptr, "off menu item", emptyAtom(), emptyAtom(), emptyAtom()).leakRef();
    return &off;
}

TextTrack* TextTrack::captionMenuAutomaticItem()
{
    static TextTrack& automatic = TextTrack::create(nullptr, nullptr, "automatic menu item", emptyAtom(), emptyAtom(), emptyAtom()).leakRef();
    return &automatic;
}

TextTrack::TextTrack(ScriptExecutionContext* context, TextTrackClient* client, const AtomString& kind, const AtomString& id, const AtomString& label, const AtomString& language, TextTrackType type)
    : TrackBase(TrackBase::TextTrack, id, label, language)
    , ContextDestructionObserver(context)
    , m_client(client)
    , m_trackType(type)
{
    if (kind == captionsKeyword())
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

TextTrack::~TextTrack()
{
    if (m_cues) {
        if (m_client)
            m_client->textTrackRemoveCues(*this, *m_cues);
        for (size_t i = 0; i < m_cues->length(); ++i)
            m_cues->item(i)->setTrack(nullptr);
    }
    if (m_regions) {
        for (size_t i = 0; i < m_regions->length(); ++i)
            m_regions->item(i)->setTrack(nullptr);
    }
}

bool TextTrack::enabled() const
{
    return m_mode != Mode::Disabled;
}

bool TextTrack::isValidKindKeyword(const AtomString& value)
{
    if (value == subtitlesKeyword())
        return true;
    if (value == captionsKeyword())
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
        return captionsKeyword();
    case Kind::Chapters:
        return chaptersKeyword();
    case Kind::Descriptions:
        return descriptionsKeyword();
    case Kind::Forced:
        return forcedKeyword();
    case Kind::Metadata:
        return metadataKeyword();
    case Kind::Subtitles:
        return subtitlesKeyword();
    }
    ASSERT_NOT_REACHED();
    return subtitlesKeyword();
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
    if (m_sourceBuffer)
        m_sourceBuffer->textTracks().scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
    if (mediaElement())
        mediaElement()->ensureTextTracks().scheduleChangeEvent();
#endif

    if (m_client && oldKind != m_kind)
        m_client->textTrackKindChanged(*this);
}

void TextTrack::setKindKeywordIgnoringASCIICase(StringView keyword)
{
    if (keyword.isNull()) {
        // The missing value default is the subtitles state.
        setKind(Kind::Subtitles);
        return;
    }
    if (equalLettersIgnoringASCIICase(keyword, "captions"))
        setKind(Kind::Captions);
    else if (equalLettersIgnoringASCIICase(keyword, "chapters"))
        setKind(Kind::Chapters);
    else if (equalLettersIgnoringASCIICase(keyword, "descriptions"))
        setKind(Kind::Descriptions);
    else if (equalLettersIgnoringASCIICase(keyword, "forced"))
        setKind(Kind::Forced);
    else if (equalLettersIgnoringASCIICase(keyword, "metadata"))
        setKind(Kind::Metadata);
    else if (equalLettersIgnoringASCIICase(keyword, "subtitles"))
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

    // If mode changes to disabled, remove this track's cues from the client
    // because they will no longer be accessible from the cues() function.
    if (mode == Mode::Disabled && m_client && m_cues)
        m_client->textTrackRemoveCues(*this, *m_cues);

    if (mode != Mode::Showing && m_cues) {
        for (size_t i = 0; i < m_cues->length(); ++i) {
            RefPtr<TextTrackCue> cue = m_cues->item(i);
            if (cue->isRenderable())
                toVTTCue(cue.get())->removeDisplayTree();
        }
    }

    m_mode = mode;

    if (m_client)
        m_client->textTrackModeChanged(*this);
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

    if (m_client)
        m_client->textTrackRemoveCues(*this, *m_cues);
    
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
    if (cue->cueType() == TextTrackCue::Data && m_kind != Kind::Metadata)
        return Exception { InvalidNodeTypeError };

    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (!cue->startMediaTime().isValid() || !cue->endMediaTime().isValid() || cue->startMediaTime() < MediaTime::zeroTime() || cue->endMediaTime() < MediaTime::zeroTime())
        return { };

    // 4.8.10.12.5 Text track API

    // The addCue(cue) method of TextTrack objects, when invoked, must run the following steps:

    auto cueTrack = makeRefPtr(cue->track());
    if (cueTrack == this)
        return { };

    // 1. If the given cue is in a text track list of cues, then remove cue from that text track
    // list of cues.
    if (cueTrack)
        cueTrack->removeCue(cue.get());

    // 2. Add cue to the method's TextTrack object's text track's text track list of cues.
    cue->setTrack(this);
    ensureTextTrackCueList().add(cue.copyRef());

    if (m_client)
        m_client->textTrackAddCue(*this, cue);

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
    if (m_client)
        m_client->textTrackRemoveCue(*this, cue);

    return { };
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

void TextTrack::addRegion(RefPtr<VTTRegion>&& region)
{
    if (!region)
        return;

    auto& regionList = ensureVTTRegionList();

    // 1. If the given region is in a text track list of regions, then remove
    // region from that text track list of regions.
    auto regionTrack = makeRefPtr(region->track());
    if (regionTrack && regionTrack != this)
        regionTrack->removeRegion(region.get());

    // 2. If the method's TextTrack object's text track list of regions contains
    // a region with the same identifier as region replace the values of that
    // region's width, height, anchor point, viewport anchor point and scroll
    // attributes with those of region.
    auto existingRegion = makeRefPtr(regionList.getRegionById(region->id()));
    if (existingRegion) {
        existingRegion->updateParametersFromRegion(*region);
        return;
    }

    // Otherwise: add region to the method's TextTrack object's text track list of regions.
    region->setTrack(this);
    regionList.add(region.releaseNonNull());
}

ExceptionOr<void> TextTrack::removeRegion(VTTRegion* region)
{
    if (!region)
        return { };

    // 1. If the given region is not currently listed in the method's TextTrack
    // object's text track list of regions, then throw a NotFoundError exception.
    if (region->track() != this)
        return Exception { NotFoundError };

    ASSERT(m_regions);
    m_regions->remove(*region);
    region->setTrack(nullptr);
    return { };
}

void TextTrack::cueWillChange(TextTrackCue* cue)
{
    if (!m_client)
        return;

    // The cue may need to be repositioned in the media element's interval tree, may need to
    // be re-rendered, etc, so remove it before the modification...
    m_client->textTrackRemoveCue(*this, *cue);
}

void TextTrack::cueDidChange(TextTrackCue* cue)
{
    if (!m_client)
        return;

    // Make sure the TextTrackCueList order is up-to-date.
    ensureTextTrackCueList().updateCueIndex(*cue);

    // ... and add it back again.
    m_client->textTrackAddCue(*this, *cue);
}

int TextTrack::trackIndex()
{
    ASSERT(m_mediaElement);
    if (!m_trackIndex)
        m_trackIndex = m_mediaElement->ensureTextTracks().getTrackIndex(*this);
    return m_trackIndex.value();
}

void TextTrack::invalidateTrackIndex()
{
    m_trackIndex = WTF::nullopt;
    m_renderedTrackIndex = WTF::nullopt;
}

bool TextTrack::isRendered()
{
    return (m_kind == Kind::Captions || m_kind == Kind::Subtitles || m_kind == Kind::Forced)
        && m_mode == Mode::Showing;
}

TextTrackCueList& TextTrack::ensureTextTrackCueList()
{
    if (!m_cues)
        m_cues = TextTrackCueList::create();
    return *m_cues;
}

int TextTrack::trackIndexRelativeToRenderedTracks()
{
    ASSERT(m_mediaElement);
    if (!m_renderedTrackIndex)
        m_renderedTrackIndex = m_mediaElement->ensureTextTracks().getTrackIndexRelativeToRenderedTracks(*this);
    return m_renderedTrackIndex.value();
}

bool TextTrack::hasCue(TextTrackCue* cue, TextTrackCue::CueMatchRules match)
{
    if (cue->startMediaTime() < MediaTime::zeroTime() || cue->endMediaTime() < MediaTime::zeroTime())
        return false;
    
    if (!m_cues || !m_cues->length())
        return false;
    
    size_t searchStart = 0;
    size_t searchEnd = m_cues->length();
    
    while (1) {
        ASSERT(searchStart <= m_cues->length());
        ASSERT(searchEnd <= m_cues->length());
        
        RefPtr<TextTrackCue> existingCue;
        
        // Cues in the TextTrackCueList are maintained in start time order.
        if (searchStart == searchEnd) {
            if (!searchStart)
                return false;

            // If there is more than one cue with the same start time, back up to first one so we
            // consider all of them.
            while (searchStart >= 2 && cue->hasEquivalentStartTime(*m_cues->item(searchStart - 2)))
                --searchStart;
            
            bool firstCompare = true;
            while (1) {
                if (!firstCompare)
                    ++searchStart;
                firstCompare = false;
                if (searchStart > m_cues->length())
                    return false;

                existingCue = m_cues->item(searchStart - 1);
                if (!existingCue)
                    return false;

                if (cue->startMediaTime() > (existingCue->startMediaTime() + startTimeVariance()))
                    return false;

                if (existingCue->isEqual(*cue, match))
                    return true;
            }
        }
        
        size_t index = (searchStart + searchEnd) / 2;
        existingCue = m_cues->item(index);
        if ((cue->startMediaTime() + startTimeVariance()) < existingCue->startMediaTime() || (match != TextTrackCue::IgnoreDuration && cue->hasEquivalentStartTime(*existingCue) && cue->endMediaTime() > existingCue->endMediaTime()))
            searchEnd = index;
        else
            searchStart = index + 1;
    }
    
    ASSERT_NOT_REACHED();
    return false;
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

#if ENABLE(MEDIA_SOURCE)
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
    if (m_sourceBuffer)
        m_sourceBuffer->textTracks().scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
    if (mediaElement())
        mediaElement()->ensureTextTracks().scheduleChangeEvent();
}
#endif

} // namespace WebCore

#endif

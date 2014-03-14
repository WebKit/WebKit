/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011-2014 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "TextTrack.h"

#include "Event.h"
#include "HTMLMediaElement.h"
#include "SourceBuffer.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#include "TextTrackRegionList.h"

namespace WebCore {

static const int invalidTrackIndex = -1;

const AtomicString& TextTrack::subtitlesKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, subtitles, ("subtitles", AtomicString::ConstructFromLiteral));
    return subtitles;
}

const AtomicString& TextTrack::captionsKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, captions, ("captions", AtomicString::ConstructFromLiteral));
    return captions;
}

const AtomicString& TextTrack::descriptionsKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, descriptions, ("descriptions", AtomicString::ConstructFromLiteral));
    return descriptions;
}

const AtomicString& TextTrack::chaptersKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, chapters, ("chapters", AtomicString::ConstructFromLiteral));
    return chapters;
}

const AtomicString& TextTrack::metadataKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, metadata, ("metadata", AtomicString::ConstructFromLiteral));
    return metadata;
}
    
const AtomicString& TextTrack::forcedKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, forced, ("forced", AtomicString::ConstructFromLiteral));
    return forced;
}

const AtomicString& TextTrack::disabledKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, open, ("disabled", AtomicString::ConstructFromLiteral));
    return open;
}

const AtomicString& TextTrack::hiddenKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, closed, ("hidden", AtomicString::ConstructFromLiteral));
    return closed;
}

const AtomicString& TextTrack::showingKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, ended, ("showing", AtomicString::ConstructFromLiteral));
    return ended;
}

TextTrack* TextTrack::captionMenuOffItem()
{
    static TextTrack* off = TextTrack::create(0, 0, "off menu item", "", "", "").leakRef();
    return off;
}

TextTrack* TextTrack::captionMenuAutomaticItem()
{
    static TextTrack* automatic = TextTrack::create(0, 0, "automatic menu item", "", "", "").leakRef();
    return automatic;
}

TextTrack::TextTrack(ScriptExecutionContext* context, TextTrackClient* client, const AtomicString& kind, const AtomicString& id, const AtomicString& label, const AtomicString& language, TextTrackType type)
    : TrackBase(TrackBase::TextTrack, id, label, language)
    , m_cues(0)
#if ENABLE(WEBVTT_REGIONS)
    , m_regions(0)
#endif
    , m_scriptExecutionContext(context)
    , m_mode(disabledKeyword().string())
    , m_client(client)
    , m_trackType(type)
    , m_readinessState(NotLoaded)
    , m_trackIndex(invalidTrackIndex)
    , m_renderedTrackIndex(invalidTrackIndex)
    , m_hasBeenConfigured(false)
{
    setKindInternal(kind);
}

TextTrack::~TextTrack()
{
    if (m_cues) {
        if (m_client)
            m_client->textTrackRemoveCues(this, m_cues.get());

        for (size_t i = 0; i < m_cues->length(); ++i)
            m_cues->item(i)->setTrack(0);
#if ENABLE(WEBVTT_REGIONS)
        for (size_t i = 0; i < m_regions->length(); ++i)
            m_regions->item(i)->setTrack(0);
#endif
    }
    clearClient();
}

bool TextTrack::isValidKind(const AtomicString& value) const
{
    return TextTrack::isValidKindKeyword(value);
}

bool TextTrack::enabled() const
{
    return m_mode != disabledKeyword();
}

bool TextTrack::isValidKindKeyword(const AtomicString& value)
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

void TextTrack::setKind(const AtomicString& newKind)
{
    String oldKind = kind();

#if ENABLE(MEDIA_SOURCE)
    // 10.1 kind, on setting:
    // 1. If the value being assigned to this attribute does not match one of the text track kinds,
    // then abort these steps.
    if (!isValidKindKeyword(newKind))
        return;

    // 2. Update this attribute to the new value.
    setKindInternal(newKind);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.textTracks.
    if (m_sourceBuffer)
        m_sourceBuffer->textTracks()->scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
    if (mediaElement())
        mediaElement()->textTracks()->scheduleChangeEvent();
#else
    TrackBase::setKind(newKind);
#endif

    if (m_client && oldKind != kind())
        m_client->textTrackKindChanged(this);
}

void TextTrack::setMode(const AtomicString& mode)
{
    // On setting, if the new value isn't equal to what the attribute would currently
    // return, the new value must be processed as follows ...
    if (mode != disabledKeyword() && mode != hiddenKeyword() && mode != showingKeyword())
        return;

    if (m_mode == mode)
        return;

    // If mode changes to disabled, remove this track's cues from the client
    // because they will no longer be accessible from the cues() function.
    if (mode == disabledKeyword() && m_client && m_cues)
        m_client->textTrackRemoveCues(this, m_cues.get());
         
    if (mode != showingKeyword() && m_cues) {
        for (size_t i = 0; i < m_cues->length(); ++i) {
            TextTrackCue* cue = m_cues->item(i);
            if (cue->isRenderable())
                toVTTCue(cue)->removeDisplayTree();
        }
    }

    m_mode = mode;

    if (m_client)
        m_client->textTrackModeChanged(this);
}

TextTrackCueList* TextTrack::cues()
{
    // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
    // then the cues attribute must return a live TextTrackCueList object ...
    // Otherwise, it must return null. When an object is returned, the
    // same object must be returned each time.
    // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-cues
    if (m_mode != disabledKeyword())
        return ensureTextTrackCueList();
    return 0;
}

void TextTrack::removeAllCues()
{
    if (!m_cues)
        return;

    if (m_client)
        m_client->textTrackRemoveCues(this, m_cues.get());
    
    for (size_t i = 0; i < m_cues->length(); ++i)
        m_cues->item(i)->setTrack(0);
    
    m_cues = 0;
}

TextTrackCueList* TextTrack::activeCues() const
{
    // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
    // then the activeCues attribute must return a live TextTrackCueList object ...
    // ... whose active flag was set when the script started, in text track cue
    // order. Otherwise, it must return null. When an object is returned, the
    // same object must be returned each time.
    // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-activecues
    if (m_cues && m_mode != disabledKeyword())
        return m_cues->activeCues();
    return 0;
}

void TextTrack::addCue(PassRefPtr<TextTrackCue> prpCue, ExceptionCode& ec)
{
    if (!prpCue)
        return;

    RefPtr<TextTrackCue> cue = prpCue;

    // 4.7.10.12.6 Text tracks exposing in-band metadata
    // The UA will use DataCue to expose only text track cue objects that belong to a text track that has a text
    // track kind of metadata.
    // If a DataCue is added to a TextTrack via the addCue() method but the text track does not have its text
    // track kind set to metadata, throw a InvalidNodeTypeError exception and don't add the cue to the TextTrackList
    // of the TextTrack.
    if (cue->cueType() == TextTrackCue::Data && kind() != metadataKeyword()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (std::isnan(cue->startTime()) || std::isnan(cue->endTime()) || cue->startTime() < 0 || cue->endTime() < 0)
        return;

    // 4.8.10.12.5 Text track API

    // The addCue(cue) method of TextTrack objects, when invoked, must run the following steps:

    // 1. If the given cue is in a text track list of cues, then remove cue from that text track
    // list of cues.
    TextTrack* cueTrack = cue->track();
    if (cueTrack && cueTrack != this)
        cueTrack->removeCue(cue.get(), ASSERT_NO_EXCEPTION);

    // 2. Add cue to the method's TextTrack object's text track's text track list of cues.
    cue->setTrack(this);
    ensureTextTrackCueList()->add(cue);
    
    if (m_client)
        m_client->textTrackAddCue(this, cue.get());
}

void TextTrack::removeCue(TextTrackCue* cue, ExceptionCode& ec)
{
    if (!cue)
        return;

    // 4.8.10.12.5 Text track API

    // The removeCue(cue) method of TextTrack objects, when invoked, must run the following steps:

    // 1. If the given cue is not currently listed in the method's TextTrack 
    // object's text track's text track list of cues, then throw a NotFoundError exception.
    if (cue->track() != this) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // 2. Remove cue from the method's TextTrack object's text track's text track list of cues.
    if (!m_cues || !m_cues->remove(cue)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    cue->setTrack(0);
    if (m_client)
        m_client->textTrackRemoveCue(this, cue);
}

#if ENABLE(VIDEO_TRACK) && ENABLE(WEBVTT_REGIONS)
TextTrackRegionList* TextTrack::regionList()
{
    return ensureTextTrackRegionList();
}

TextTrackRegionList* TextTrack::ensureTextTrackRegionList()
{
    if (!m_regions)
        m_regions = TextTrackRegionList::create();

    return m_regions.get();
}

TextTrackRegionList* TextTrack::regions()
{
    // If the text track mode of the text track that the TextTrack object
    // represents is not the text track disabled mode, then the regions
    // attribute must return a live TextTrackRegionList object that represents
    // the text track list of regions of the text track. Otherwise, it must
    // return null. When an object is returned, the same object must be returned
    // each time.
    if (m_mode != disabledKeyword())
        return ensureTextTrackRegionList();

    return 0;
}

void TextTrack::addRegion(PassRefPtr<TextTrackRegion> prpRegion)
{
    if (!prpRegion)
        return;

    RefPtr<TextTrackRegion> region = prpRegion;
    TextTrackRegionList* regionList = ensureTextTrackRegionList();

    // 1. If the given region is in a text track list of regions, then remove
    // region from that text track list of regions.
    TextTrack* regionTrack = region->track();
    if (regionTrack && regionTrack != this)
        regionTrack->removeRegion(region.get(), ASSERT_NO_EXCEPTION);

    // 2. If the method's TextTrack object's text track list of regions contains
    // a region with the same identifier as region replace the values of that
    // region's width, height, anchor point, viewport anchor point and scroll
    // attributes with those of region.
    TextTrackRegion* existingRegion = regionList->getRegionById(region->id());
    if (existingRegion) {
        existingRegion->updateParametersFromRegion(region.get());
        return;
    }

    // Otherwise: add region to the method's TextTrack object's text track
    // list of regions.
    region->setTrack(this);
    regionList->add(region);
}

void TextTrack::removeRegion(TextTrackRegion* region, ExceptionCode &ec)
{
    if (!region)
        return;

    // 1. If the given region is not currently listed in the method's TextTrack
    // object's text track list of regions, then throw a NotFoundError exception.
    if (region->track() != this) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (!m_regions || !m_regions->remove(region)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    region->setTrack(0);
}
#endif

void TextTrack::cueWillChange(TextTrackCue* cue)
{
    if (!m_client)
        return;

    // The cue may need to be repositioned in the media element's interval tree, may need to
    // be re-rendered, etc, so remove it before the modification...
    m_client->textTrackRemoveCue(this, cue);
}

void TextTrack::cueDidChange(TextTrackCue* cue)
{
    if (!m_client)
        return;

    // Make sure the TextTrackCueList order is up-to-date.
    ensureTextTrackCueList()->updateCueIndex(cue);

    // ... and add it back again.
    m_client->textTrackAddCue(this, cue);
}

int TextTrack::trackIndex()
{
    ASSERT(m_mediaElement);

    if (m_trackIndex == invalidTrackIndex)
        m_trackIndex = m_mediaElement->textTracks()->getTrackIndex(this);

    return m_trackIndex;
}

void TextTrack::invalidateTrackIndex()
{
    m_trackIndex = invalidTrackIndex;
    m_renderedTrackIndex = invalidTrackIndex;
}

bool TextTrack::isRendered()
{
    if (kind() != captionsKeyword() && kind() != subtitlesKeyword() && kind() != forcedKeyword())
        return false;

    if (m_mode != showingKeyword())
        return false;

    return true;
}

TextTrackCueList* TextTrack::ensureTextTrackCueList()
{
    if (!m_cues)
        m_cues = TextTrackCueList::create();

    return m_cues.get();
}

int TextTrack::trackIndexRelativeToRenderedTracks()
{
    ASSERT(m_mediaElement);
    
    if (m_renderedTrackIndex == invalidTrackIndex)
        m_renderedTrackIndex = m_mediaElement->textTracks()->getTrackIndexRelativeToRenderedTracks(this);
    
    return m_renderedTrackIndex;
}

bool TextTrack::hasCue(VTTCue* cue, VTTCue::CueMatchRules match)
{
    if (cue->startTime() < 0 || cue->endTime() < 0)
        return false;
    
    if (!m_cues || !m_cues->length())
        return false;
    
    size_t searchStart = 0;
    size_t searchEnd = m_cues->length();
    
    while (1) {
        ASSERT(searchStart <= m_cues->length());
        ASSERT(searchEnd <= m_cues->length());
        
        TextTrackCue* existingCue;
        
        // Cues in the TextTrackCueList are maintained in start time order.
        if (searchStart == searchEnd) {
            if (!searchStart)
                return false;

            // If there is more than one cue with the same start time, back up to first one so we
            // consider all of them.
            while (searchStart >= 2 && cue->startTime() == m_cues->item(searchStart - 2)->startTime())
                --searchStart;
            
            bool firstCompare = true;
            while (1) {
                if (!firstCompare)
                    ++searchStart;
                firstCompare = false;
                if (searchStart > m_cues->length())
                    return false;

                existingCue = m_cues->item(searchStart - 1);
                if (!cue->isRenderable())
                    continue;

                if (!existingCue || cue->startTime() > existingCue->startTime())
                    return false;

                if (!toVTTCue(existingCue)->isEqual(*cue, match))
                    continue;
                
                return true;
            }
        }
        
        size_t index = (searchStart + searchEnd) / 2;
        existingCue = m_cues->item(index);
        if (cue->startTime() < existingCue->startTime() || (match != VTTCue::IgnoreDuration && cue->startTime() == existingCue->startTime() && cue->endTime() > existingCue->endTime()))
            searchEnd = index;
        else
            searchStart = index + 1;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

#if USE(PLATFORM_TEXT_TRACK_MENU)
PassRefPtr<PlatformTextTrack> TextTrack::platformTextTrack()
{
    if (m_platformTextTrack)
        return m_platformTextTrack;

    PlatformTextTrack::TrackKind platformKind = PlatformTextTrack::Caption;
    if (kind() == subtitlesKeyword())
        platformKind = PlatformTextTrack::Subtitle;
    else if (kind() == captionsKeyword())
        platformKind = PlatformTextTrack::Caption;
    else if (kind() == descriptionsKeyword())
        platformKind = PlatformTextTrack::Description;
    else if (kind() == chaptersKeyword())
        platformKind = PlatformTextTrack::Chapter;
    else if (kind() == metadataKeyword())
        platformKind = PlatformTextTrack::MetaData;
    else if (kind() == forcedKeyword())
        platformKind = PlatformTextTrack::Forced;

    PlatformTextTrack::TrackType type = PlatformTextTrack::OutOfBand;
    if (m_trackType == TrackElement)
        type = PlatformTextTrack::OutOfBand;
    else if (m_trackType == AddTrack)
        type = PlatformTextTrack::Script;
    else if (m_trackType == InBand)
        type = PlatformTextTrack::InBand;

    PlatformTextTrack::TrackMode platformMode = PlatformTextTrack::Disabled;
    if (TextTrack::hiddenKeyword() == mode())
        platformMode = PlatformTextTrack::Hidden;
    else if (TextTrack::disabledKeyword() == mode())
        platformMode = PlatformTextTrack::Disabled;
    else if (TextTrack::showingKeyword() == mode())
        platformMode = PlatformTextTrack::Showing;

    m_platformTextTrack = PlatformTextTrack::create(this, label(), language(), platformMode, platformKind, type, uniqueId());

    return m_platformTextTrack;
}
#endif

bool TextTrack::isMainProgramContent() const
{
    // "Main program" content is intrinsic to the presentation of the media file, regardless of locale. Content such as
    // directors commentary is not "main program" because it is not essential for the presentation. HTML5 doesn't have
    // a way to express this in a machine-reable form, it is typically done with the track label, so we assume that caption
    // tracks are main content and all other track types are not.
    return kind() == captionsKeyword();
}

#if ENABLE(MEDIA_SOURCE)
void TextTrack::setLanguage(const AtomicString& language)
{
    // 11.1 language, on setting:
    // 1. If the value being assigned to this attribute is not an empty string or a BCP 47 language
    // tag[BCP47], then abort these steps.
    // FIXME(123926): Validate the BCP47-ness of langague.

    // 2. Update this attribute to the new value.
    TrackBase::setLanguage(language);

    // 3. If the sourceBuffer attribute on this track is not null, then queue a task to fire a simple
    // event named change at sourceBuffer.textTracks.
    if (m_sourceBuffer)
        m_sourceBuffer->textTracks()->scheduleChangeEvent();

    // 4. Queue a task to fire a simple event named change at the TextTrackList object referenced by
    // the textTracks attribute on the HTMLMediaElement.
    if (mediaElement())
        mediaElement()->textTracks()->scheduleChangeEvent();
}
#endif

} // namespace WebCore

#endif

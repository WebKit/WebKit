/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "TrackPrivateBaseGStreamer.h"

#include "GStreamerCommon.h"
#include "Logging.h"
#include "TrackPrivateBase.h"
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

static GRefPtr<GstPad> findBestUpstreamPad(GRefPtr<GstPad> pad)
{
    GRefPtr<GstPad> sinkPad = pad;
    GRefPtr<GstPad> peerSrcPad;

    peerSrcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    // Some tag events with language tags don't reach the webkittextcombiner pads on time.
    // It's better to listen for them in the earlier upstream ghost pads.
    if (GST_IS_GHOST_PAD(peerSrcPad.get()))
        sinkPad = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD(peerSrcPad.get())));
    return sinkPad;
}

TrackPrivateBaseGStreamer::TrackPrivateBaseGStreamer(TrackPrivateBase* owner, gint index, GRefPtr<GstPad> pad)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_index(index)
    , m_eventProbe(0)
    , m_owner(owner)
{
    setPad(WTFMove(pad));
    ASSERT(m_pad);

    // We can't call notifyTrackOfTagsChanged() directly, because we need tagsChanged() to setup m_tags.
    tagsChanged();
}

TrackPrivateBaseGStreamer::TrackPrivateBaseGStreamer(TrackPrivateBase* owner, gint index, GRefPtr<GstStream> stream)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_index(index)
    , m_stream(stream)
    , m_eventProbe(0)
    , m_owner(owner)
{
    ASSERT(m_stream);

    // We can't call notifyTrackOfTagsChanged() directly, because we need tagsChanged() to setup m_tags.
    tagsChanged();
}

void TrackPrivateBaseGStreamer::setPad(GRefPtr<GstPad>&& pad)
{
    if (m_bestUpstreamPad && m_eventProbe)
        gst_pad_remove_probe(m_bestUpstreamPad.get(), m_eventProbe);

    m_pad = WTFMove(pad);
    m_bestUpstreamPad = findBestUpstreamPad(m_pad);
    m_eventProbe = gst_pad_add_probe(m_bestUpstreamPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [] (GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* track = static_cast<TrackPrivateBaseGStreamer*>(userData);
        switch (GST_EVENT_TYPE(gst_pad_probe_info_get_event(info))) {
        case GST_EVENT_TAG:
            tagsChangedCallback(track);
            break;
        default:
            break;
        }
        return GST_PAD_PROBE_OK;
    }, this, nullptr);
}

TrackPrivateBaseGStreamer::~TrackPrivateBaseGStreamer()
{
    disconnect();
    m_notifier->invalidate();
}

void TrackPrivateBaseGStreamer::disconnect()
{
    m_tags.clear();

    if (m_stream)
        m_stream.clear();

    m_notifier->cancelPendingNotifications();

    if (m_bestUpstreamPad && m_eventProbe) {
        gst_pad_remove_probe(m_bestUpstreamPad.get(), m_eventProbe);
        m_eventProbe = 0;
        m_bestUpstreamPad.clear();
    }

    if (m_pad)
        m_pad.clear();
}

void TrackPrivateBaseGStreamer::tagsChangedCallback(TrackPrivateBaseGStreamer* track)
{
    track->tagsChanged();
}

void TrackPrivateBaseGStreamer::tagsChanged()
{
    GRefPtr<GstTagList> tags;
    if (m_bestUpstreamPad) {
        GRefPtr<GstEvent> tagEvent;
        guint i = 0;
        // Prefer the tag event having a language tag, if available.
        do {
            tagEvent = adoptGRef(gst_pad_get_sticky_event(m_bestUpstreamPad.get(), GST_EVENT_TAG, i));
            if (tagEvent) {
                GstTagList* tagsFromEvent;
                gst_event_parse_tag(tagEvent.get(), &tagsFromEvent);
                tags = adoptGRef(gst_tag_list_copy(tagsFromEvent));
                String language;
                if (getTag(tags.get(), GST_TAG_LANGUAGE_CODE, language))
                    break;
            }
            i++;
        } while (tagEvent);
    } else if (m_stream)
        tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (!tags)
        tags = adoptGRef(gst_tag_list_new_empty());

    GST_DEBUG("Inspecting track at index %d with tags: %" GST_PTR_FORMAT, m_index, tags.get());
    {
        Locker locker { m_tagMutex };
        m_tags.swap(tags);
    }

    m_notifier->notify(MainThreadNotification::TagsChanged, [this] { notifyTrackOfTagsChanged(); });
}

bool TrackPrivateBaseGStreamer::getLanguageCode(GstTagList* tags, AtomString& value)
{
    String language;
    if (getTag(tags, GST_TAG_LANGUAGE_CODE, language)) {
        language = gst_tag_get_language_code_iso_639_1(language.utf8().data());
        GST_DEBUG("Converted track %d's language code to %s.", m_index, language.utf8().data());
        if (language != value) {
            value = language;
            return true;
        }
    }
    return false;
}

template<class StringType>
bool TrackPrivateBaseGStreamer::getTag(GstTagList* tags, const gchar* tagName, StringType& value)
{
    GUniqueOutPtr<gchar> tagValue;
    if (gst_tag_list_get_string(tags, tagName, &tagValue.outPtr())) {
        GST_DEBUG("Track %d got %s %s.", m_index, tagName, tagValue.get());
        value = tagValue.get();
        return true;
    }
    return false;
}

void TrackPrivateBaseGStreamer::notifyTrackOfTagsChanged()
{
    TrackPrivateBaseClient* client = m_owner->client();

    GRefPtr<GstTagList> tags;
    {
        Locker locker { m_tagMutex };
        tags.swap(m_tags);
    }

    if (!tags)
        return;

    if (getTag(tags.get(), GST_TAG_TITLE, m_label) && client)
        client->labelChanged(m_label);

    AtomString language;
    if (!getLanguageCode(tags.get(), language))
        return;

    if (language == m_language)
        return;

    m_language = language;
    if (client)
        client->languageChanged(m_language);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)

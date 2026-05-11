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
#include "TrackPrivateBase.h"
#include <gst/tag/tag.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

static GRefPtr<GstTagList> getAllTags(const GRefPtr<GstPad>& pad)
{
    auto allTags = adoptGRef(gst_tag_list_new_empty());
    GstTagList* taglist = nullptr;
    for (unsigned i = 0;; i++) {
        GRefPtr<GstEvent> tagsEvent = adoptGRef(gst_pad_get_sticky_event(pad.get(), GST_EVENT_TAG, i));
        if (!tagsEvent)
            break;
        gst_event_parse_tag(tagsEvent.get(), &taglist);
        allTags = adoptGRef(gst_tag_list_merge(allTags.get(), taglist, GST_TAG_MERGE_APPEND));
    }
    return allTags;
}

static GRefPtr<GstPad> findBestUpstreamPad(GRefPtr<GstPad> pad)
{
    if (!pad)
        return nullptr;

    GRefPtr<GstPad> sinkPad = pad;
    GRefPtr<GstPad> peerSrcPad;

    peerSrcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    // Some tag events with language tags don't reach the webkittextcombiner pads on time.
    // It's better to listen for them in the earlier upstream ghost pads.
    if (GST_IS_GHOST_PAD(peerSrcPad.get()))
        sinkPad = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD(peerSrcPad.get())));
    return sinkPad;
}

static std::optional<String> getTag(GstTagList* tags, ASCIILiteral tagName)
{
    GUniqueOutPtr<gchar> tagValue;
    if (!gst_tag_list_get_string(tags, tagName.characters(), &tagValue.outPtr()))
        return std::nullopt;

    GST_DEBUG("Track got %s %s.", tagName.characters(), tagValue.get());
    return String(byteCast<char8_t>(unsafeSpan(tagValue.get())));
}

static std::optional<String> getLanguageCode(GstTagList* tags)
{
    auto language = getTag(tags, ASCIILiteral::fromLiteralUnsafe(GST_TAG_LANGUAGE_CODE));
    if (!language)
        return std::nullopt;

    auto convertedLanguage = CStringView::unsafeFromUTF8(gst_tag_get_language_code_iso_639_1(language->utf8().data()));
    GST_DEBUG("Converted track's language code to %s.", convertedLanguage.utf8());
    return String(convertedLanguage.span());
}

TrackDataHolder::TrackDataHolder(TrackPrivateBaseGStreamer& track)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_track(track)
{
}

TrackPrivateBaseGStreamer::TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&& player, GStreamerTrackType type, TrackPrivateBase* owner, unsigned index, GRefPtr<GstPad>&& pad, bool shouldHandleStreamStartEvent)
    : m_data(TrackDataHolder::create(*this))
{
    m_data->m_player = WTF::move(player);
    m_data->m_index = index;
    m_data->m_type = type;
    m_data->m_owner = owner;
    m_data->m_shouldHandleStreamStartEvent = shouldHandleStreamStartEvent;
    ASSERT(pad);
    m_data->setPad(WTF::move(pad));

    // We can't call notifyTrackOfTagsChanged() directly, because we need tagsChanged() to setup m_tags.
    m_data->tagsChanged();

    m_data->installUpdateConfigurationHandlers();
}

TrackPrivateBaseGStreamer::TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&& player, GStreamerTrackType type, TrackPrivateBase* owner, unsigned index, GRefPtr<GstPad>&& pad, TrackID trackId)
    : m_data(TrackDataHolder::create(*this))
{
    m_data->m_player = WTF::move(player);
    m_data->m_index = index;
    m_data->m_id = trackId;
    m_data->m_type = type;
    m_data->m_owner = owner;
    m_data->m_shouldUsePadStreamId = false;
    m_data->m_shouldHandleStreamStartEvent = false;

    ASSERT(pad);
    m_data->setPad(WTF::move(pad));

    // We can't call notifyTrackOfTagsChanged() directly, because we need tagsChanged() to setup m_tags.
    m_data->tagsChanged();

    m_data->installUpdateConfigurationHandlers();
}

TrackPrivateBaseGStreamer::TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&& player, GStreamerTrackType type, TrackPrivateBase* owner, unsigned index, GstStream* stream)
    : m_data(TrackDataHolder::create(*this))
{
    ASSERT(stream);

    m_data->m_player = WTF::move(player);
    m_data->m_index = index;
    m_data->m_gstStreamId = byteCast<Latin1Character>(unsafeSpan(gst_stream_get_stream_id(stream)));
    m_data->m_id = parseStreamId(m_data->m_gstStreamId).value_or(index);
    m_data->m_type = type;
    m_data->m_owner = owner;

    m_data->setStream(GRefPtr(stream));

    // We can't call notifyTrackOfTagsChanged() directly, because we need tagsChanged() to setup m_tags.
    m_data->tagsChanged();

    m_data->installUpdateConfigurationHandlers();
}

TrackPrivateBaseGStreamer::~TrackPrivateBaseGStreamer()
{
    disconnect();
}

void TrackPrivateBaseGStreamer::setPad(GRefPtr<GstPad>&& pad)
{
    m_data->setPad(WTF::move(pad));
}

void TrackDataHolder::setPad(GRefPtr<GstPad>&& pad)
{
    ASSERT(isMainThread());

    if (m_bestUpstreamPad && m_eventProbe)
        gst_pad_remove_probe(m_bestUpstreamPad.get(), m_eventProbe);

    g_clear_signal_handler(&m_padTagsNotifyHandlerId, m_pad.get());
    g_clear_signal_handler(&m_padCapsNotifyHandlerId, m_pad.get());
    m_pad = WTF::move(pad);
    m_bestUpstreamPad = findBestUpstreamPad(m_pad);
    m_gstStreamId = byteCast<Latin1Character>(unsafeSpan(gst_pad_get_stream_id(m_pad.get())));

    if (m_shouldUsePadStreamId)
        m_id = parseStreamId(m_gstStreamId).value_or(m_index);

    if (!m_bestUpstreamPad)
        return;

    m_eventProbe = gst_pad_add_probe(m_bestUpstreamPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
        if (!holder)
            return GST_PAD_PROBE_REMOVE;

        auto event = gst_pad_probe_info_get_event(info);
        switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_TAG:
            holder->tagsChanged();
            break;
        case GST_EVENT_STREAM_START:
            if (holder->m_shouldHandleStreamStartEvent)
                holder->streamChanged();
            break;
        case GST_EVENT_CAPS: {
            holder->m_taskQueue.enqueueTask([weakHolder = ThreadSafeWeakPtr<TrackDataHolder> { holder.get() }, event = GRefPtr<GstEvent>(event)]() mutable {
                RefPtr holder = weakHolder.get();
                if (!holder)
                    return;

                GstCaps* caps;
                gst_event_parse_caps(event.get(), &caps);
                if (!caps)
                    return;
                holder->m_track.capsChanged(holder->m_id, GRefPtr<GstCaps>(caps));
            });
            break;
        }
        default:
            break;
        }
        return GST_PAD_PROBE_OK;
    }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, reinterpret_cast<GDestroyNotify>(+[](gpointer data) {
        delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
    }));
}

void TrackDataHolder::setStream(GRefPtr<GstStream>&& stream)
{
    g_clear_signal_handler(&m_streamTagsNotifyHandlerId, m_stream.get());
    g_clear_signal_handler(&m_streamCapsNotifyHandlerId, m_stream.get());
    m_stream = WTF::move(stream);
    m_streamTagsNotifyHandlerId = g_signal_connect_data(m_stream.get(), "notify::tags", G_CALLBACK(+[](GstStream*, GParamSpec*, gpointer userData) {
        RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
        if (!holder)
            return;
        holder->tagsChanged();
    }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, [](gpointer data, GClosure*) {
        delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
    }, static_cast<GConnectFlags>(0));
}

TrackDataHolder::~TrackDataHolder()
{
    disconnect();
    m_notifier->invalidate();
}

GstObject* TrackDataHolder::objectForLogging() const
{
    if (m_stream)
        return GST_OBJECT_CAST(m_stream.get());

    ASSERT(m_pad);
    return GST_OBJECT_CAST(m_pad.get());
}

GRefPtr<GstPad> TrackPrivateBaseGStreamer::pad() const
{
    return m_data->m_pad;
}

unsigned TrackPrivateBaseGStreamer::index()
{
    return m_data->m_index;
}

void TrackPrivateBaseGStreamer::setIndex(unsigned index)
{
    m_data->m_index = index;
}

GRefPtr<GstStream> TrackPrivateBaseGStreamer::stream() const
{
    return m_data->m_stream;
}

void TrackPrivateBaseGStreamer::setInitialCaps(GRefPtr<GstCaps>&& caps)
{
    m_data->m_initialCaps = WTF::move(caps);
}

GRefPtr<GstCaps> TrackPrivateBaseGStreamer::initialCaps()
{
    return m_data->m_initialCaps;
}

TrackID TrackPrivateBaseGStreamer::streamId() const
{
    return m_data->m_id;
}

String TrackPrivateBaseGStreamer::gstStreamId() const
{
    return m_data->m_gstStreamId;
}

void TrackPrivateBaseGStreamer::disconnect()
{
    m_data->disconnect();
}

void TrackDataHolder::disconnect()
{
    m_taskQueue.startAborting();

    if (m_stream) {
        g_clear_signal_handler(&m_streamTagsNotifyHandlerId, m_stream.get());
        g_clear_signal_handler(&m_streamCapsNotifyHandlerId, m_stream.get());
        m_stream.clear();
    }

    m_tags.clear();

    m_notifier->cancelPendingNotifications();

    if (m_bestUpstreamPad && m_eventProbe) {
        gst_pad_remove_probe(m_bestUpstreamPad.get(), m_eventProbe);
        m_eventProbe = 0;
        m_bestUpstreamPad.clear();
    }

    if (m_pad) {
        g_clear_signal_handler(&m_padTagsNotifyHandlerId, m_pad.get());
        g_clear_signal_handler(&m_padCapsNotifyHandlerId, m_pad.get());
        m_pad.clear();
    }

    m_player = nullptr;
    m_taskQueue.finishAborting();
}

void TrackDataHolder::tagsChanged()
{
    // May be called by any thread, including the streaming thread.
    GRefPtr<GstTagList> tags;
    if (m_bestUpstreamPad) {
        GRefPtr<GstEvent> tagEvent;
        unsigned i = 0;
        // Prefer the tag event having a language tag, if available.
        do {
            tagEvent = adoptGRef(gst_pad_get_sticky_event(m_bestUpstreamPad.get(), GST_EVENT_TAG, i));
            if (tagEvent) {
                GstTagList* tagsFromEvent = nullptr;
                gst_event_parse_tag(tagEvent.get(), &tagsFromEvent);
                tags = adoptGRef(gst_tag_list_copy(tagsFromEvent));
                auto language = getTag(tags.get(), ASCIILiteral::fromLiteralUnsafe(GST_TAG_LANGUAGE_CODE));
                if (language)
                    break;
            }
            i++;
        } while (tagEvent);
    } else if (m_stream)
        tags = adoptGRef(gst_stream_get_tags(m_stream.get()));

    if (!tags)
        tags = adoptGRef(gst_tag_list_new_empty());

    GST_DEBUG("Inspecting track %" PRIu64 " with tags: %" GST_PTR_FORMAT, m_id, tags.get());
    {
        Locker locker { m_tagMutex };
        m_tags.swap(tags);
    }

    m_notifier->notify(MainThreadNotification::TagsChanged, [this] {
        notifyTrackOfTagsChanged();
    });
}

void TrackDataHolder::notifyTrackOfTagsChanged()
{
    ASSERT(isMainThread());
    GRefPtr<GstTagList> tags;
    {
        Locker locker { m_tagMutex };
        tags.swap(m_tags);
    }

    if (!tags)
        return;

    m_track.tagsChanged(GRefPtr<GstTagList>(tags));

    RefPtr owner = m_owner.get();
    if (!owner)
        return;

    auto label = getTag(tags.get(), ASCIILiteral::fromLiteralUnsafe(GST_TAG_TITLE));
    if (label) {
        m_label = *label;
        owner->notifyMainThreadClient([&](auto& client) {
            client.labelChanged(m_label);
        });
    }

    auto language = getLanguageCode(tags.get());
    if (!language)
        return;

    if (*language == m_language)
        return;

    m_language = *language;
    owner->notifyMainThreadClient([&](auto& client) {
        client.languageChanged(m_language);
    });
}

void TrackDataHolder::streamIdChanged()
{
    if (!m_pad)
        return;

    String gstStreamId = byteCast<Latin1Character>(unsafeSpan(gst_pad_get_stream_id(m_pad.get())));
    auto streamId = parseStreamId(gstStreamId);
    if (!streamId)
        return;

    ASSERT(isMainThread());
    m_gstStreamId = gstStreamId;
    m_id = streamId.value();
    GST_INFO("Track %" PRIu64 " got stream start. GStreamer stream-id: %s", m_id, m_gstStreamId.utf8().data());
}

void TrackDataHolder::streamChanged()
{
    m_notifier->notify(MainThreadNotification::StreamChanged, [this] {
        streamIdChanged();
    });
}

void TrackDataHolder::installUpdateConfigurationHandlers()
{
    if (m_pad) {
        m_padCapsNotifyHandlerId = g_signal_connect_data(m_pad.get(), "notify::caps", G_CALLBACK(+[](GstPad*, GParamSpec*, gpointer userData) {
            RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
            if (!holder)
                return;
            if (!holder->m_pad)
                return;
            auto caps = adoptGRef(gst_pad_get_current_caps(holder->m_pad.get()));
            // We will receive a synchronous notification for caps being unset during pipeline teardown.
            if (!caps)
                return;

            holder->m_taskQueue.enqueueTask([weakHolder = ThreadSafeWeakPtr<TrackDataHolder> { holder.get() }, caps = WTF::move(caps)]() mutable {
                auto holder = weakHolder.get();
                if (!holder)
                    return;
                holder->m_track.capsChanged(getStreamIdFromPad(holder->m_pad.get()).value_or(holder->m_index), WTF::move(caps));
            });
        }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, [](gpointer data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
        }, static_cast<GConnectFlags>(0));
        m_padTagsNotifyHandlerId = g_signal_connect_data(m_pad.get(), "notify::tags", G_CALLBACK(+[](GstPad*, GParamSpec*, gpointer userData) {
            RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
            if (!holder)
                return;
            holder->m_taskQueue.enqueueTask([weakHolder = ThreadSafeWeakPtr<TrackDataHolder> { holder.get() }]() mutable {
                auto holder = weakHolder.get();
                if (!holder)
                    return;
                if (!holder->m_pad)
                    return;
                holder->m_track.updateConfigurationFromTags(getAllTags(holder->m_pad));
            });
        }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, [](gpointer data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
        }, static_cast<GConnectFlags>(0));
    } else if (m_stream) {
        m_streamCapsNotifyHandlerId = g_signal_connect_data(m_stream.get(), "notify::caps", G_CALLBACK(+[](GstStream*, GParamSpec*, gpointer userData) {
            RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
            if (!holder)
                return;
            holder->m_taskQueue.enqueueTask([weakHolder = ThreadSafeWeakPtr<TrackDataHolder> { holder.get() }]() mutable {
                auto holder = weakHolder.get();
                if (!holder)
                    return;
                auto caps = adoptGRef(gst_stream_get_caps(holder->m_stream.get()));
                holder->m_track.capsChanged(getStreamIdFromStream(holder->m_stream.get()).value_or(holder->m_index), WTF::move(caps));
            });
        }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, [](gpointer data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
        }, static_cast<GConnectFlags>(0));

        // This signal can be triggered from the main thread
        // (CanvasCaptureMediaStreamTrack::Source::captureCanvas() triggering the mediastreamsrc
        // InternalSource::videoFrameAvailable() which can update the stream tags.)
        m_streamTagsNotifyHandlerId = g_signal_connect_data(m_stream.get(), "notify::tags", G_CALLBACK(+[](GstStream*, GParamSpec*, gpointer userData) {
            RefPtr holder = reinterpret_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(userData)->get();
            if (!holder)
                return;
            if (isMainThread()) {
                auto tags = adoptGRef(gst_stream_get_tags(holder->m_stream.get()));
                holder->m_track.updateConfigurationFromTags(WTF::move(tags));
                return;
            }
            holder->m_taskQueue.enqueueTask([weakHolder = ThreadSafeWeakPtr<TrackDataHolder> { holder.get() }]() mutable {
                auto holder = weakHolder.get();
                if (!holder)
                    return;
                auto tags = adoptGRef(gst_stream_get_tags(holder->m_stream.get()));
                holder->m_track.updateConfigurationFromTags(WTF::move(tags));
            });
        }), new ThreadSafeWeakPtr<TrackDataHolder> { this }, [](gpointer data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<TrackDataHolder>*>(data);
        }, static_cast<GConnectFlags>(0));
    }
}

bool TrackDataHolder::updateTrackIDFromTags(const GRefPtr<GstTagList>& tags)
{
    ASSERT(isMainThread());
    GUniqueOutPtr<char> trackIDString;
    if (!gst_tag_list_get_string(tags.get(), "container-specific-track-id", &trackIDString.outPtr()))
        return false;

    auto trackID = WTF::parseInteger<TrackID>(byteCast<Latin1Character>(unsafeSpan(trackIDString.get())));
    if (trackID && *trackID != m_trackID.value_or(0)) {
        m_trackID = *trackID;
        ASSERT(m_trackID);
        return true;
    }
    return false;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)

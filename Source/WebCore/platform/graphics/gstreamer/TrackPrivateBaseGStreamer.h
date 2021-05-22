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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MainThreadNotifier.h"
#include <gst/gst.h>
#include <wtf/Lock.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TrackPrivateBase;

class TrackPrivateBaseGStreamer {
public:
    virtual ~TrackPrivateBaseGStreamer();

    enum TrackType {
        Audio,
        Video,
        Text,
        Unknown
    };

    GstPad* pad() const { return m_pad.get(); }
    void setPad(GRefPtr<GstPad>&&);

    virtual void disconnect();

    virtual void setActive(bool) { }

    void setIndex(int index) { m_index =  index; }

    GstStream* stream()
    {
        return m_stream.get();
    }

    // Used for MSE, where the initial caps of the pad are relevant for initializing the matching pad in the
    // playback pipeline.
    void setInitialCaps(GRefPtr<GstCaps>&& caps) { m_initialCaps = WTFMove(caps); }
    const GRefPtr<GstCaps>& initialCaps() { return m_initialCaps; }

protected:
    TrackPrivateBaseGStreamer(TrackPrivateBase* owner, gint index, GRefPtr<GstPad>);
    TrackPrivateBaseGStreamer(TrackPrivateBase* owner, gint index, GRefPtr<GstStream>);

    void notifyTrackOfTagsChanged();

    enum MainThreadNotification {
        TagsChanged = 1 << 1,
        NewSample = 1 << 2,
        StreamChanged = 1 << 3
    };

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    gint m_index;
    AtomString m_label;
    AtomString m_language;
    GRefPtr<GstPad> m_pad;
    GRefPtr<GstPad> m_bestUpstreamPad;
    GRefPtr<GstStream> m_stream;
    gulong m_eventProbe;
    GRefPtr<GstCaps> m_initialCaps;

private:
    bool getLanguageCode(GstTagList* tags, AtomString& value);

    template<class StringType>
    bool getTag(GstTagList* tags, const gchar* tagName, StringType& value);

    static void activeChangedCallback(TrackPrivateBaseGStreamer*);
    static void tagsChangedCallback(TrackPrivateBaseGStreamer*);

    void tagsChanged();

    TrackPrivateBase* m_owner;
    Lock m_tagMutex;
    GRefPtr<GstTagList> m_tags;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)

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

#ifndef TrackPrivateBaseGStreamer_h
#define TrackPrivateBaseGStreamer_h

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#include "GRefPtrGStreamer.h"
#include <wtf/ThreadingPrimitives.h>
#include <wtf/gobject/GThreadSafeMainLoopSource.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TrackPrivateBase;

class TrackPrivateBaseGStreamer {
public:
    virtual ~TrackPrivateBaseGStreamer();

    GstPad* pad() const { return m_pad.get(); }

    virtual void disconnect();

    virtual void setActive(bool) { }

    void setIndex(int index) { m_index =  index; }

    void activeChanged();
    void tagsChanged();

    void notifyTrackOfActiveChanged();
    void notifyTrackOfTagsChanged();

protected:
    TrackPrivateBaseGStreamer(TrackPrivateBase* owner, gint index, GRefPtr<GstPad>);

    gint m_index;
    AtomicString m_label;
    AtomicString m_language;
    GRefPtr<GstPad> m_pad;

private:
    bool getLanguageCode(GstTagList* tags, AtomicString& value);

    template<class StringType>
    bool getTag(GstTagList* tags, const gchar* tagName, StringType& value);

    TrackPrivateBase* m_owner;
    GThreadSafeMainLoopSource m_activeTimerHandler;
    GThreadSafeMainLoopSource m_tagTimerHandler;

    Mutex m_tagMutex;
    GRefPtr<GstTagList> m_tags;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#endif // TrackPrivateBaseGStreamer_h

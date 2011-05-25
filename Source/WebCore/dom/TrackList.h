/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
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

#ifndef TrackList_h
#define TrackList_h

#if ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#include "EventNames.h"
#include "EventTarget.h"
#include "MediaStreamFrameController.h"
#include "ScriptExecutionContext.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Track : public RefCounted<Track> {
public:
    static PassRefPtr<Track> create(const String& id, const AtomicString& kind, const String& label, const String& language);

    const String& id() const { return m_id; }
    const AtomicString& kind() const { return m_kind; }
    const String& label() const { return m_label; }
    const String& language() const { return m_language; }

private:
    Track(const String& id, const AtomicString& kind, const String& label, const String& language);

    String m_id;
    AtomicString m_kind;
    String m_label;
    String m_language;
};

typedef Vector<RefPtr<Track> > TrackVector;

class TrackList : public RefCounted<TrackList>,
                  public EventTarget {
public:
    static PassRefPtr<TrackList> create(const TrackVector&);
    virtual ~TrackList();

    unsigned long length() const;
    const String& getID(unsigned long index, ExceptionCode&) const;
    const AtomicString& getKind(unsigned long index, ExceptionCode&) const;
    const String& getLabel(unsigned long index, ExceptionCode&) const;
    const String& getLanguage(unsigned long index, ExceptionCode&) const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    virtual void clear();

    // EventTarget implementation.
    virtual TrackList* toTrackList();
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    using RefCounted<TrackList>::ref;
    using RefCounted<TrackList>::deref;

    template <typename T, typename U>
    class DispatchTask : public ScriptExecutionContext::Task {
    public:
        typedef void (T::*Callback)(U);

        static PassOwnPtr<DispatchTask> create(PassRefPtr<T> object, Callback callback, U args)
        {
            return adoptPtr(new DispatchTask(object, callback, args));
        }

        virtual void performTask(ScriptExecutionContext*)
        {
            (m_object.get()->*m_callback)(m_args);
        }

    public:
        DispatchTask(PassRefPtr<T> object, Callback callback, U args)
            : m_object(object)
            , m_callback(callback)
            , m_args(args) { }

        RefPtr<T> m_object;
        Callback m_callback;
        U m_args;
    };

protected:
    TrackList(const TrackVector&);
    bool checkIndex(long index, ExceptionCode&, long allowed = 0) const;
    void postChangeEvent();

    // EventTarget implementation.
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

private:
    // EventTarget implementation.
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    void dispatchChangeEvent(int);

    EventTargetData m_eventTargetData;
    TrackVector m_tracks;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#endif // TrackList_h

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Dictionary_h
#define Dictionary_h

#include "EventListener.h"
#include "MessagePort.h"
#include "PlatformString.h"
#include "ScriptValue.h"
#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class DOMStringList;
class DOMWindow;
class IDBKeyRange;
class MediaKeyError;
class Notification;
class SpeechRecognitionError;
class SpeechRecognitionResult;
class SpeechRecognitionResultList;
class Storage;
class TrackBase;

class Dictionary {
public:
    Dictionary();
    Dictionary(const v8::Local<v8::Value>& options);
    ~Dictionary();

    Dictionary& operator=(const Dictionary&);

    bool isObject() const;
    bool isUndefinedOrNull() const;

    bool get(const String&, bool&) const;
    bool get(const String&, int32_t&) const;
    bool get(const String&, double&) const;
    bool get(const String&, String&) const;
    bool get(const String&, ScriptValue&) const;
    bool get(const String&, short&) const;
    bool get(const String&, unsigned short&) const;
    bool get(const String&, unsigned&) const;
    bool get(const String&, unsigned long long&) const;
    bool get(const String&, RefPtr<DOMWindow>&) const;
    bool get(const String&, RefPtr<Storage>&) const;
    bool get(const String&, MessagePortArray&) const;
    bool get(const String&, RefPtr<Uint8Array>&) const;
#if ENABLE(ENCRYPTED_MEDIA)
    bool get(const String&, RefPtr<MediaKeyError>&) const;
#endif
#if ENABLE(VIDEO_TRACK)
    bool get(const String&, RefPtr<TrackBase>&) const;
#endif
#if ENABLE(SCRIPTED_SPEECH)
    bool get(const String&, RefPtr<SpeechRecognitionError>&) const;
    bool get(const String&, RefPtr<SpeechRecognitionResult>&) const;
    bool get(const String&, RefPtr<SpeechRecognitionResultList>&) const;
#endif
    bool get(const String&, HashSet<AtomicString>&) const;
    bool get(const String&, Dictionary&) const;
    bool getOwnPropertiesAsStringHashMap(WTF::HashMap<String, String>&) const;

    bool getWithUndefinedOrNullCheck(const String&, String&) const;

    PassRefPtr<EventListener> getEventListener(const String&, Notification*) const { return 0; }

private:
    bool getKey(const String& key, v8::Local<v8::Value>&) const;

    // This object can only be used safely when stack allocated because of v8::Local.
    static void* operator new(size_t);
    static void* operator new[](size_t);
    static void operator delete(void *);

    v8::Local<v8::Value> m_options;
};

}

#endif // Dictionary_h

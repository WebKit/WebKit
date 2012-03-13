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

#include "SerializedScriptValue.h"
#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class DOMStringList;
class DOMWindow;
class IDBKeyRange;
class ScriptValue;
class Storage;
class TrackBase;

// FIXME: Implement.
class Dictionary {
public:
    Dictionary() { }

    bool isObject() const { return false; }
    bool isUndefinedOrNull() const { return false; }

    bool get(const String&, bool&) const { return false; }
    bool get(const String&, int32_t&) const { return false; }
    bool get(const String&, double&) const { return false; }
    bool get(const String&, String&) const { return false; }
    bool get(const String&, ScriptValue&) const { return false; }
    bool get(const String&, unsigned short&) const { return false; }
    bool get(const String&, unsigned&) const { return false; }
    bool get(const String&, unsigned long long&) const { return false; }
    bool get(const String&, RefPtr<DOMWindow>&) const { return false; }
    bool get(const String&, RefPtr<Storage>&) const { return false; }
    bool get(const String&, MessagePortArray&) const { return false; }
#if ENABLE(VIDEO_TRACK)
    bool get(const String&, RefPtr<TrackBase>&) const { return false; }
#endif
    bool get(const String&, HashSet<AtomicString>&) const { return false; }

    bool getWithUndefinedOrNullCheck(const String&, String&) const { return false; }
};

}

#endif // Dictionary_h

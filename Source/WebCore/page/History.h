/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef History_h
#define History_h

#include "DOMWindowProperty.h"
#include "ScriptWrappable.h"
#include "SerializedScriptValue.h"
#include "URL.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class Frame;
struct ExceptionCodeWithMessage;
typedef int ExceptionCode;

class History : public ScriptWrappable, public RefCounted<History>, public DOMWindowProperty {
public:
    static Ref<History> create(Frame* frame) { return adoptRef(*new History(frame)); }

    unsigned length() const;
    PassRefPtr<SerializedScriptValue> state();
    void back();
    void forward();
    void go(int);

    void back(Document&);
    void forward(Document&);
    void go(Document&, int);

    bool stateChanged() const;
    bool isSameAsCurrentState(SerializedScriptValue*) const;

    enum class StateObjectType {
        Push,
        Replace
    };
    void stateObjectAdded(PassRefPtr<SerializedScriptValue>, const String& title, const String& url, StateObjectType, ExceptionCodeWithMessage&);

private:
    explicit History(Frame*);

    URL urlForState(const String& url);

    PassRefPtr<SerializedScriptValue> stateInternal() const;

    RefPtr<SerializedScriptValue> m_lastStateObjectRequested;

    unsigned m_nonUserGestureObjectsAdded { 0 };
    unsigned m_currentUserGestureObjectsAdded { 0 };
    double m_currentUserGestureTimestamp { 0 };

    // For the main frame's History object to keep track of all state object usage.
    uint64_t m_totalStateObjectUsage { 0 };

    // For each individual History object to keep track of the most recent state object added.
    uint64_t m_mostRecentStateObjectUsage { 0 };
};

} // namespace WebCore

#endif // History_h

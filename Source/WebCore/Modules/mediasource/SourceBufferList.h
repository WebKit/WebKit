/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class SourceBuffer;
class WebCoreOpaqueRoot;

class SourceBufferList final : public RefCounted<SourceBufferList>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SourceBufferList);
public:
    static Ref<SourceBufferList> create(ScriptExecutionContext*);
    virtual ~SourceBufferList();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    bool isSupportedPropertyIndex(unsigned index) const { return index < length(); }
    unsigned length() const { return m_list.size(); }

    RefPtr<SourceBuffer> item(unsigned index) const;

    void add(Ref<SourceBuffer>&&);
    void remove(SourceBuffer&);
    bool contains(SourceBuffer&) const;
    void clear();
    void replaceWith(Vector<Ref<SourceBuffer>>&&);

    auto begin() LIFETIME_BOUND { return m_list.begin(); }
    auto end() LIFETIME_BOUND { return m_list.end(); }
    auto begin() const LIFETIME_BOUND { return m_list.begin(); }
    auto end() const LIFETIME_BOUND { return m_list.end(); }
    size_t size() const { return m_list.size(); }

    // EventTarget interface
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::SourceBufferList; }
    ScriptExecutionContext* scriptExecutionContext() const final;

private:
    explicit SourceBufferList(ScriptExecutionContext*);

    void scheduleEvent(const AtomString&);

    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    Vector<Ref<SourceBuffer>> m_list;
};

WebCoreOpaqueRoot root(SourceBufferList*);

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)

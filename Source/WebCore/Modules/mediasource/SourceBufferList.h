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

#ifndef SourceBufferList_h
#define SourceBufferList_h

#if ENABLE(MEDIA_SOURCE)

#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class SourceBuffer;

class SourceBufferList final : public RefCounted<SourceBufferList>, public ScriptWrappable, public EventTargetWithInlineData {
public:
    static PassRefPtr<SourceBufferList> create(ScriptExecutionContext* context)
    {
        return adoptRef(new SourceBufferList(context));
    }
    virtual ~SourceBufferList();

    unsigned long length() const { return m_list.size(); }
    SourceBuffer* item(unsigned long index) const { return (index < m_list.size()) ? m_list[index].get() : 0; }

    void add(PassRefPtr<SourceBuffer>);
    void remove(SourceBuffer*);
    bool contains(SourceBuffer* buffer) { return m_list.find(buffer) != notFound; }
    void clear();

    Vector<RefPtr<SourceBuffer>>::iterator begin() { return m_list.begin(); }
    Vector<RefPtr<SourceBuffer>>::iterator end() { return m_list.end(); }

    // EventTarget interface
    virtual EventTargetInterface eventTargetInterface() const override { return SourceBufferListEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return m_scriptExecutionContext; }

    using RefCounted<SourceBufferList>::ref;
    using RefCounted<SourceBufferList>::deref;

private:
    explicit SourceBufferList(ScriptExecutionContext*);

    void scheduleEvent(const AtomicString&);

    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    ScriptExecutionContext* m_scriptExecutionContext;
    GenericEventQueue m_asyncEventQueue;

    Vector<RefPtr<SourceBuffer>> m_list;
};

} // namespace WebCore

#endif

#endif

/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(INSPECTOR)

#include "ScriptGCEvent.h"
#include "ScriptGCEventListener.h"
#include "V8Binding.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

static GCEventData& isolateGCEventData()
{
    V8BindingPerIsolateData* isolateData = V8BindingPerIsolateData::current();
    ASSERT(isolateData);
    return isolateData->gcEventData();
}

void ScriptGCEvent::addEventListener(ScriptGCEventListener* eventListener)
{
    GCEventData::GCEventListeners& listeners = isolateGCEventData().listeners();
    if (listeners.isEmpty()) {
        v8::V8::AddGCPrologueCallback(ScriptGCEvent::gcPrologueCallback);
        v8::V8::AddGCEpilogueCallback(ScriptGCEvent::gcEpilogueCallback);
    }
    listeners.append(eventListener);
}

void ScriptGCEvent::removeEventListener(ScriptGCEventListener* eventListener)
{
    ASSERT(eventListener);
    GCEventData::GCEventListeners& listeners = isolateGCEventData().listeners();
    ASSERT(!listeners.isEmpty());
    size_t i = listeners.find(eventListener);
    ASSERT(i != notFound);
    listeners.remove(i);
    if (listeners.isEmpty()) {
        v8::V8::RemoveGCPrologueCallback(ScriptGCEvent::gcPrologueCallback);
        v8::V8::RemoveGCEpilogueCallback(ScriptGCEvent::gcEpilogueCallback);
    }
}

void ScriptGCEvent::getHeapSize(size_t& usedHeapSize, size_t& totalHeapSize, size_t& heapSizeLimit)
{
    v8::HeapStatistics heapStatistics;
    v8::V8::GetHeapStatistics(&heapStatistics);
    usedHeapSize = heapStatistics.used_heap_size();
    totalHeapSize = heapStatistics.total_heap_size();
    heapSizeLimit = heapStatistics.heap_size_limit();
}

size_t ScriptGCEvent::getUsedHeapSize()
{
    v8::HeapStatistics heapStatistics;
    v8::V8::GetHeapStatistics(&heapStatistics);
    return heapStatistics.used_heap_size();
}

void ScriptGCEvent::gcPrologueCallback(v8::GCType type, v8::GCCallbackFlags flags)
{
    GCEventData& gcEventData = isolateGCEventData();
    gcEventData.startTime = WTF::monotonicallyIncreasingTime();
    gcEventData.usedHeapSize = getUsedHeapSize();
}

void ScriptGCEvent::gcEpilogueCallback(v8::GCType type, v8::GCCallbackFlags flags)
{
    GCEventData& gcEventData = isolateGCEventData();
    if (!gcEventData.usedHeapSize)
        return;
    double endTime = WTF::monotonicallyIncreasingTime();
    size_t usedHeapSize = getUsedHeapSize();
    size_t collectedBytes = usedHeapSize > gcEventData.usedHeapSize ? 0 : gcEventData.usedHeapSize - usedHeapSize;
    GCEventData::GCEventListeners& listeners = gcEventData.listeners();
    for (GCEventData::GCEventListeners::iterator i = listeners.begin(); i != listeners.end(); ++i)
        (*i)->didGC(gcEventData.startTime, endTime, collectedBytes);
    gcEventData.clear();
}
    
} // namespace WebCore

#endif // ENABLE(INSPECTOR)

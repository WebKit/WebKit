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

#include <wtf/CurrentTime.h>

namespace WebCore {

ScriptGCEvent::GCEventListeners ScriptGCEvent::s_eventListeners;
double ScriptGCEvent::s_startTime = 0.0;
size_t ScriptGCEvent::s_usedHeapSize = 0;

void ScriptGCEvent::addEventListener(ScriptGCEventListener* eventListener)
{
    ASSERT(eventListener);
    if (s_eventListeners.isEmpty()) {
        v8::V8::AddGCPrologueCallback(ScriptGCEvent::gcPrologueCallback);
        v8::V8::AddGCEpilogueCallback(ScriptGCEvent::gcEpilogueCallback);
    }
    s_eventListeners.append(eventListener);
}

void ScriptGCEvent::removeEventListener(ScriptGCEventListener* eventListener)
{
    ASSERT(eventListener);
    ASSERT(!s_eventListeners.isEmpty());
    size_t i = s_eventListeners.find(eventListener);
    ASSERT(i != notFound);
    s_eventListeners.remove(i);
    if (s_eventListeners.isEmpty()) {
        v8::V8::RemoveGCPrologueCallback(ScriptGCEvent::gcPrologueCallback);
        v8::V8::RemoveGCEpilogueCallback(ScriptGCEvent::gcEpilogueCallback);
    }
}

void ScriptGCEvent::getHeapSize(size_t& usedHeapSize, size_t& totalHeapSize)
{
    v8::HeapStatistics heapStatistics;
    v8::V8::GetHeapStatistics(&heapStatistics);
    usedHeapSize = heapStatistics.used_heap_size();
    totalHeapSize = heapStatistics.total_heap_size();
}

size_t ScriptGCEvent::getUsedHeapSize()
{
    v8::HeapStatistics heapStatistics;
    v8::V8::GetHeapStatistics(&heapStatistics);
    return heapStatistics.used_heap_size();
}

void ScriptGCEvent::gcPrologueCallback(v8::GCType type, v8::GCCallbackFlags flags)
{
    s_startTime = WTF::currentTimeMS();
    s_usedHeapSize = getUsedHeapSize();
}

void ScriptGCEvent::gcEpilogueCallback(v8::GCType type, v8::GCCallbackFlags flags)
{
    double endTime = WTF::currentTimeMS();
    size_t collectedBytes = s_usedHeapSize - getUsedHeapSize();
    GCEventListeners listeners(s_eventListeners);
    for (GCEventListeners::iterator i = listeners.begin(); i != listeners.end(); ++i)
        (*i)->didGC(s_startTime, endTime, collectedBytes);
}
    
} // namespace WebCore

#endif // ENABLE(INSPECTOR)

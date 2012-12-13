/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "NodeRareData.h"

#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>

namespace WebCore {

struct SameSizeAsNodeRareData {
    void* m_pointer[4];
    unsigned m_indicesAndBitfields[2];

#if ENABLE(MUTATION_OBSERVERS)
    void* m_mutationObserver;
#endif

#if ENABLE(MICRODATA)
    void* m_microData;
#endif
};
COMPILE_ASSERT(sizeof(NodeRareData) == sizeof(SameSizeAsNodeRareData), NodeRareDataShouldStaySmall);

void NodeListsNodeData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    info.addMember(m_childNodeList);
    info.addMember(m_atomicNameCaches);
    info.addMember(m_nameCaches);
    info.addMember(m_tagNodeListCacheNS);
}

void NodeRareData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    info.addMember(treeScope());
    info.addMember(m_nodeLists);

#if ENABLE(MUTATION_OBSERVERS)
    info.addMember(m_mutationObserverData);
#endif

#if ENABLE(MICRODATA)
    info.addMember(m_microDataTokenLists);
#endif
}

} // namespace WebCore

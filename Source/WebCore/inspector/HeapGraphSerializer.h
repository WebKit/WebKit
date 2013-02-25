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

#ifndef HeapGraphSerializer_h
#define HeapGraphSerializer_h

#if ENABLE(INSPECTOR)

#include "InspectorFrontend.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryInstrumentation.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class HeapGraphSerializer {
    WTF_MAKE_NONCOPYABLE(HeapGraphSerializer);
public:

    class Client {
    public:
        virtual ~Client() { }
        virtual void addNativeSnapshotChunk(PassRefPtr<TypeBuilder::Memory::HeapSnapshotChunk>) = 0;
    };

    explicit HeapGraphSerializer(Client*);
    ~HeapGraphSerializer();
    void reportNode(const WTF::MemoryObjectInfo&);
    void reportEdge(const void*, const char*, WTF::MemberType);
    void reportLeaf(const WTF::MemoryObjectInfo&, const char*);
    void reportBaseAddress(const void*, const void*);
    int registerString(const char*);

    PassRefPtr<InspectorObject> finish();

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    void pushUpdateIfNeeded();
    void pushUpdate();

    int toNodeId(const void*);

    void addRootNode();
    int registerTypeString(const char*);

    void reportEdgeImpl(const int toNodeId, const char* name, int memberType);
    int reportNodeImpl(const WTF::MemoryObjectInfo&, int edgesCount);

    Client* m_client;

    typedef HashMap<String, int> StringMap;
    StringMap m_stringToIndex;
    typedef TypeBuilder::Array<String> Strings;
    RefPtr<Strings> m_strings;

    typedef TypeBuilder::Array<int> Edges;
    RefPtr<Edges> m_edges;
    int m_nodeEdgesCount;
    static const size_t s_edgeFieldsCount = 3;

    typedef TypeBuilder::Array<int> Nodes;
    RefPtr<Nodes> m_nodes;
    static const size_t s_nodeFieldsCount = 5;

    typedef TypeBuilder::Array<int> BaseToRealNodeIdMap;
    RefPtr<BaseToRealNodeIdMap> m_baseToRealNodeIdMap;
    static const size_t s_idMapEntryFieldCount = 2;

    typedef HashMap<const void*, int> Address2NodeId;
    Address2NodeId m_address2NodeIdMap;

    Vector<const void*> m_roots;
    RefPtr<InspectorObject> m_typeStrings;

    size_t m_edgeTypes[WTF::LastMemberTypeEntry];
    int m_unknownClassNameId;
    int m_leafCount;

    static const int s_firstNodeId = 1;
};

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
#endif // !defined(HeapGraphSerializer_h)

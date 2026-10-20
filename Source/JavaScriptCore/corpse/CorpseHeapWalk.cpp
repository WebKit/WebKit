/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CorpseHeapWalk.h"

#if ENABLE(MYA_HEAP)

#include "CorpseError.h"
#include "CorpseSnapshot.h"
#include "CorpseSnapshotDebugInfo.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace JSC {
namespace Corpse {

static unsigned long long forReport(Address address)
{
    return address.toTargetVMAddress();
}

namespace {

class HeapWalker {
public:
    HeapWalker(Snapshot& snapshot, SnapshotDebugInfo& debugInfo, HeapAnalyzer& analyzer)
        : m_snapshot(snapshot)
        , m_debugInfo(debugInfo)
        , m_analyzer(analyzer)
    {
    }

    void run(const TargetObject& root)
    {
        node(root);
        while (!m_worklist.isEmpty()) {
            TargetObject object = m_worklist.takeLast();
            CORPSE_DIAGNOSTICS(diagnostics, "reading the %s at 0x%llx", name(object.type).utf8().legacyCStringPointer(), forReport(object.address));
            walkValue(object, object.address, object.type, ObjectKind::Complete);
        }
        resolveDeclaredEdges();
    }

private:
    enum class ObjectKind : bool { Complete, Base };

    // A pointer to a class that the holder's image only declares.
    struct DeclaredEdge {
        TargetObject from;
        Address to;
        const TargetDeclaration& declaration;
    };

    struct Undefined {
        unsigned count { 0 };
        Address example;
    };

    // The node for `object`: a node of the same type at its address, or a new one.
    TargetObject node(const TargetObject& object)
    {
        auto& types = m_nodes.ensure(object.address, [] {
            return Vector<TargetTypeRef, 1> { };
        }).iterator->value;
        for (const TargetType& existing : types) {
            if (sameType(existing, object.type))
                return { object.address, existing };
        }
        types.append(object.type);
        m_analyzer.analyzeNode(object);
        m_worklist.append(object);
        return object;
    }

    void walkValue(const TargetObject& from, Address address, const TargetType& type, ObjectKind objectKind)
    {
        WTF::switchOn(type,
            [&](const TargetClass&) {
                walkClass(from, address, type, objectKind);
            },
            [&](const TargetPointer& pointer) {
                walkPointer(from, address, pointer);
            },
            [&](const TargetDeclaration&) {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const TargetOpaque&) { },
            [&](const TargetArray& array) {
                if (std::holds_alternative<TargetOpaque>(array.element))
                    return;
                uint64_t elementSize = byteSize(array.element);
                for (uint64_t i = 0; i < array.count; ++i)
                    walkValue(from, address + i * elementSize, array.element, ObjectKind::Complete);
            });
    }

    // A base subobject's virtual bases are walked from the complete object that holds them.
    void walkClass(const TargetObject& from, Address address, const TargetType& type, ObjectKind objectKind)
    {
        const TargetClass& targetClass = std::get<TargetClass>(type);
        if (targetClass.needsReader != TargetClass::NeedsReader::No) {
            m_analyzer.analyzeNeedsReader(from, { address, type });
            return;
        }
        for (const TargetMember& member : targetClass.members)
            walkValue(from, address + member.offset, member.type, member.kind == TargetMember::Kind::Base ? ObjectKind::Base : ObjectKind::Complete);
        if (objectKind == ObjectKind::Base)
            return;
        for (const TargetBase& base : targetClass.virtualBases)
            walkValue(from, address + base.offset, base.type, ObjectKind::Base);
    }

    void walkPointer(const TargetObject& from, Address address, const TargetPointer& pointer)
    {
        auto value = m_snapshot.memory().ptr<uint64_t>(address);
        if (!value) {
            CORPSE_REPORT("Could not read the pointer at 0x%llx", forReport(address));
            return;
        }
        if (!*value)
            return;
        const TargetType* pointee = m_debugInfo.pointee(pointer);
        if (!pointee)
            return;
        Address target { *value };
        WTF::switchOn(*pointee,
            [&](const TargetClass&) {
                TargetCanonicalPointer to = m_debugInfo.canonicalPointer(m_snapshot, target, *pointee);
                m_analyzer.analyzeEdge(from, { node(to.node), to.subobject });
            },
            [&](const TargetDeclaration& declaration) {
                m_declaredEdges.append({ from, target, declaration });
            },
            [&](const TargetOpaque& opaque) {
                m_analyzer.analyzeOpaqueEdge(from, address, opaque);
            },
            [&](const auto&) {
                TargetObject to = node({ target, *pointee });
                m_analyzer.analyzeEdge(from, { to, to });
            });
    }

    // The object a declared edge points to is a node if the walk reached it through an image that defines the class.
    void resolveDeclaredEdges()
    {
        HashMap<const TargetDeclaration*, Undefined> undefined;
        for (const DeclaredEdge& edge : m_declaredEdges) {
            if (std::optional<TargetCanonicalPointer> to = definedNodeAt(edge.to, edge.declaration)) {
                m_analyzer.analyzeEdge(edge.from, *to);
                continue;
            }
            Undefined& objects = undefined.add(&edge.declaration, Undefined { 0, edge.to }).iterator->value;
            ++objects.count;
        }
        for (auto& [declaration, objects] : undefined)
            CORPSE_REPORT("The class %s, which '%s' only declares, has %u objects, such as the one at 0x%llx, that the walk reached through no image that defines it", declaration->name.utf8().legacyCStringPointer(), declaration->image.utf8().legacyCStringPointer(), objects.count, forReport(objects.example));
    }

    std::optional<TargetCanonicalPointer> definedNodeAt(Address address, const TargetDeclaration& declaration) const
    {
        auto nodes = m_nodes.find(address);
        if (nodes == m_nodes.end())
            return std::nullopt;
        for (const TargetType& type : nodes->value) {
            if (!std::holds_alternative<TargetClass>(type))
                continue;
            if (const TargetType* definition = definitionOf(type, declaration))
                return TargetCanonicalPointer { { address, type }, { address, *definition } };
        }
        return std::nullopt;
    }

    Snapshot& m_snapshot;
    SnapshotDebugInfo& m_debugInfo;
    HeapAnalyzer& m_analyzer;
    HashMap<Address, Vector<TargetTypeRef, 1>> m_nodes;
    Vector<TargetObject> m_worklist;
    Vector<DeclaredEdge> m_declaredEdges;
};

} // anonymous namespace

void walkHeap(Snapshot& snapshot, SnapshotDebugInfo& debugInfo, const TargetObject& root, HeapAnalyzer& analyzer)
{
    CORPSE_DIAGNOSTICS(diagnostics, "walking the heap from the %s at 0x%llx", name(root.type).utf8().legacyCStringPointer(), forReport(root.address));
    HeapWalker(snapshot, debugInfo, analyzer).run(root);
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)

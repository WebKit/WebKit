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

#pragma once

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseTargetType.h>
#include <lldb/API/LLDB.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSCToolsTest {
class SnapshotDebugInfoTester;
}

namespace JSC {
namespace Corpse {

class Snapshot;

// The debug info of a snapshot's images, read through liblldb, which is never given a process.
class SnapshotDebugInfo : public RefCounted<SnapshotDebugInfo> {
    WTF_MAKE_TZONE_ALLOCATED(SnapshotDebugInfo);
public:
    // An image liblldb cannot open is reported and left out.
    static Ref<SnapshotDebugInfo> create(Snapshot&);
    ~SnapshotDebugInfo();

    // The types of the parameters of the function at `function`, as its image defines them. Empty, and
    // reported, when there is no such function or a parameter cannot be described.
    Vector<TargetTypeRef> parameterTypes(Address function);

    // What `pointer` points to, described the first time this is asked. Null, and reported that once, when it
    // cannot be described.
    const TargetType* pointee(const TargetPointer&);

    // The complete object that holds the `staticType` at `address`, as the image of its vtable defines it,
    // and that subobject of it. When the vtable or its class is not in the debug info, which is reported,
    // the object is read as its static type.
    TargetCanonicalPointer canonicalPointer(Snapshot&, Address, const TargetType& staticType);

private:
    friend class JSCToolsTest::SnapshotDebugInfoTester;

    // A class as one image defines it. liblldb's == is a type's identity, and the types it calls equal have
    // one name, size and members, which the hash is of. Member names tell apart the lambdas that share a name.
    struct ClassKey {
        ClassKey() = default;
        explicit ClassKey(lldb::SBType);

        lldb::SBType type;
        unsigned hash { 0 };
        bool isDeletedValue { false };
    };

    struct ClassKeyHash {
        static unsigned hash(const ClassKey& key) { return key.hash; }
        static bool equal(const ClassKey& a, const ClassKey& b)
        {
            lldb::SBType aType = a.type;
            lldb::SBType bType = b.type;
            return aType == bType;
        }
        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };

    struct ClassKeyHashTraits : WTF::GenericHashTraits<ClassKey> {
        static constexpr bool emptyValueIsZero = false;
        static constexpr bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const ClassKey& key) { return !key.isDeletedValue && !key.type.IsValid(); }
        static void constructDeletedValue(ClassKey& slot)
        {
            new (NotNull, &slot) ClassKey;
            slot.isDeletedValue = true;
        }
        static bool isDeletedValue(const ClassKey& key) { return key.isDeletedValue; }
    };

    struct Pointee {
        lldb::SBType type;
        lldb::SBModule module;
        bool isDescribed { false };
        const TargetType* described { nullptr };
    };

    SnapshotDebugInfo();

    // A declaration has no size and no fields. A class made of nothing but a flexible array has no size.
    static bool isDefinition(lldb::SBType&);

    // Null, and reported, when the type cannot be described. A class is described once per image. A class the
    // image only declares is a TargetDeclaration, which definedType() reports instead.
    const TargetType* valueType(lldb::SBType, const lldb::SBModule&);
    const TargetType* definedType(lldb::SBType, const lldb::SBModule&);
    const TargetType* classType(lldb::SBType, const lldb::SBModule&);
    std::unique_ptr<TargetType> createClass(lldb::SBType definition, const lldb::SBModule&);
    const TargetType& opaqueType(lldb::SBType, TargetOpaque::Kind);
    const TargetType& addType(std::unique_ptr<TargetType>&&);

    lldb::SBDebugger m_debugger;
    lldb::SBTarget m_target;
    HashMap<ClassKey, const TargetType*, ClassKeyHash, ClassKeyHashTraits> m_classes;
    HashMap<const TargetPointer*, Pointee> m_pointees;
    Vector<std::unique_ptr<TargetType>> m_types;
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)

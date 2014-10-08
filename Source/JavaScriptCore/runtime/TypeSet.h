/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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

#ifndef TypeSet_h
#define TypeSet_h

#include "StructureIDTable.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>
#include <wtf/Vector.h>

namespace Inspector {
namespace Protocol  {
template<typename T> class Array;

namespace Runtime {
class StructureDescription;
class TypeSet;
}

}
}

namespace JSC {

class JSValue;

enum RuntimeType {
    TypeNothing            = 0x0,
    TypeFunction           = 0x1,
    TypeUndefined          = 0x2,
    TypeNull               = 0x4,
    TypeBoolean            = 0x8,
    TypeMachineInt         = 0x10,
    TypeNumber             = 0x20,
    TypeString             = 0x40,
    TypeObject             = 0x80
};

class StructureShape : public RefCounted<StructureShape> {
    friend class TypeSet;

public:
    StructureShape();

    static PassRefPtr<StructureShape> create() { return adoptRef(new StructureShape); }
    String propertyHash();
    void markAsFinal();
    void addProperty(RefPtr<StringImpl>);
    String stringRepresentation();
    String toJSONString() const;
    PassRefPtr<Inspector::Protocol::Runtime::StructureDescription> inspectorRepresentation();
    void setConstructorName(String name) { m_constructorName = (name.isEmpty() ? "Object" : name); }
    String constructorName() { return m_constructorName; }
    void setProto(PassRefPtr<StructureShape> shape) { m_proto = shape; }
    void enterDictionaryMode();

private:
    static String leastCommonAncestor(const Vector<RefPtr<StructureShape>>);
    static PassRefPtr<StructureShape> merge(const PassRefPtr<StructureShape>, const PassRefPtr<StructureShape>);
    bool hasSamePrototypeChain(PassRefPtr<StructureShape>);

    HashSet<RefPtr<StringImpl>> m_fields;
    HashSet<RefPtr<StringImpl>> m_optionalFields;
    RefPtr<StructureShape> m_proto;
    std::unique_ptr<String> m_propertyHash;
    String m_constructorName;
    bool m_final;
    bool m_isInDictionaryMode;
};

class TypeSet : public RefCounted<TypeSet> {

public:
    static PassRefPtr<TypeSet> create() { return adoptRef(new TypeSet); }
    TypeSet();
    void addTypeInformation(RuntimeType, PassRefPtr<StructureShape>, StructureID);
    static RuntimeType getRuntimeTypeForValue(JSValue);
    void invalidateCache();
    JS_EXPORT_PRIVATE String seenTypes() const;
    String displayName() const;
    PassRefPtr<Inspector::Protocol::Array<String>> allPrimitiveTypeNames() const;
    PassRefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::StructureDescription>> allStructureRepresentations() const;
    String toJSONString() const;
    bool isOverflown() const { return m_isOverflown; }
    String leastCommonAncestor() const;
    PassRefPtr<Inspector::Protocol::Runtime::TypeSet> inspectorTypeSet() const;
    bool isEmpty() const { return m_seenTypes == TypeNothing; }

private:
    void dumpSeenTypes();
    bool doesTypeConformTo(uint32_t test) const;

    uint32_t m_seenTypes;
    bool m_isOverflown;
    Vector<RefPtr<StructureShape>> m_structureHistory;
    HashSet<StructureID> m_structureIDCache;
};

} //namespace JSC

#endif //TypeSet_h

/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "DumpContext.h"
#include <wtf/HashMap.h>
#include <wtf/PrintStream.h>

namespace JSC {

class JSGlobalObject;

namespace DFG {

class DesiredGlobalProperty {
public:
    DesiredGlobalProperty() = default;

    DesiredGlobalProperty(JSGlobalObject* globalObject, unsigned identifierNumber)
        : m_globalObject(globalObject)
        , m_identifierNumber(identifierNumber)
    {
    }

    DesiredGlobalProperty(WTF::HashTableDeletedValueType)
        : m_globalObject(nullptr)
        , m_identifierNumber(UINT_MAX)
    {
    }

    JSGlobalObject* globalObject() const { return m_globalObject; }
    unsigned identifierNumber() const { return m_identifierNumber; }

    friend bool operator==(const DesiredGlobalProperty&, const DesiredGlobalProperty&) = default;

    bool isHashTableDeletedValue() const
    {
        return !m_globalObject && m_identifierNumber == UINT_MAX;
    }

    unsigned hash() const
    {
        return WTF::PtrHash<JSGlobalObject*>::hash(m_globalObject) + m_identifierNumber * 7;
    }

    void dumpInContext(PrintStream& out, DumpContext*) const
    {
        out.print(m_identifierNumber, " for ", RawPointer(m_globalObject));
    }

    void dump(PrintStream& out) const
    {
        dumpInContext(out, nullptr);
    }

private:
    JSGlobalObject* m_globalObject { nullptr };
    unsigned m_identifierNumber { 0 };
};

struct DesiredGlobalPropertyHash {
    static unsigned hash(const DesiredGlobalProperty& key) { return key.hash(); }
    static bool equal(const DesiredGlobalProperty& a, const DesiredGlobalProperty& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::DFG

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::DFG::DesiredGlobalProperty> : JSC::DFG::DesiredGlobalPropertyHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::DFG::DesiredGlobalProperty> : SimpleClassHashTraits<JSC::DFG::DesiredGlobalProperty> { };

} // namespace WTF

#endif // ENABLE(DFG_JIT)

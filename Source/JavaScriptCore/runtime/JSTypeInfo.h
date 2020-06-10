// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

// This file would be called TypeInfo.h, but that conflicts with <typeinfo.h>
// in the STL on systems without case-sensitive file systems. 

#include "JSType.h"

namespace JSC {

class LLIntOffsetsExtractor;

// Inline flags.

static constexpr unsigned MasqueradesAsUndefined = 1; // WebCore uses MasqueradesAsUndefined to make document.all undetectable.
static constexpr unsigned ImplementsDefaultHasInstance = 1 << 1;
static constexpr unsigned OverridesGetCallData = 1 << 2; // Need this flag if you implement [[Callable]] interface, which means overriding getCallData. The object may not be callable since getCallData can say it is not callable.
static constexpr unsigned OverridesGetOwnPropertySlot = 1 << 3;
static constexpr unsigned OverridesToThis = 1 << 4; // If this is false then this returns something other than 'this'. Non-object cells that are visible to JS have this set as do some exotic objects.
static constexpr unsigned HasStaticPropertyTable = 1 << 5;
static constexpr unsigned TypeInfoPerCellBit = 1 << 7; // Unlike other inline flags, this will only be set on the cell itself and will not be set on the Structure.

// Out of line flags.

static constexpr unsigned ImplementsHasInstance = 1 << 8;
static constexpr unsigned OverridesGetPropertyNames = 1 << 9;
// OverridesAnyFormOfGetPropertyNames means that we cannot make assumptions about
// the cacheability or enumerability of property names, and therefore, we'll need
// to disable certain optimizations. This flag should be set if one or more of the
// following Object methods are overridden:
//     getOwnPropertyNames, getOwnNonIndexPropertyNames, getPropertyNames
static constexpr unsigned OverridesAnyFormOfGetPropertyNames = 1 << 10;
static constexpr unsigned ProhibitsPropertyCaching = 1 << 11;
static constexpr unsigned GetOwnPropertySlotIsImpure = 1 << 12;
static constexpr unsigned NewImpurePropertyFiresWatchpoints = 1 << 13;
static constexpr unsigned IsImmutablePrototypeExoticObject = 1 << 14;
static constexpr unsigned GetOwnPropertySlotIsImpureForPropertyAbsence = 1 << 15;
static constexpr unsigned InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero = 1 << 16;
static constexpr unsigned StructureIsImmortal = 1 << 17;
static constexpr unsigned HasPutPropertySecurityCheck = 1 << 18;

class TypeInfo {
public:
    typedef uint8_t InlineTypeFlags;
    typedef uint16_t OutOfLineTypeFlags;

    TypeInfo(JSType type, unsigned flags)
        : TypeInfo(type, flags & 0xff, flags >> 8)
    {
        ASSERT(!(flags >> 24));
    }

    TypeInfo(JSType type, InlineTypeFlags inlineTypeFlags, OutOfLineTypeFlags outOfLineTypeFlags)
        : m_type(type)
        , m_flags(inlineTypeFlags)
        , m_flags2(outOfLineTypeFlags)
    {
    }

    JSType type() const { return static_cast<JSType>(m_type); }
    bool isObject() const { return isObject(type()); }
    static bool isObject(JSType type) { return type >= ObjectType; }
    bool isFinalObject() const { return type() == FinalObjectType; }
    bool isNumberObject() const { return type() == NumberObjectType; }

    unsigned flags() const { return (static_cast<unsigned>(m_flags2) << 8) | static_cast<unsigned>(m_flags); }
    bool masqueradesAsUndefined() const { return isSetOnFlags1<MasqueradesAsUndefined>(); }
    bool implementsHasInstance() const { return isSetOnFlags2<ImplementsHasInstance>(); }
    bool implementsDefaultHasInstance() const { return isSetOnFlags1<ImplementsDefaultHasInstance>(); }
    bool overridesGetCallData() const { return isSetOnFlags1<OverridesGetCallData>(); }
    bool overridesGetOwnPropertySlot() const { return overridesGetOwnPropertySlot(inlineTypeFlags()); }
    static bool overridesGetOwnPropertySlot(InlineTypeFlags flags) { return flags & OverridesGetOwnPropertySlot; }
    static bool hasStaticPropertyTable(InlineTypeFlags flags) { return flags & HasStaticPropertyTable; }
    static bool perCellBit(InlineTypeFlags flags) { return flags & TypeInfoPerCellBit; }
    bool overridesToThis() const { return isSetOnFlags1<OverridesToThis>(); }
    bool structureIsImmortal() const { return isSetOnFlags2<StructureIsImmortal>(); }
    bool overridesGetPropertyNames() const { return isSetOnFlags2<OverridesGetPropertyNames>(); }
    bool overridesAnyFormOfGetPropertyNames() const { return isSetOnFlags2<OverridesAnyFormOfGetPropertyNames>(); }
    bool prohibitsPropertyCaching() const { return isSetOnFlags2<ProhibitsPropertyCaching>(); }
    bool getOwnPropertySlotIsImpure() const { return isSetOnFlags2<GetOwnPropertySlotIsImpure>(); }
    bool getOwnPropertySlotIsImpureForPropertyAbsence() const { return isSetOnFlags2<GetOwnPropertySlotIsImpureForPropertyAbsence>(); }
    bool hasPutPropertySecurityCheck() const { return isSetOnFlags2<HasPutPropertySecurityCheck>(); }
    bool newImpurePropertyFiresWatchpoints() const { return isSetOnFlags2<NewImpurePropertyFiresWatchpoints>(); }
    bool isImmutablePrototypeExoticObject() const { return isSetOnFlags2<IsImmutablePrototypeExoticObject>(); }
    bool interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero() const { return isSetOnFlags2<InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero>(); }

    static bool isArgumentsType(JSType type)
    {
        return type == DirectArgumentsType
            || type == ScopedArgumentsType
            || type == ClonedArgumentsType;
    }

    static ptrdiff_t flagsOffset()
    {
        return OBJECT_OFFSETOF(TypeInfo, m_flags);
    }

    static ptrdiff_t typeOffset()
    {
        return OBJECT_OFFSETOF(TypeInfo, m_type);
    }

    // Since the Structure doesn't track TypeInfoPerCellBit, we need to make sure we copy it.
    static InlineTypeFlags mergeInlineTypeFlags(InlineTypeFlags structureFlags, InlineTypeFlags oldCellFlags)
    {
        return structureFlags | (oldCellFlags & static_cast<InlineTypeFlags>(TypeInfoPerCellBit));
    }

    InlineTypeFlags inlineTypeFlags() const { return m_flags; }
    OutOfLineTypeFlags outOfLineTypeFlags() const { return m_flags2; }

private:
    friend class LLIntOffsetsExtractor;

    template<unsigned flag>
    bool isSetOnFlags1() const
    {
        static_assert(flag <= (1 << 7));
        return m_flags & flag;
    }

    template<unsigned flag>
    bool isSetOnFlags2() const
    {
        static_assert(flag >= (1 << 8) && flag <= (1 << 24));
        return m_flags2 & (flag >> 8);
    }

    JSType m_type;
    uint8_t m_flags;
    uint16_t m_flags2;
};

} // namespace JSC

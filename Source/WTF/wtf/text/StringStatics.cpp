/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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

#include "config.h"

#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC
#define ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "AtomicString.h"
#include "MainThread.h"
#include "NeverDestroyed.h"
#include "StaticConstructors.h"
#include "StringImpl.h"

#if USE(WEB_THREAD)
#include <pthread.h>
#endif

namespace WTF {

StringImpl* StringImpl::empty()
{
    static NeverDestroyed<StringImpl> emptyString(ConstructEmptyString);
    return &emptyString.get();
}

// Set the hash early, so that all empty unique StringImpls have a hash,
// and don't use the normal hashing algorithm - the unique nature of these
// keys means that we don't need them to match any other string (in fact,
// that's exactly the oposite of what we want!), and the normal hash would
// lead to lots of conflicts.
unsigned StringImpl::hashAndFlagsForEmptyUnique()
{
    static unsigned s_nextHashAndFlagsForEmptyUnique = BufferInternal | s_hashFlag8BitBuffer | s_hashFlagIsAtomic;
    s_nextHashAndFlagsForEmptyUnique += 1 << s_flagCount;
    s_nextHashAndFlagsForEmptyUnique |= 1 << 31;
    return s_nextHashAndFlagsForEmptyUnique;
}

WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, nullAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, emptyAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, textAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, commentAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, starAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, xmlAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, xmlnsAtom)
WTF_EXPORTDATA DEFINE_GLOBAL(AtomicString, xlinkAtom)

NEVER_INLINE unsigned StringImpl::hashSlowCase() const
{
    if (is8Bit())
        setHash(StringHasher::computeHashAndMaskTop8Bits(m_data8, m_length));
    else
        setHash(StringHasher::computeHashAndMaskTop8Bits(m_data16, m_length));
    return existingHash();
}

void AtomicString::init()
{
    static bool initialized;
    if (!initialized) {
        // Initialization is not thread safe, so this function must be called from the main thread first.
        ASSERT(isUIThread());

        // Use placement new to initialize the globals.
        new (NotNull, (void*)&nullAtom) AtomicString;
        new (NotNull, (void*)&emptyAtom) AtomicString("");
        new (NotNull, (void*)&textAtom) AtomicString("#text", AtomicString::ConstructFromLiteral);
        new (NotNull, (void*)&commentAtom) AtomicString("#comment", AtomicString::ConstructFromLiteral);
        new (NotNull, (void*)&starAtom) AtomicString("*", AtomicString::ConstructFromLiteral);
        new (NotNull, (void*)&xmlAtom) AtomicString("xml", AtomicString::ConstructFromLiteral);
        new (NotNull, (void*)&xmlnsAtom) AtomicString("xmlns", AtomicString::ConstructFromLiteral);
        new (NotNull, (void*)&xlinkAtom) AtomicString("xlink", AtomicString::ConstructFromLiteral);

        initialized = true;
    }
}

}

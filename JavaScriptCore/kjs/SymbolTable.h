/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SymbolTable_h
#define SymbolTable_h

#include "property_map.h"

namespace KJS {

    class JSValue;

    struct IdentifierRepHash {
        static unsigned hash(const KJS::UString::Rep *key) { return key->computedHash(); }
        static bool equal(const KJS::UString::Rep *a, const KJS::UString::Rep *b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };

    struct SymbolTableIndexHashTraits {
        typedef size_t TraitType;
        typedef SymbolTableIndexHashTraits StorageTraits;
        static size_t emptyValue() { return SIZE_T_MAX; }
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
        static const bool needsRef = false;
    };

    typedef HashMap<UString::Rep*, size_t, IdentifierRepHash, HashTraits<UString::Rep*>, SymbolTableIndexHashTraits> SymbolTable;

} // namespace KJS

#endif // SymbolTable_h

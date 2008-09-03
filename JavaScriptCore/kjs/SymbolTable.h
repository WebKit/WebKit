/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include "ustring.h"
#include <wtf/AlwaysInline.h>

namespace KJS {

    static ALWAYS_INLINE int missingSymbolMarker() { return std::numeric_limits<int>::max(); }

    // The bit twiddling in this class assumes that every register index is a
    // reasonably small negative number, and therefore has its high two bits set.

    struct SymbolTableEntry {
        SymbolTableEntry()
            : rawValue(0)
        {
        }
        
        SymbolTableEntry(int index)
        {
            ASSERT(index & 0x80000000);
            ASSERT(index & 0x40000000);

            rawValue = index & ~0x80000000 & ~0x40000000;
        }
        
        SymbolTableEntry(int index, unsigned attributes)
        {
            ASSERT(index & 0x80000000);
            ASSERT(index & 0x40000000);

            rawValue = index;
            
            if (!(attributes & ReadOnly))
                rawValue &= ~0x80000000;
            
            if (!(attributes & DontEnum))
                rawValue &= ~0x40000000;
        }

        bool isNull() const
        {
            return !rawValue;
        }

        int getIndex() const
        {
            ASSERT(!isNull());
            return rawValue | 0x80000000 | 0x40000000;
        }

        unsigned getAttributes() const
        {
            unsigned attributes = 0;
            
            if (rawValue & 0x80000000)
                attributes |= ReadOnly;
            
            if (rawValue & 0x40000000)
                attributes |= DontEnum;
            
            return attributes;
        }

        void setAttributes(unsigned attributes)
        {
            rawValue = getIndex();
            
            if (!(attributes & ReadOnly))
                rawValue &= ~0x80000000;
            
            if (!(attributes & DontEnum))
                rawValue &= ~0x40000000;
        }

        bool isReadOnly() const
        {
            return rawValue & 0x80000000;
        }

        int rawValue;
    };

    struct SymbolTableIndexHashTraits {
        typedef SymbolTableEntry TraitType;
        static SymbolTableEntry emptyValue() { return SymbolTableEntry(); }
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
    };

    typedef HashMap<RefPtr<UString::Rep>, SymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<UString::Rep> >, SymbolTableIndexHashTraits> SymbolTable;

} // namespace KJS

#endif // SymbolTable_h

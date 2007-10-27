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

    // SymbolTable is implemented in terms of a PropertyMap hack for now,
    // but it should move to WTF::HashMap once that's fast enough.

    class SymbolTable : private PropertyMap {
    public:
        void set(const Identifier& name, size_t index)
        {
            JSValue* v = reinterpret_cast<JSValue*>(index + 1); // Increment index by 1 because PropertyMap uses 0 to mean "not found."
            ASSERT(v); // Check for overflow.
            PropertyMap::put(name, v, 0, false);
        }

        bool get(const Identifier& name, size_t& index)
        {
            if (JSValue* v = PropertyMap::get(name)) {
                index = reinterpret_cast<uintptr_t>(v) - 1; // Decrement index by 1 to balance the increment in set.
                return true;
            }
            return false;
        }
    };

} // namespace KJS

#endif // SymbolTable_h

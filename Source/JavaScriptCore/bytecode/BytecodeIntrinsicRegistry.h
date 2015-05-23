/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#ifndef BytecodeIntrinsicRegistry_h
#define BytecodeIntrinsicRegistry_h

#include "Identifier.h"
#include <wtf/HashTable.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class CommonIdentifiers;
class BytecodeGenerator;
class BytecodeIntrinsicNode;
class RegisterID;

class BytecodeIntrinsicRegistry {
    WTF_MAKE_NONCOPYABLE(BytecodeIntrinsicRegistry);
public:
    explicit BytecodeIntrinsicRegistry(const CommonIdentifiers&);

    typedef RegisterID* (BytecodeIntrinsicNode::* EmitterType)(BytecodeGenerator&, RegisterID*);

    EmitterType lookup(const Identifier&) const;

private:
    const CommonIdentifiers& m_propertyNames;
    HashMap<RefPtr<UniquedStringImpl>, EmitterType, IdentifierRepHash> m_bytecodeIntrinsicMap;
};

} // namespace JSC

#endif // BytecodeIntrinsicRegistry_h

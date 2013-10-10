/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLThunks_h
#define FTLThunks_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FTLLocation.h"
#include "MacroAssemblerCodeRef.h"
#include <wtf/HashMap.h>

namespace JSC {

class VM;

namespace FTL {

MacroAssemblerCodeRef osrExitGenerationWithoutStackMapThunkGenerator(VM*);

MacroAssemblerCodeRef osrExitGenerationWithStackMapThunkGenerator(VM&, const Location&);

template<typename MapType, typename KeyType, typename GeneratorType>
MacroAssemblerCodeRef generateIfNecessary(
    VM& vm, MapType& map, const KeyType& key, GeneratorType generator)
{
    typename MapType::iterator iter = map.find(key);
    if (iter != map.end())
        return iter->value;
    
    MacroAssemblerCodeRef result = generator(vm, key);
    map.add(key, result);
    return result;
}

class Thunks {
public:
    MacroAssemblerCodeRef getOSRExitGenerationThunk(VM& vm, const Location& location)
    {
        return generateIfNecessary(
            vm, m_osrExitThunks, location, osrExitGenerationWithStackMapThunkGenerator);
    }
    
private:
    HashMap<Location, MacroAssemblerCodeRef> m_osrExitThunks;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLTHunks_h

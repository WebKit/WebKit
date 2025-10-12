/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "Disassembler.h"

#include "MacroAssemblerCodeRef.h"
#include <wtf/Condition.h>
#include <wtf/DataLog.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SystemFree.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace Disassembler {

Lock labelMapLock;

using LabelMap = UncheckedKeyHashMap<void*, Variant<CString, const char*>>;
LazyNeverDestroyed<LabelMap> labelMap;

static LabelMap& ensureLabelMap() WTF_REQUIRES_LOCK(labelMapLock)
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        labelMap.construct();
    });
    return labelMap.get();
}

} // namespace Disassembler

void disassemble(const CodePtr<DisassemblyPtrTag>& codePtr, size_t size, void* codeStart, void* codeEnd, const char* prefix, PrintStream& out)
{
    if (tryToDisassemble(codePtr, size, codeStart, codeEnd, prefix, out))
        return;
    
    out.printf("%sdisassembly not available for range %p...%p\n", prefix, codePtr.untaggedPtr(), codePtr.untaggedPtr<char*>() + size);
}

void registerLabel(void* thunkAddress, CString&& label)
{
    Locker lock { Disassembler::labelMapLock };
    Disassembler::ensureLabelMap().add(thunkAddress, WTFMove(label));
}

void registerLabel(void* address, const char* label)
{
    Locker lock { Disassembler::labelMapLock };
    Disassembler::ensureLabelMap().add(address, label);
}

const char* labelFor(void* thunkAddress)
{
    Locker lock { Disassembler::labelMapLock };
    auto& map = Disassembler::ensureLabelMap();
    auto it = map.find(thunkAddress);
    if (it == map.end())
        return nullptr;
    if (std::holds_alternative<CString>(it->value))
        return std::get<CString>(it->value).data();
    return std::get<const char*>(it->value);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

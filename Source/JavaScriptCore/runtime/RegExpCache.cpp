/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Renata Hodovan (hodovan@inf.u-szeged.hu)
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RegExpCache.h"

#include "StrongInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RegExpCache);

RegExp* RegExpCache::lookupOrCreate(VM& vm, const String& patternString, OptionSet<Yarr::Flags> flags)
{
    RegExpKey key(flags, patternString);
    if (RegExp* regExp = m_weakCache.get(key))
        return regExp;

    RegExp* regExp = RegExp::createWithoutCaching(vm, patternString, flags);
#if ENABLE(REGEXP_TRACING)
    vm.addRegExpToTrace(regExp);
#endif

    weakAdd(m_weakCache, key, Weak<RegExp>(regExp, this));
    return regExp;
}

RegExp* RegExpCache::ensureEmptyRegExpSlow(VM& vm)
{
    RegExp* regExp = RegExp::create(vm, emptyString(), { });
    m_emptyRegExp = regExp;
    return regExp;
}

void RegExpCache::finalize(Handle<Unknown> handle, void*)
{
    RegExp* regExp = static_cast<RegExp*>(handle.get().asCell());
    weakRemove(m_weakCache, regExp->key(), regExp);
}

void RegExpCache::addToStrongCache(RegExp* regExp)
{
    String pattern = regExp->pattern();
    if (pattern.length() > maxStrongCacheablePatternLength)
        return;

    m_strongCache[m_nextEntryInStrongCache] = regExp;
    m_nextEntryInStrongCache++;
    if (m_nextEntryInStrongCache == maxStrongCacheableEntries)
        m_nextEntryInStrongCache = 0;
}

void RegExpCache::deleteAllCode()
{
    m_strongCache.fill(nullptr);
    m_nextEntryInStrongCache = 0;

    for (auto& [key, weakHandle] : m_weakCache) {
        RegExp* regExp = weakHandle.get();
        if (!regExp) // Skip zombies.
            continue;
        regExp->deleteCode();
    }
}

template<typename Visitor>
void RegExpCache::visitAggregateImpl(Visitor& visitor)
{
    for (auto cell : m_strongCache)
        visitor.appendUnbarriered(cell);
    visitor.appendUnbarriered(m_emptyRegExp);
}
DEFINE_VISIT_AGGREGATE(RegExpCache);

}

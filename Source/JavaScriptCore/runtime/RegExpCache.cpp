/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Renata Hodovan (hodovan@inf.u-szeged.hu)
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
#include "RegExpObject.h"
#include "StrongInlines.h"

namespace JSC {

RegExp* RegExpCache::lookupOrCreate(const UString& patternString, RegExpFlags flags)
{
    RegExpKey key(flags, patternString);
    RegExpCacheMap::iterator result = m_weakCache.find(key);
    if (result != m_weakCache.end())
        return result->second.get();
    RegExp* regExp = RegExp::createWithoutCaching(*m_globalData, patternString, flags);
#if ENABLE(REGEXP_TRACING)
    m_globalData->addRegExpToTrace(regExp);
#endif
    // We need to do a second lookup to add the RegExp as
    // allocating it may have caused a gc cycle, which in
    // turn may have removed items from the cache.
    m_weakCache.add(key, PassWeak<RegExp>(regExp, this));
    return regExp;
}

RegExpCache::RegExpCache(JSGlobalData* globalData)
    : m_nextEntryInStrongCache(0)
    , m_globalData(globalData)
{
}

void RegExpCache::finalize(Handle<Unknown> handle, void*)
{
    RegExp* regExp = static_cast<RegExp*>(handle.get().asCell());
    m_weakCache.remove(regExp->key());
    regExp->invalidateCode();
}

void RegExpCache::addToStrongCache(RegExp* regExp)
{
    UString pattern = regExp->pattern();
    if (pattern.length() > maxStrongCacheablePatternLength)
        return;
    m_strongCache[m_nextEntryInStrongCache].set(*m_globalData, regExp);
    m_nextEntryInStrongCache++;
    if (m_nextEntryInStrongCache == maxStrongCacheableEntries)
        m_nextEntryInStrongCache = 0;
}

void RegExpCache::invalidateCode()
{
    for (int i = 0; i < maxStrongCacheableEntries; i++)
        m_strongCache[i].clear();
    m_nextEntryInStrongCache = 0;
    RegExpCacheMap::iterator end = m_weakCache.end();
    for (RegExpCacheMap::iterator ptr = m_weakCache.begin(); ptr != end; ++ptr)
        ptr->second->invalidateCode();
}

}

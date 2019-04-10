/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "CacheUpdate.h"

namespace JSC {

CacheUpdate::CacheUpdate(GlobalUpdate&& update)
    : m_update(WTFMove(update))
{
}

CacheUpdate::CacheUpdate(FunctionUpdate&& update)
    : m_update(WTFMove(update))
{
}

CacheUpdate::CacheUpdate(CacheUpdate&& other)
{
    if (WTF::holds_alternative<GlobalUpdate>(other.m_update))
        new (this) CacheUpdate(WTFMove(WTF::get<GlobalUpdate>(other.m_update)));
    else
        new (this) CacheUpdate(WTFMove(WTF::get<FunctionUpdate>(other.m_update)));
}

CacheUpdate& CacheUpdate::operator=(CacheUpdate&& other)
{
    this->~CacheUpdate();
    return *new (this) CacheUpdate(WTFMove(other));
}

bool CacheUpdate::isGlobal() const
{
    return WTF::holds_alternative<GlobalUpdate>(m_update);
}

const CacheUpdate::GlobalUpdate& CacheUpdate::asGlobal() const
{
    ASSERT(isGlobal());
    return WTF::get<GlobalUpdate>(m_update);
}

const CacheUpdate::FunctionUpdate& CacheUpdate::asFunction() const
{
    ASSERT(!isGlobal());
    return WTF::get<FunctionUpdate>(m_update);
}

} // namespace JSC

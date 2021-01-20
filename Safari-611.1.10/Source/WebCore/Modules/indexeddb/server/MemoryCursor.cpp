/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "MemoryCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBResourceIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace IDBServer {

static HashMap<IDBResourceIdentifier, MemoryCursor*>& cursorMap()
{
    static NeverDestroyed<HashMap<IDBResourceIdentifier, MemoryCursor*>> map;
    return map;
}

MemoryCursor::MemoryCursor(const IDBCursorInfo& info)
    : m_info(info)
{
    ASSERT(!cursorMap().contains(m_info.identifier()));
    cursorMap().set(m_info.identifier(), this);
}

MemoryCursor::~MemoryCursor()
{
    ASSERT(cursorMap().contains(m_info.identifier()));
    cursorMap().remove(m_info.identifier());
}

MemoryCursor* MemoryCursor::cursorForIdentifier(const IDBResourceIdentifier& identifier)
{
    return cursorMap().get(identifier);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

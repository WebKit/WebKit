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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "ExceptionCode.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

IDBCursor::IDBCursor()
{
}

const AtomicString& IDBCursor::directionNext()
{
    static NeverDestroyed<AtomicString> next("next", AtomicString::ConstructFromLiteral);
    return next;
}

const AtomicString& IDBCursor::directionNextUnique()
{
    static NeverDestroyed<AtomicString> nextunique("nextunique", AtomicString::ConstructFromLiteral);
    return nextunique;
}

const AtomicString& IDBCursor::directionPrev()
{
    static NeverDestroyed<AtomicString> prev("prev", AtomicString::ConstructFromLiteral);
    return prev;
}

const AtomicString& IDBCursor::directionPrevUnique()
{
    static NeverDestroyed<AtomicString> prevunique("prevunique", AtomicString::ConstructFromLiteral);
    return prevunique;
}

IndexedDB::CursorDirection IDBCursor::stringToDirection(const String& directionString, ExceptionCode& ec)
{
    if (directionString == IDBCursor::directionNext())
        return IndexedDB::CursorDirection::Next;
    if (directionString == IDBCursor::directionNextUnique())
        return IndexedDB::CursorDirection::NextNoDuplicate;
    if (directionString == IDBCursor::directionPrev())
        return IndexedDB::CursorDirection::Prev;
    if (directionString == IDBCursor::directionPrevUnique())
        return IndexedDB::CursorDirection::PrevNoDuplicate;

    ec = TypeError;
    return IndexedDB::CursorDirection::Next;
}

const AtomicString& IDBCursor::directionToString(IndexedDB::CursorDirection direction)
{
    switch (direction) {
    case IndexedDB::CursorDirection::Next:
        return IDBCursor::directionNext();

    case IndexedDB::CursorDirection::NextNoDuplicate:
        return IDBCursor::directionNextUnique();

    case IndexedDB::CursorDirection::Prev:
        return IDBCursor::directionPrev();

    case IndexedDB::CursorDirection::PrevNoDuplicate:
        return IDBCursor::directionPrevUnique();

    default:
        ASSERT_NOT_REACHED();
        return IDBCursor::directionNext();
    }
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

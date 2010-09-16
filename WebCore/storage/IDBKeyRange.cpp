/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "IDBKeyRange.h"

#include "IDBKey.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBKeyRange::IDBKeyRange(PassRefPtr<IDBKey> left, PassRefPtr<IDBKey> right, unsigned short flags)
    : m_left(left)
    , m_right(right)
    , m_flags(flags)
{
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(PassRefPtr<IDBKey> prpValue)
{
    RefPtr<IDBKey> value = prpValue;
    return IDBKeyRange::create(value, value, IDBKeyRange::SINGLE);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::leftBound(PassRefPtr<IDBKey> bound, bool open)
{
    unsigned short flags = IDBKeyRange::LEFT_BOUND;
    if (open)
        flags |= IDBKeyRange::LEFT_OPEN;
    return IDBKeyRange::create(bound, IDBKey::create(), flags);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::rightBound(PassRefPtr<IDBKey> bound, bool open)
{
    unsigned short flags = IDBKeyRange::RIGHT_BOUND;
    if (open)
        flags |= IDBKeyRange::RIGHT_OPEN;
    return IDBKeyRange::create(IDBKey::create(), bound, flags);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::bound(PassRefPtr<IDBKey> left, PassRefPtr<IDBKey> right, bool openLeft, bool openRight)
{
    unsigned short flags = IDBKeyRange::LEFT_BOUND | IDBKeyRange::RIGHT_BOUND;
    if (openLeft)
        flags |= IDBKeyRange::LEFT_OPEN;
    if (openRight)
        flags |= IDBKeyRange::RIGHT_OPEN;
    return IDBKeyRange::create(left, right, flags);
}

String IDBKeyRange::leftWhereClauseComparisonOperator() const
{
    if (m_flags & LEFT_OPEN)
        return "<";

    if (m_flags & LEFT_BOUND)
        return "<=";

    if (m_flags == SINGLE)
        return "=";

    ASSERT_NOT_REACHED();
    return "";
}

String IDBKeyRange::rightWhereClauseComparisonOperator() const
{
    if (m_flags & RIGHT_OPEN)
        return "<";

    if (m_flags & RIGHT_BOUND)
        return "<=";

    if (m_flags == SINGLE)
        return "=";

    ASSERT_NOT_REACHED();
    return "";
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

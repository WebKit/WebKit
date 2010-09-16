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

#ifndef IDBKeyRange_h
#define IDBKeyRange_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBKeyRange : public RefCounted<IDBKeyRange> {
public:
    // Keep in sync with what's in the .idl file.
    enum Flags {
        SINGLE = 0,
        LEFT_OPEN = 1,
        RIGHT_OPEN = 2,
        LEFT_BOUND = 4,
        RIGHT_BOUND = 8,
    };

    static PassRefPtr<IDBKeyRange> create(PassRefPtr<IDBKey> left, PassRefPtr<IDBKey> right, unsigned short flags)
    {
        return adoptRef(new IDBKeyRange(left, right, flags));
    }

    ~IDBKeyRange() { }


    PassRefPtr<IDBKey> left() const { return m_left; }
    PassRefPtr<IDBKey> right() const { return m_right; }
    unsigned short flags() const { return m_flags; }

    String leftWhereClauseComparisonOperator() const;
    String rightWhereClauseComparisonOperator() const;

    static PassRefPtr<IDBKeyRange> only(PassRefPtr<IDBKey> value);
    static PassRefPtr<IDBKeyRange> leftBound(PassRefPtr<IDBKey> bound, bool open = false);
    static PassRefPtr<IDBKeyRange> rightBound(PassRefPtr<IDBKey> bound, bool open = false);
    static PassRefPtr<IDBKeyRange> bound(PassRefPtr<IDBKey> left, PassRefPtr<IDBKey> right, bool openLeft = false, bool openRight = false);

private:
    IDBKeyRange(PassRefPtr<IDBKey> left, PassRefPtr<IDBKey> right, unsigned short flags);

    RefPtr<IDBKey> m_left;
    RefPtr<IDBKey> m_right;
    unsigned short m_flags;
};

} // namespace WebCore

#endif

#endif // IDBKeyRange_h

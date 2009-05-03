/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "JSPropertyNameIterator.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSPropertyNameIterator);

JSPropertyNameIterator::~JSPropertyNameIterator()
{
}

JSValue JSPropertyNameIterator::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    ASSERT_NOT_REACHED();
    return JSValue();
}

bool JSPropertyNameIterator::getPrimitiveNumber(ExecState*, double&, JSValue&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool JSPropertyNameIterator::toBoolean(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

double JSPropertyNameIterator::toNumber(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

UString JSPropertyNameIterator::toString(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return "";
}

JSObject* JSPropertyNameIterator::toObject(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void JSPropertyNameIterator::mark()
{
    JSCell::mark();
    if (m_object && !m_object->marked())
        m_object->mark();
}

void JSPropertyNameIterator::invalidate()
{
    ASSERT(m_position == m_end);
    m_object = 0;
    m_data.clear();
}

} // namespace JSC

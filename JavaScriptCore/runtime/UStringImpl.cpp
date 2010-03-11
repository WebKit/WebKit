/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "UStringImpl.h"

#include "Identifier.h"
#include "StdLibExtras.h"
#include "UString.h"
#include <wtf/unicode/UTF8.h>

using namespace WTF::Unicode;
using namespace std;

namespace JSC {

UStringImpl* UStringImpl::empty()
{
    // A non-null pointer at an invalid address (in page zero) so that if it were to be accessed we
    // should catch the error with fault (however it should be impossible to access, since length is zero).
    static const UChar* invalidNonNullUCharPtr = reinterpret_cast<UChar*>(static_cast<intptr_t>(1));
    DEFINE_STATIC_LOCAL(UStringImpl, emptyString, (invalidNonNullUCharPtr, 0, ConstructStaticString));
    return &emptyString;
}

PassRefPtr<UStringImpl> UStringImpl::create(const UChar* characters, unsigned length)
{
    if (!characters || !length)
        return empty();

    UChar* data;
    PassRefPtr<UStringImpl> string = createUninitialized(length, data);
    memcpy(data, characters, length * sizeof(UChar));
    return string;
}

PassRefPtr<UStringImpl> UStringImpl::create(const char* characters, unsigned length)
{
    if (!characters || !length)
        return empty();

    UChar* data;
    PassRefPtr<UStringImpl> string = createUninitialized(length, data);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    return string;
}

PassRefPtr<UStringImpl> UStringImpl::create(const char* string)
{
    if (!string)
        return empty();
    return create(string, strlen(string));
}

SharedUChar* UStringImpl::baseSharedBuffer()
{
    ASSERT((bufferOwnership() == BufferShared)
        || ((bufferOwnership() == BufferOwned) && !m_buffer));

    if (bufferOwnership() != BufferShared) {
        m_refCountAndFlags = (m_refCountAndFlags & ~s_refCountMaskBufferOwnership) | BufferShared;
        m_bufferShared = SharedUChar::create(new SharableUChar(m_data)).releaseRef();
    }

    return m_bufferShared;
}

SharedUChar* UStringImpl::sharedBuffer()
{
    if (m_length < s_minLengthToShare)
        return 0;
    ASSERT(!isStatic());

    UStringImpl* owner = bufferOwnerString();
    if (owner->bufferOwnership() == BufferInternal)
        return 0;

    return owner->baseSharedBuffer();
}

UStringImpl::~UStringImpl()
{
    ASSERT(!isStatic());
    checkConsistency();

    if (isIdentifier())
        Identifier::remove(this);

    if (bufferOwnership() != BufferInternal) {
        if (bufferOwnership() == BufferOwned)
            fastFree(const_cast<UChar*>(m_data));
        else if (bufferOwnership() == BufferSubstring)
            m_bufferSubstring->deref();
        else {
            ASSERT(bufferOwnership() == BufferShared);
            m_bufferShared->deref();
        }
    }
}

void URopeImpl::derefFibersNonRecursive(Vector<URopeImpl*, 32>& workQueue)
{
    unsigned length = fiberCount();
    for (unsigned i = 0; i < length; ++i) {
        Fiber& fiber = fibers(i);
        if (fiber->isRope()) {
            URopeImpl* nextRope = static_cast<URopeImpl*>(fiber);
            if (nextRope->hasOneRef())
                workQueue.append(nextRope);
            else
                nextRope->deref();
        } else
            static_cast<UStringImpl*>(fiber)->deref();
    }
}

void URopeImpl::destructNonRecursive()
{
    Vector<URopeImpl*, 32> workQueue;

    derefFibersNonRecursive(workQueue);
    delete this;

    while (!workQueue.isEmpty()) {
        URopeImpl* rope = workQueue.last();
        workQueue.removeLast();
        rope->derefFibersNonRecursive(workQueue);
        delete rope;
    }
}

} // namespace JSC

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
#include "UString.h"
#include <wtf/unicode/UTF8.h>

using namespace WTF::Unicode;
using namespace std;

namespace JSC {

PassRefPtr<UStringImpl> UStringImpl::create(const char* c)
{
    ASSERT(c);

    if (!c[0])
        return &UStringImpl::empty();

    size_t length = strlen(c);
    UChar* d;
    PassRefPtr<UStringImpl> result = UStringImpl::createUninitialized(length, d);
    for (size_t i = 0; i < length; i++)
        d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
    return result;
}

PassRefPtr<UStringImpl> UStringImpl::create(const char* c, unsigned length)
{
    ASSERT(c);

    if (!length)
        return &UStringImpl::empty();

    UChar* d;
    PassRefPtr<UStringImpl> result = UStringImpl::createUninitialized(length, d);
    for (unsigned i = 0; i < length; i++)
        d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
    return result;
}

PassRefPtr<UStringImpl> UStringImpl::create(const UChar* buffer, unsigned length)
{
    UChar* newBuffer;
    PassRefPtr<UStringImpl> impl = createUninitialized(length, newBuffer);
    copyChars(newBuffer, buffer, length);
    return impl;
}

SharedUChar* UStringImpl::baseSharedBuffer()
{
    ASSERT((bufferOwnership() == BufferShared)
        || ((bufferOwnership() == BufferOwned) && !m_buffer));

    if (bufferOwnership() != BufferShared) {
        m_refCountAndFlags = (m_refCountAndFlags & ~s_refCountMaskBufferOwnership) | BufferShared;
        m_bufferShared = SharedUChar::create(new OwnFastMallocPtr<UChar>(m_data)).releaseRef();
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
            fastFree(m_data);
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

PassRefPtr<UStringImpl> singleCharacterSubstring(UStringOrRopeImpl* impl, unsigned index)
{
top:
    if (impl->isRope()) {
        URopeImpl* rope = static_cast<URopeImpl*>(impl);
        for (unsigned i = 0; i < rope->m_fiberCount; ++i) {
            UStringOrRopeImpl* currentFiber = rope->fibers(i);
            unsigned fiberLength = currentFiber->length();
            if (index < fiberLength) {
                impl = currentFiber;
                goto top;
            }
            index -= fiberLength;
        }
        CRASH();
    }

    return static_cast<UStringImpl*>(impl)->singleCharacterSubstring(index);
}

} // namespace JSC

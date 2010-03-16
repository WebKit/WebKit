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

static const unsigned minLengthToShare = 20;

UStringImpl::~UStringImpl()
{
    ASSERT(!isStatic());

    if (isIdentifier())
        Identifier::remove(this);

    BufferOwnership ownership = bufferOwnership();
    if (ownership != BufferInternal) {
        if (ownership == BufferOwned) {
            ASSERT(!m_sharedBuffer);
            ASSERT(m_data);
            fastFree(const_cast<UChar*>(m_data));
        } else if (ownership == BufferSubstring) {
            ASSERT(m_substringBuffer);
            m_substringBuffer->deref();
        } else {
            ASSERT(ownership == BufferShared);
            ASSERT(m_sharedBuffer);
            m_sharedBuffer->deref();
        }
    }
}

UStringImpl* UStringImpl::empty()
{
    // FIXME: This works around a bug in our port of PCRE, that a regular expression
    // run on the empty string may still perform a read from the first element, and
    // as such we need this to be a valid pointer. No code should ever be reading
    // from a zero length string, so this should be able to be a non-null pointer
    // into the zero-page.
    // Replace this with 'reinterpret_cast<UChar*>(static_cast<intptr_t>(1))' once
    // PCRE goes away.
    static UChar emptyUCharData = 0;
    DEFINE_STATIC_LOCAL(UStringImpl, emptyString, (&emptyUCharData, 0, ConstructStaticString));
    return &emptyString;
}

PassRefPtr<UStringImpl> UStringImpl::createUninitialized(unsigned length, UChar*& data)
{
    if (!length) {
        data = 0;
        return empty();
    }

    // Allocate a single buffer large enough to contain the StringImpl
    // struct as well as the data which it contains. This removes one 
    // heap allocation from this call.
    if (length > ((std::numeric_limits<size_t>::max() - sizeof(UStringImpl)) / sizeof(UChar)))
        CRASH();
    size_t size = sizeof(UStringImpl) + length * sizeof(UChar);
    UStringImpl* string = static_cast<UStringImpl*>(fastMalloc(size));

    data = reinterpret_cast<UChar*>(string + 1);
    return adoptRef(new (string) UStringImpl(length));
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

PassRefPtr<UStringImpl> UStringImpl::create(PassRefPtr<SharedUChar> sharedBuffer, const UChar* buffer, unsigned length)
{
    if (!length)
        return empty();
    return adoptRef(new UStringImpl(buffer, length, sharedBuffer));
}

SharedUChar* UStringImpl::sharedBuffer()
{
    if (m_length < minLengthToShare)
        return 0;
    // All static strings are smaller that the minimim length to share.
    ASSERT(!isStatic());

    BufferOwnership ownership = bufferOwnership();

    if (ownership == BufferInternal)
        return 0;
    if (ownership == BufferSubstring)
        return m_substringBuffer->sharedBuffer();
    if (ownership == BufferOwned) {
        ASSERT(!m_sharedBuffer);
        m_sharedBuffer = SharedUChar::create(new SharableUChar(m_data)).releaseRef();
        m_refCountAndFlags = (m_refCountAndFlags & ~s_refCountMaskBufferOwnership) | BufferShared;
    }

    ASSERT(bufferOwnership() == BufferShared);
    ASSERT(m_sharedBuffer);
    return m_sharedBuffer;
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

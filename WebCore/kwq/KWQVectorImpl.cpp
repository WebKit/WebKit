/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <string.h>
#include "KWQVectorImpl.h"

KWQVectorImpl::KWQVectorImpl(void (*f)(void *))
    : m_data(0), m_size(0), m_count(0), m_deleteItemFunction(f)
{
}

KWQVectorImpl::KWQVectorImpl(uint size, void (*f)(void *))
    : m_data((void **)fastMalloc(size * sizeof(void *)))
    , m_size(size), m_count(0), m_deleteItemFunction(f)
{
    memset(m_data, 0, size * sizeof(void *));
}

KWQVectorImpl::KWQVectorImpl(const KWQVectorImpl &vi)
    : m_data(vi.m_data ? (void **)fastMalloc(vi.m_size * sizeof(void *)) : 0)
    , m_size(vi.m_size), m_count(vi.m_count)
    , m_deleteItemFunction(vi.m_deleteItemFunction)
{
    memcpy(m_data, vi.m_data, vi.m_size * sizeof(void *));
}

KWQVectorImpl::~KWQVectorImpl()
{
    fastFree(m_data);
}

void KWQVectorImpl::clear(bool delItems)
{
    if (delItems) {
	for (uint i = 0; i < m_size; ++i) {
            void *item = m_data[i];
	    if (item) {
		m_deleteItemFunction(item);
	    }
	}
    }

    fastFree(m_data);
    m_data = 0;
    m_size = 0;
    m_count = 0;
}

bool KWQVectorImpl::remove(uint n, bool delItems)
{
    if (n >= m_size) {
        return false;
    }
    
    void *item = m_data[n];
    if (item) {
        if (delItems) {
            m_deleteItemFunction(item);
        }
        --m_count;
    }
    m_data[n] = 0;
    return true;
}

bool KWQVectorImpl::resize(uint size, bool delItems)
{
    uint oldSize = m_size;
    m_size = size;
    
    for (uint i = size; i < oldSize; ++i) {
        void *item = m_data[i];
        if (item) {
            if (delItems) {
                m_deleteItemFunction(item);
            }
            --m_count;
        }
    }

    m_size = size;
    m_data = (void **)fastRealloc(m_data, size * sizeof(void *));

    if (size > oldSize) {
        memset(&m_data[oldSize], 0, (size - oldSize) * sizeof(void *));
    }

    return true;
}

bool KWQVectorImpl::insert(uint n, void *item, bool delItems)
{
    if (n >= m_size) {
	return false;
    }

    void *oldItem = m_data[n];
    if (oldItem) {
        if (delItems) {
            m_deleteItemFunction(oldItem);
        }
        --m_count;
    }
    
    m_data[n] = item;
    if (item) {
        ++m_count;
    }
    
    return true;
}

int KWQVectorImpl::findRef(void *item)
{
    for (unsigned i = 0; i < m_count; i++) {
        if (m_data[i] == item) {
            return i;
        }
    }
    
    return -1;
}


KWQVectorImpl &KWQVectorImpl::assign(KWQVectorImpl &vi, bool delItems)
{
    clear(delItems);
    
    m_data = vi.m_data ? (void **)fastMalloc(vi.m_size * sizeof(void *)) : 0;
    m_size = vi.m_size;
    m_count = vi.m_count;
    m_deleteItemFunction = vi.m_deleteItemFunction;

    memcpy(m_data, vi.m_data, vi.m_size * sizeof(void *));
    
    return *this;
}

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

#include "KWQAccObjectCache.h"
#include "KWQAccObject.h"

// The simple Cocoa calls in this file can't throw.

KWQAccObjectCache::KWQAccObjectCache()
{
    accCache = NULL;
}

KWQAccObjectCache::~KWQAccObjectCache()
{
    // Destroy the dictionary.
    CFRelease(accCache);
}

KWQAccObject* KWQAccObjectCache::accObject(khtml::RenderObject* renderer)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, renderer);
    if (!obj) {
        obj = [[KWQAccObject alloc] initWithRenderer: renderer]; // Initial ref happens here.
        setAccObject(renderer, obj);
    }

    return obj;
}

void KWQAccObjectCache::setAccObject(khtml::RenderObject* impl, KWQAccObject* accObject)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionarySetValue(accCache, (const void *)impl, accObject);
}

void KWQAccObjectCache::removeAccObject(khtml::RenderObject* impl)
{
    if (!accCache)
        return;

    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, impl);
    if (obj) {
        [obj detach];
        [obj release];
        CFDictionaryRemoveValue(accCache, impl);
    }
}

void KWQAccObjectCache::detach(khtml::RenderObject* renderer)
{
    removeAccObject(renderer);
}

void KWQAccObjectCache::childrenChanged(khtml::RenderObject* renderer)
{
    if (!accCache)
        return;
    
    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, renderer);
    if (!obj)
        return;
    
    [obj childrenChanged];
}

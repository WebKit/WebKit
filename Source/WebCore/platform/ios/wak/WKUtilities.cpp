/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKUtilities.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/Assertions.h>

const CFArrayCallBacks WKCollectionArrayCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL };
const CFSetCallBacks WKCollectionSetCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL, NULL };

const void *WKCollectionRetain(CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    return WAKRetain(value);
}

const void *WAKRetain(const void *o)
{
    WAKObjectRef object = (WAKObjectRef)(uintptr_t)o;
    
    object->referenceCount++;
    
    return object;
}

void WKCollectionRelease(CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    WAKRelease (value);
}

void WAKRelease(const void *o)
{
    WAKObjectRef object = (WAKObjectRef)(uintptr_t)o;

    if (!object->referenceCount) {
        WTFLogAlways("WAKRelease: attempt to release invalid object");
        return;
    }
    
    object->referenceCount--;

    if (!object->referenceCount) {
        const WKClassInfo *info = object->classInfo;
        while (info) {
            if (info->dealloc)
                info->dealloc(object);
            info = info->parent;
        }
    }
}

static void WAKObjectDealloc(WAKObjectRef v)
{
    free(v);
}

WKClassInfo WAKObjectClass = { 0, "WAKObject", WAKObjectDealloc };

const void *WKCreateObjectWithSize(size_t size, WKClassInfo *info)
{
    WAKObjectRef object = (WAKObjectRef)calloc(size, 1);
    if (!object)
        return 0;

    object->classInfo = info;
    
    WAKRetain(object);
    
    return object;
}

CFIndex WKArrayIndexOfValue(CFArrayRef array, const void *value)
{
    CFIndex i, count, index = -1;

    count = CFArrayGetCount(array);
    for (i = 0; i < count; i++) {
        if (CFArrayGetValueAtIndex(array, i) == value) {
            index = i;
            break;
        }
    }
    
    return index;
}

WKClassInfo *WKGetClassInfo(WAKObjectRef object)
{
    return object->classInfo;
}

#endif // PLATFORM(IOS_FAMILY)

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

#if PLATFORM(IOS)

#import <wtf/Assertions.h>

const CFArrayCallBacks WKCollectionArrayCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL };
const CFSetCallBacks WKCollectionSetCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL, NULL };

const void *WKCollectionRetain (CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    return WKRetain (value);
}

const void *WKRetain(const void *o)
{
    WKObjectRef object = (WKObjectRef)o;
    
    object->referenceCount++;
    
    return object;
}

void WKCollectionRelease (CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    WKRelease (value);
}

void WKRelease(const void *o)
{
    WKObjectRef object = (WKObjectRef)o;

    if (object->referenceCount == 0) {
        WKError ("attempt to release invalid object");
        return;
    }
    
    object->referenceCount--;

    if (object->referenceCount == 0) {
        const WKClassInfo *info = object->classInfo;
        while (info) {
            if (info->dealloc)
                info->dealloc ((void *)object);
            info = info->parent;
        }
    }
}

static void _WKObjectDealloc (WKObjectRef v)
{
    free (v);
}

WKClassInfo WKObjectClass = { 0, "WKObject", _WKObjectDealloc };

const void *WKCreateObjectWithSize (size_t size, WKClassInfo *info)
{
    WKObjectRef object = (WKObjectRef)calloc (size, 1);
    if (!object)
        return 0;

    object->classInfo = info;
    
    WKRetain(object);
    
    return object;
}

WTF_ATTRIBUTE_PRINTF(4, 5)
void WKReportError(const char *file, int line, const char *function, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d %s:  ", file, line, function);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

CFIndex WKArrayIndexOfValue (CFArrayRef array, const void *value)
{
    CFIndex i, count, index = -1;

    count = CFArrayGetCount (array);
    for (i = 0; i < count; i++) {
        if (CFArrayGetValueAtIndex (array, i) == value) {
            index = i;
            break;
        }
    }
    
    return index;
}

WKClassInfo *WKGetClassInfo (WKObjectRef object)
{
    return object->classInfo;
}

#endif // PLATFORM(IOS)

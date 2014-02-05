/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef MarkStack_h
#define MarkStack_h

#if ENABLE(OBJECT_MARK_LOGGING)
#define MARK_LOG_MESSAGE0(message) dataLogF(message)
#define MARK_LOG_MESSAGE1(message, arg1) dataLogF(message, arg1)
#define MARK_LOG_MESSAGE2(message, arg1, arg2) dataLogF(message, arg1, arg2)
#define MARK_LOG_ROOT(visitor, rootName) \
    dataLogF("\n%s: ", rootName); \
    (visitor).resetChildCount()
#define MARK_LOG_PARENT(visitor, parent) \
    dataLogF("\n%p (%s): ", parent, parent->className() ? parent->className() : "unknown"); \
    (visitor).resetChildCount()
#define MARK_LOG_CHILD(visitor, child) \
    if ((visitor).childCount()) \
    dataLogFString(", "); \
    dataLogF("%p", child); \
    (visitor).incrementChildCount()
#else
#define MARK_LOG_MESSAGE0(message) do { } while (false)
#define MARK_LOG_MESSAGE1(message, arg1) do { } while (false)
#define MARK_LOG_MESSAGE2(message, arg1, arg2) do { } while (false)
#define MARK_LOG_ROOT(visitor, rootName) do { } while (false)
#define MARK_LOG_PARENT(visitor, parent) do { } while (false)
#define MARK_LOG_CHILD(visitor, child) do { } while (false)
#endif

#include "GCSegmentedArrayInlines.h"

namespace JSC {

class JSCell;

class MarkStackArray : public GCSegmentedArray<const JSCell*> {
public:
    MarkStackArray(BlockAllocator&);

    void donateSomeCellsTo(MarkStackArray& other);
    void stealSomeCellsFrom(MarkStackArray& other, size_t idleThreadCount);
};

} // namespace JSC

#endif

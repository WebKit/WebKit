/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/Handle.h>

namespace JSC {

class AbstractSlotVisitor;

class JS_EXPORT_PRIVATE WeakHandleOwner {
public:
    enum class FinalizeResult : bool {
        // If the WeakImpl is managed by JSC::Weak, this result should always be returned.
        KeepImpl,
        DeallocateImpl,
    };

    WeakHandleOwner() = default;
    virtual ~WeakHandleOwner();
    // FIXME: These should take a JSValue. The Handle<Unknown> they're passed points at a copy, not at the WeakImpl.
    // reason will only be non-null when generating a debug GC heap snapshot.
    virtual bool isReachableFromOpaqueRoots(Handle<Unknown>, void* context, AbstractSlotVisitor&, ASCIILiteral* reason = nullptr);

    // The handle is already dead when finalize is called. Thus, it must follow the same rules as any `destroy`: no entering into
    // JS, allocating GC objects, no looking at GC member values. The last one includes Structure, which means `downcast`/`is`
    // cannot be used, `static_cast` should be used instead.
    // A finalizer may clear other handles only on the mutator, never when WeakSet::finalizeDeadHandles() runs it on another thread.
    virtual FinalizeResult finalize(Handle<Unknown>, void* context);

private:
    explicit WeakHandleOwner(ClangVTableWorkaroundTag);
};

} // namespace JSC

/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)
#include "JSTextTrackCustom.h"

#include "JSTextTrack.h"
#include "JSTextTrackCueList.h"

using namespace JSC;

namespace WebCore {

bool JSTextTrackOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSTextTrack* jsTextTrack = jsCast<JSTextTrack*>(handle.get().asCell());
    TextTrack* textTrack = static_cast<TextTrack*>(jsTextTrack->impl());

    // If the cue is firing event listeners, its wrapper is reachable because
    // the wrapper is responsible for marking those event listeners.
    if (textTrack->isFiringEventListeners())
        return true;

    // If the cue has no event listeners and has no custom properties, it is not reachable.
    if (!textTrack->hasEventListeners() && !jsTextTrack->hasCustomProperties())
        return false;

    return visitor.containsOpaqueRoot(root(textTrack));
}

void JSTextTrack::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSTextTrack* jsTextTrack = jsCast<JSTextTrack*>(cell);
    ASSERT_GC_OBJECT_INHERITS(jsTextTrack, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(jsTextTrack->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(jsTextTrack, visitor);

    TextTrack* textTrack = static_cast<TextTrack*>(jsTextTrack->impl());
    visitor.addOpaqueRoot(root(textTrack));

    textTrack->visitJSEventListeners(visitor);
}

} // namespace WebCore

#endif

/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "JSTextTrackCue.h"

#include "JSDataCue.h"
#include "JSTrackCustom.h"
#include "JSVTTCue.h"
#include "TextTrack.h"


namespace WebCore {
using namespace JSC;

bool JSTextTrackCueOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, const char** reason)
{
    JSTextTrackCue* jsTextTrackCue = jsCast<JSTextTrackCue*>(handle.slot()->asCell());
    TextTrackCue& textTrackCue = jsTextTrackCue->wrapped();

    if (!textTrackCue.isContextStopped() && textTrackCue.hasPendingActivity()) {
        if (UNLIKELY(reason))
            *reason = "TextTrackCue with pending activity";
        return true;
    }

    // If the cue is not associated with a track, it is not reachable.
    if (!textTrackCue.track())
        return false;

    if (UNLIKELY(reason))
        *reason = "TextTrack is an opaque root";

    return visitor.containsOpaqueRoot(root(textTrackCue.track()));
}

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<TextTrackCue>&& cue)
{
    switch (cue->cueType()) {
    case TextTrackCue::Data:
        return createWrapper<DataCue>(globalObject, WTFMove(cue));
    case TextTrackCue::WebVTT:
    case TextTrackCue::ConvertedToWebVTT:
        return createWrapper<VTTCue>(globalObject, WTFMove(cue));
    case TextTrackCue::Generic:
        return createWrapper<TextTrackCue>(globalObject, WTFMove(cue));
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, TextTrackCue& cue)
{
    return wrap(lexicalGlobalObject, globalObject, cue);
}

template<typename Visitor>
void JSTextTrackCue::visitAdditionalChildren(Visitor& visitor)
{
    if (TextTrack* textTrack = wrapped().track())
        visitor.addOpaqueRoot(root(textTrack));
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSTextTrackCue);

} // namespace WebCore

#endif

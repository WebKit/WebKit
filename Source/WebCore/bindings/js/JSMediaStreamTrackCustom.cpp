/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSMediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "MediaSourceSettings.h"
#include <runtime/JSObject.h>
#include <runtime/ObjectConstructor.h>

using namespace JSC;

namespace WebCore {

JSC::JSValue JSMediaStreamTrack::getSettings(ExecState& state)
{
    JSValue thisValue = state.thisValue();
    JSMediaStreamTrack* castedThis = jsDynamicCast<JSMediaStreamTrack*>(thisValue);
    if (UNLIKELY(!castedThis))
        return JSValue::decode(throwThisTypeError(state, "MediaStreamTrack", "getSettings"));

    JSObject* object = constructEmptyObject(&state);
    auto& impl = castedThis->wrapped();
    RefPtr<MediaSourceSettings> settings = WTF::getPtr(impl.getSettings());

    if (settings->supportsWidth())
        object->putDirect(state.vm(), Identifier::fromString(&state, "width"), jsNumber(settings->width()), DontDelete | ReadOnly);
    if (settings->supportsHeight())
        object->putDirect(state.vm(), Identifier::fromString(&state, "height"), jsNumber(settings->height()), DontDelete | ReadOnly);
    if (settings->supportsAspectRatio())
        object->putDirect(state.vm(), Identifier::fromString(&state, "aspectRatio"), jsDoubleNumber(settings->aspectRatio()), DontDelete | ReadOnly);
    if (settings->supportsFrameRate())
        object->putDirect(state.vm(), Identifier::fromString(&state, "frameRate"), jsDoubleNumber(settings->frameRate()), DontDelete | ReadOnly);
    if (settings->supportsFacingMode())
        object->putDirect(state.vm(), Identifier::fromString(&state, "facingMode"), jsStringWithCache(&state, settings->facingMode()), DontDelete | ReadOnly);
    if (settings->supportsVolume())
        object->putDirect(state.vm(), Identifier::fromString(&state, "volume"), jsDoubleNumber(settings->volume()), DontDelete | ReadOnly);
    if (settings->supportsSampleRate())
        object->putDirect(state.vm(), Identifier::fromString(&state, "sampleRate"), jsNumber(settings->sampleRate()), DontDelete | ReadOnly);
    if (settings->supportsSampleSize())
        object->putDirect(state.vm(), Identifier::fromString(&state, "sampleSize"), jsNumber(settings->sampleSize()), DontDelete | ReadOnly);
    if (settings->supportsEchoCancellation())
        object->putDirect(state.vm(), Identifier::fromString(&state, "echoCancellation"), jsBoolean(settings->echoCancellation()), DontDelete | ReadOnly);
    if (settings->supportsDeviceId())
        object->putDirect(state.vm(), Identifier::fromString(&state, "deviceId"), jsStringWithCache(&state, settings->deviceId()), DontDelete | ReadOnly);
    if (settings->supportsGroupId())
        object->putDirect(state.vm(), Identifier::fromString(&state, "groupId"), jsStringWithCache(&state, settings->groupId()), DontDelete | ReadOnly);
    
    return object;
}

} // namespace WebCore

#endif

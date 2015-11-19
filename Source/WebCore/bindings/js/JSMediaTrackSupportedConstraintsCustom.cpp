/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "JSMediaTrackSupportedConstraints.h"

#if ENABLE(MEDIA_STREAM)

#include "JSDOMBinding.h"
#include "JSDOMWrapper.h"
#include "RealtimeMediaSourceCenter.h"

using namespace JSC;

namespace WebCore {

#define FOR_EACH_CONSTRAINT_PROPERTY(macro) \
    macro(width, Width) \
    macro(height, Height) \
    macro(aspectRatio, AspectRatio) \
    macro(frameRate, FrameRate) \
    macro(facingMode, FacingMode) \
    macro(volume, Volume) \
    macro(sampleRate, SampleRate) \
    macro(sampleSize, SampleSize) \
    macro(echoCancellation, EchoCancellation) \
    macro(deviceId, DeviceId) \
    macro(groupId, GroupId)

bool JSMediaTrackSupportedConstraints::getOwnPropertySlotDelegate(ExecState*, PropertyName propertyName, PropertySlot& slot)
{
#define CHECK_PROPERTY(lowerName, capitalName) \
    if (AtomicString(#lowerName, AtomicString::ConstructFromLiteral) == propertyNameToAtomicString(propertyName)) { \
        slot.setValue(this, DontDelete | ReadOnly, jsBoolean(wrapped().supports##capitalName())); \
        return true; \
    }

    FOR_EACH_CONSTRAINT_PROPERTY(CHECK_PROPERTY)

    return false;
}

void JSMediaTrackSupportedConstraints::getOwnPropertyNames(JSObject* object, ExecState* state, PropertyNameArray& propertyNames, EnumerationMode mode)
{
#define MAYBE_ADD_PROPERTY(lowerName, capitalName) \
    if (supportedConstraints.supports##capitalName()) \
        propertyNames.add(Identifier::fromString(state, AtomicString(#lowerName, AtomicString::ConstructFromLiteral)));

    static const RealtimeMediaSourceSupportedConstraints& supportedConstraints = RealtimeMediaSourceCenter::singleton().supportedConstraints();

    FOR_EACH_CONSTRAINT_PROPERTY(MAYBE_ADD_PROPERTY)

    auto* thisObject = jsCast<JSMediaTrackSupportedConstraints*>(object);
    Base::getOwnPropertyNames(thisObject, state, propertyNames, mode);
}

} // namespace WebCore

#endif

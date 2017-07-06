/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSourceSettings.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const AtomicString& userFacing()
{
    static NeverDestroyed<AtomicString> userFacing("user", AtomicString::ConstructFromLiteral);
    return userFacing;
}
static const AtomicString& environmentFacing()
{
    static NeverDestroyed<AtomicString> environmentFacing("environment", AtomicString::ConstructFromLiteral);
    return environmentFacing;
}

static const AtomicString& leftFacing()
{
    static NeverDestroyed<AtomicString> leftFacing("left", AtomicString::ConstructFromLiteral);
    return leftFacing;
}

static const AtomicString& rightFacing()
{
    static NeverDestroyed<AtomicString> rightFacing("right", AtomicString::ConstructFromLiteral);
    return rightFacing;
}

const AtomicString& RealtimeMediaSourceSettings::facingMode(RealtimeMediaSourceSettings::VideoFacingMode mode)
{
    switch (mode) {
    case RealtimeMediaSourceSettings::User:
        return userFacing();
    case RealtimeMediaSourceSettings::Environment:
        return environmentFacing();
    case RealtimeMediaSourceSettings::Left:
        return leftFacing();
    case RealtimeMediaSourceSettings::Right:
        return rightFacing();
    case RealtimeMediaSourceSettings::Unknown:
        return emptyAtom();
    }
    
    ASSERT_NOT_REACHED();
    return emptyAtom();
}

RealtimeMediaSourceSettings::VideoFacingMode RealtimeMediaSourceSettings::videoFacingModeEnum(const String& mode)
{
    if (mode == userFacing())
        return RealtimeMediaSourceSettings::User;
    if (mode == environmentFacing())
        return RealtimeMediaSourceSettings::Environment;
    if (mode == leftFacing())
        return RealtimeMediaSourceSettings::Left;
    if (mode == rightFacing())
        return RealtimeMediaSourceSettings::Right;

    return RealtimeMediaSourceSettings::Unknown;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

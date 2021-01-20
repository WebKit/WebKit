/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "DoubleRange.h"
#include "LongRange.h"
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MediaConstraints;

struct ConstrainBooleanParameters {
    Optional<bool> exact;
    Optional<bool> ideal;
};

struct ConstrainDOMStringParameters {
    Optional<Variant<String, Vector<String>>> exact;
    Optional<Variant<String, Vector<String>>> ideal;
};

struct ConstrainDoubleRange : DoubleRange {
    Optional<double> exact;
    Optional<double> ideal;
};

struct ConstrainLongRange : LongRange {
    Optional<int> exact;
    Optional<int> ideal;
};

using ConstrainBoolean = Variant<bool, ConstrainBooleanParameters>;
using ConstrainDOMString = Variant<String, Vector<String>, ConstrainDOMStringParameters>;
using ConstrainDouble = Variant<double, ConstrainDoubleRange>;
using ConstrainLong = Variant<int, ConstrainLongRange>;

struct MediaTrackConstraintSet {
    Optional<ConstrainLong> width;
    Optional<ConstrainLong> height;
    Optional<ConstrainDouble> aspectRatio;
    Optional<ConstrainDouble> frameRate;
    Optional<ConstrainDOMString> facingMode;
    Optional<ConstrainDouble> volume;
    Optional<ConstrainLong> sampleRate;
    Optional<ConstrainLong> sampleSize;
    Optional<ConstrainBoolean> echoCancellation;
    Optional<ConstrainDOMString> deviceId;
    Optional<ConstrainDOMString> groupId;
    Optional<ConstrainDOMString> displaySurface;
    Optional<ConstrainBoolean> logicalSurface;
};

struct MediaTrackConstraints : MediaTrackConstraintSet {
    Optional<Vector<MediaTrackConstraintSet>> advanced;
};

MediaConstraints createMediaConstraints(const MediaTrackConstraints&);

}

#endif

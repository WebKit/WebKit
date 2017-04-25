/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "MediaConstraintsImpl.h"

namespace WebCore {

static inline void addDefaultVideoConstraints(MediaTrackConstraintSetMap& videoConstraints, bool addFrameRateConstraint, bool addSizeConstraint, bool addFacingModeConstraint)
{
    if (addFrameRateConstraint) {
        DoubleConstraint frameRateConstraint({ }, MediaConstraintType::FrameRate);
        frameRateConstraint.setIdeal(30);
        videoConstraints.set(MediaConstraintType::FrameRate, WTFMove(frameRateConstraint));
    }
    if (addSizeConstraint) {
        IntConstraint widthConstraint({ }, MediaConstraintType::Width);
        widthConstraint.setIdeal(640);
        videoConstraints.set(MediaConstraintType::Width, WTFMove(widthConstraint));

        IntConstraint heightConstraint({ }, MediaConstraintType::Height);
        heightConstraint.setIdeal(480);
        videoConstraints.set(MediaConstraintType::Height, WTFMove(heightConstraint));
    }
    if (addFacingModeConstraint) {
        StringConstraint facingModeConstraint({ }, MediaConstraintType::FacingMode);
        facingModeConstraint.setIdeal(ASCIILiteral("user"));
        videoConstraints.set(MediaConstraintType::FacingMode, WTFMove(facingModeConstraint));
    }
}

bool MediaConstraintsData::isConstraintSet(std::function<bool(const MediaTrackConstraintSetMap&)>&& callback)
{
    if (callback(mandatoryConstraints))
        return true;

    for (const auto& constraint : advancedConstraints) {
        if (callback(constraint))
            return true;
    }
    return false;
}

void MediaConstraintsData::setDefaultVideoConstraints()
{
    // 640x480, 30fps, font-facing camera
    bool hasFrameRateConstraints = isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.frameRate();
    });

    bool hasSizeConstraints = isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.width() || !!constraint.height();
    });

    bool hasFacingModeConstraints = isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.facingMode();
    });

    if (hasFrameRateConstraints && hasSizeConstraints && hasFacingModeConstraints)
        return;

    addDefaultVideoConstraints(mandatoryConstraints, !hasFrameRateConstraints, !hasSizeConstraints, !hasFacingModeConstraints);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

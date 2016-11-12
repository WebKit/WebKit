/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ExceptionBase.h"
#include "MediaConstraints.h"
#include <wtf/Vector.h>

namespace WebCore {

struct MediaConstraintsData {
    MediaConstraintsData() = default;
    MediaConstraintsData(MediaTrackConstraintSetMap&& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>&& advancedConstraints, bool isValid)
        : mandatoryConstraints(WTFMove(mandatoryConstraints))
        , advancedConstraints(WTFMove(advancedConstraints))
        , isValid(isValid)
    {
    }

    MediaTrackConstraintSetMap mandatoryConstraints;
    Vector<MediaTrackConstraintSetMap> advancedConstraints;
    bool isValid { false };
};

class MediaConstraintsImpl final : public MediaConstraints {
public:
    static Ref<MediaConstraintsImpl> create(MediaTrackConstraintSetMap&& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>&& advancedConstraints, bool isValid);
    WEBCORE_EXPORT static Ref<MediaConstraintsImpl> create(const MediaConstraintsData&);

    MediaConstraintsImpl() = default;
    virtual ~MediaConstraintsImpl() = default;

    const MediaTrackConstraintSetMap& mandatoryConstraints() const final { return m_data.mandatoryConstraints; }
    const Vector<MediaTrackConstraintSetMap>& advancedConstraints() const final { return m_data.advancedConstraints; }
    bool isValid() const final { return m_data.isValid; }
    const MediaConstraintsData& data() const { return m_data; }

private:
    MediaConstraintsImpl(MediaTrackConstraintSetMap&& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>&& advancedConstraints, bool isValid)
        : m_data({ WTFMove(mandatoryConstraints), WTFMove(advancedConstraints), isValid })
    {
    }
    explicit MediaConstraintsImpl(const MediaConstraintsData& data)
        : m_data(data)
    {
    }

    MediaConstraintsData m_data;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

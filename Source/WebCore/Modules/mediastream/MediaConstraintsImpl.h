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

#ifndef MediaConstraintsImpl_h
#define MediaConstraintsImpl_h

#if ENABLE(MEDIA_STREAM)

#include "ExceptionBase.h"
#include "MediaConstraints.h"
#include <wtf/Vector.h>

namespace WebCore {

class ArrayValue;
class Dictionary;

class MediaConstraintsImpl final : public MediaConstraints {
public:
    static Ref<MediaConstraintsImpl> create();
    static Ref<MediaConstraintsImpl> create(MediaTrackConstraintSetMap&& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>&& advancedConstraints, bool isValid);

    virtual ~MediaConstraintsImpl();
    void initialize(const Dictionary&);

    const MediaTrackConstraintSetMap& mandatoryConstraints() const final { return m_mandatoryConstraints; }
    const Vector<MediaTrackConstraintSetMap>& advancedConstraints() const final { return m_advancedConstraints; }
    bool isValid() const final { return m_isValid; }

private:
    MediaConstraintsImpl() { }
    MediaConstraintsImpl(MediaTrackConstraintSetMap&& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>&& advancedConstraints, bool isValid)
        : m_mandatoryConstraints(WTFMove(mandatoryConstraints))
        , m_advancedConstraints(WTFMove(advancedConstraints))
        , m_isValid(isValid)
    {
    }

    MediaTrackConstraintSetMap m_mandatoryConstraints;
    Vector<MediaTrackConstraintSetMap> m_advancedConstraints;
    bool m_isValid;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaConstraintsImpl_h



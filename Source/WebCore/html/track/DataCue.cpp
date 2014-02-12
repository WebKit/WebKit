/*
 * Copyright (C) 2014 Cable Television Labs Inc. All rights reserved.
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
#include "DataCue.h"

#include "Logging.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"

namespace WebCore {

DataCue::DataCue(ScriptExecutionContext& context, double start, double end, PassRefPtr<ArrayBuffer> data)
    : TextTrackCue(context, start, end)
    , m_data(data)
{
}

DataCue::~DataCue()
{
}

String DataCue::text(bool& isNull) const
{
    isNull = true;
    return String();
}

DataCue* toDataCue(TextTrackCue* cue)
{
    ASSERT_WITH_SECURITY_IMPLICATION(cue->cueType() == TextTrackCue::Data);
    return static_cast<DataCue*>(cue);
}

const DataCue* toDataCue(const TextTrackCue* cue)
{
    ASSERT_WITH_SECURITY_IMPLICATION(cue->cueType() == TextTrackCue::Data);
    return static_cast<const DataCue*>(cue);
}

} // namespace WebCore

#endif

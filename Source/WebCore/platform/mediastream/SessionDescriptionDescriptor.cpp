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

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "SessionDescriptionDescriptor.h"

#include "IceCandidateDescriptor.h"
#include "MediaStreamCenter.h"

namespace WebCore {

PassRefPtr<SessionDescriptionDescriptor> SessionDescriptionDescriptor::create(const String& sdp)
{
    return adoptRef(new SessionDescriptionDescriptor(sdp));
}

SessionDescriptionDescriptor::SessionDescriptionDescriptor(const String& sdp)
    : m_initialSDP(sdp)
{
}

SessionDescriptionDescriptor::~SessionDescriptionDescriptor()
{
}

void SessionDescriptionDescriptor::addCandidate(PassRefPtr<IceCandidateDescriptor> candidate)
{
    m_candidates.append(candidate);
}

String SessionDescriptionDescriptor::toSDP()
{
    return MediaStreamCenter::instance().constructSDP(this);
}

size_t SessionDescriptionDescriptor::numberOfAddedCandidates() const
{
    return m_candidates.size();
}

IceCandidateDescriptor* SessionDescriptionDescriptor::candidate(size_t index) const
{
    return m_candidates[index].get();
}

const String& SessionDescriptionDescriptor::initialSDP()
{
    return m_initialSDP;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

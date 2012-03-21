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

#include "IceCandidate.h"

#include "IceCandidateDescriptor.h"

namespace WebCore {

PassRefPtr<IceCandidate> IceCandidate::create(const String& label, const String& candidateLine)
{
    return adoptRef(new IceCandidate(IceCandidateDescriptor::create(label, candidateLine)));
}

PassRefPtr<IceCandidate> IceCandidate::create(PassRefPtr<IceCandidateDescriptor> descriptor)
{
    return adoptRef(new IceCandidate(descriptor));
}

IceCandidate::IceCandidate(PassRefPtr<IceCandidateDescriptor> descriptor)
    : m_descriptor(descriptor)
{
}

IceCandidate::~IceCandidate()
{
}

const String& IceCandidate::label()
{
    return m_descriptor->label();
}

const String& IceCandidate::candidateLine()
{
    return m_descriptor->candidateLine();
}

String IceCandidate::toSdp()
{
    return m_descriptor->toSDP();
}

IceCandidateDescriptor* IceCandidate::descriptor()
{
    return m_descriptor.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

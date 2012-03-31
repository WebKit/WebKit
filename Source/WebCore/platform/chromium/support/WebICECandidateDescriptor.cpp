/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include <public/WebICECandidateDescriptor.h>

#include "IceCandidateDescriptor.h"
#include <public/WebString.h>

using namespace WebCore;

namespace WebKit {

WebICECandidateDescriptor::WebICECandidateDescriptor(IceCandidateDescriptor* iceCandidate)
    : m_private(iceCandidate)
{
}

WebICECandidateDescriptor::WebICECandidateDescriptor(PassRefPtr<IceCandidateDescriptor> iceCandidate)
    : m_private(iceCandidate)
{
}

void WebICECandidateDescriptor::assign(const WebICECandidateDescriptor& other)
{
    m_private = other.m_private;
}

void WebICECandidateDescriptor::reset()
{
    m_private.reset();
}

void WebICECandidateDescriptor::initialize(const WebString& label, const WebString& candidateLine)
{
    m_private = IceCandidateDescriptor::create(label, candidateLine);
}

WebICECandidateDescriptor::operator PassRefPtr<WebCore::IceCandidateDescriptor>() const
{
    return m_private.get();
}

WebString WebICECandidateDescriptor::label() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->label();
}

WebString WebICECandidateDescriptor::candidateLine() const
{
    ASSERT(!m_private.isNull());
    return m_private.get()->candidateLine();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)


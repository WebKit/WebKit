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

#ifndef AllVideoCapabilities_h
#define AllVideoCapabilities_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamCapabilities.h"

namespace WebCore {

class CapabilityRange;
class MediaStreamSourceCapabilities;

class AllVideoCapabilities : public MediaStreamCapabilities {
public:
    static RefPtr<AllVideoCapabilities> create(PassRefPtr<MediaStreamSourceCapabilities> capabilities)
    {
        return adoptRef(new AllVideoCapabilities(capabilities));
    }
    virtual ~AllVideoCapabilities() { }

private:
    explicit AllVideoCapabilities(PassRefPtr<MediaStreamSourceCapabilities> capabilities)
        : MediaStreamCapabilities(capabilities)
    {
    }
};

} // namespace WebCore

#endif // AllVideoCapabilities_h

#endif

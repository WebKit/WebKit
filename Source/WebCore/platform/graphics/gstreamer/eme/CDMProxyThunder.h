/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

#include "CDMInstanceSession.h"
#include "CDMThunder.h"
#include "GStreamerEMEUtilities.h"
#include "MediaPlayerPrivate.h"
#include "SharedBuffer.h"
#include <wtf/Condition.h>
#include <wtf/VectorHash.h>

namespace WebCore {

// This is the thread-safe API that decode threads should use to make use of a platform CDM module.
class CDMProxyThunder final : public CDMProxy, public CanMakeWeakPtr<CDMProxyThunder, WeakPtrFactoryInitialization::Eager> {
public:
    CDMProxyThunder(const String& keySystem)
        : m_keySystem(keySystem) { }
    virtual ~CDMProxyThunder() = default;

    struct DecryptionContext {
        GstBuffer* keyIDBuffer;
        GstBuffer* ivBuffer;
        GstBuffer* dataBuffer;
        GstBuffer* subsamplesBuffer;
        size_t numSubsamples;
    };

    bool decrypt(DecryptionContext&);
    const String& keySystem() { return m_keySystem; }

private:
    BoxPtr<OpenCDMSession> getDecryptionSession(const DecryptionContext&) const;
    String m_keySystem;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

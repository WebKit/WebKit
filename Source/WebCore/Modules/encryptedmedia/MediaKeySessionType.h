/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
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

#if ENABLE(ENCRYPTED_MEDIA)

#include <WebCore/CDMSessionType.h>

namespace WebCore {

enum class MediaKeySessionType : uint8_t {
    Temporary,
    PersistentUsageRecord,
    PersistentLicense,
};

inline CDMSessionType toPlatform(MediaKeySessionType value)
{
    switch (value) {
    case MediaKeySessionType::Temporary:             return CDMSessionType::Temporary;
    case MediaKeySessionType::PersistentUsageRecord: return CDMSessionType::PersistentUsageRecord;
    case MediaKeySessionType::PersistentLicense:     return CDMSessionType::PersistentLicense;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline MediaKeySessionType fromPlatform(CDMSessionType value)
{
    switch (value) {
    case CDMSessionType::Temporary:             return MediaKeySessionType::Temporary;
    case CDMSessionType::PersistentUsageRecord: return MediaKeySessionType::PersistentUsageRecord;
    case CDMSessionType::PersistentLicense:     return MediaKeySessionType::PersistentLicense;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

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

#include "config.h"
#include "CDMProxyThunder.h"

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

#include "CDMThunder.h"
#include "Logging.h"
#include <open_cdm_adapter.h>
#include <wtf/ByteOrder.h>

GST_DEBUG_CATEGORY_EXTERN(webkitMediaThunderDecryptDebugCategory);
#define GST_CAT_DEFAULT webkitMediaThunderDecryptDebugCategory

namespace WebCore {

// NOTE: YouTube 2019 EME conformance tests expect this to be >=5s.
const WTF::Seconds s_licenseKeyResponseTimeout = WTF::Seconds(6);

BoxPtr<OpenCDMSession> CDMProxyThunder::getDecryptionSession(const DecryptionContext& in) const
{
    auto mappedKeyID = GstMappedBuffer::create(in.keyIDBuffer, GST_MAP_READ);
    if (!mappedKeyID) {
        GST_ERROR("Failed to map key ID buffer");
        return nullptr;
    }

    auto keyID = mappedKeyID->createVector();

    auto keyHandle = getOrWaitForKeyHandle(keyID);
    if (!keyHandle.hasValue() || !keyHandle.value()->isStatusCurrentlyValid())
        return nullptr;

    KeyHandleValueVariant keyData = keyHandle.value()->value();
    ASSERT(WTF::holds_alternative<BoxPtr<OpenCDMSession>>(keyData));

    BoxPtr<OpenCDMSession> keyValue = WTF::get<BoxPtr<OpenCDMSession>>(keyData);

    if (!keyValue) {
        keyValue = adoptInBoxPtr(opencdm_get_system_session(&static_cast<const CDMInstanceThunder*>(instance())->thunderSystem(), keyID.data(),
            keyID.size(), s_licenseKeyResponseTimeout.millisecondsAs<uint32_t>()));
        ASSERT(keyValue);
        // takeValueIfDifferent takes and r-value ref of
        // KeyHandleValueVariant. We want to copy the BoxPtr when
        // passing it down, cause we return it from this method. If we
        // just don't move the BoxPtr, the const BoxPtr& constructor
        // will be used. Anyway, letting that subtlety to the compiler
        // could be misleading so we explicitly invoke the const
        // BoxPtr& constructor here.
        keyHandle.value()->takeValueIfDifferent(BoxPtr<OpenCDMSession>(keyValue));
    }

    return keyValue;
}

bool CDMProxyThunder::decrypt(CDMProxyThunder::DecryptionContext& input)
{
    BoxPtr<OpenCDMSession> session = getDecryptionSession(input);
    if (!session) {
        GST_ERROR("there is no valid session to decrypt for the provided key ID");
        return false;
    }

    GST_TRACE("decrypting");
    // Decrypt cipher.
    OpenCDMError errorCode = opencdm_gstreamer_session_decrypt(session->get(), input.dataBuffer, input.subsamplesBuffer, input.numSubsamples,
        input.ivBuffer, input.keyIDBuffer, 0);
    if (errorCode) {
        GST_ERROR("decryption failed, error code %X", errorCode);
        return false;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

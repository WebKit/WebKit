/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VibrationProvider_h
#define VibrationProvider_h

#if ENABLE(VIBRATION)

#include "WKRetainPtr.h"
#include "ewk_context.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

typedef struct Ewk_Vibration_Client Ewk_Vibration_Client;

namespace WebKit {

class VibrationProvider : public RefCounted<VibrationProvider> {
public:
    static PassRefPtr<VibrationProvider> create(WKContextRef);
    virtual ~VibrationProvider();

    void vibrate(uint64_t vibrationTime);
    void cancelVibration();
    void setVibrationClientCallbacks(Ewk_Vibration_Client_Vibrate_Cb, Ewk_Vibration_Client_Vibration_Cancel_Cb, void*);

private:
    explicit VibrationProvider(WKContextRef);

    WKRetainPtr<WKContextRef> m_wkContext;
    OwnPtr<Ewk_Vibration_Client> m_vibrationClient;
};

} // namespace WebKit

#endif // ENABLE(VIBRATION)

#endif // VibrationProvider_h

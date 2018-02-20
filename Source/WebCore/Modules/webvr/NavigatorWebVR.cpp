/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "NavigatorWebVR.h"

#include "Document.h"
#include "JSVRDisplay.h"
#include "Navigator.h"
#include "RuntimeEnabledFeatures.h"
#include "VRDisplay.h"
#include "VRManager.h"

namespace WebCore {

void NavigatorWebVR::getVRDisplays(Navigator&, Document& document, GetVRDisplaysPromise&& promise)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webVREnabled()) {
        promise.reject();
        return;
    }

    document.postTask([promise = WTFMove(promise)] (ScriptExecutionContext& context) mutable {
        std::optional<VRManager::VRDisplaysVector> platformDisplays = VRManager::singleton().getVRDisplays();
        if (!platformDisplays) {
            promise.reject();
            return;
        }

        Vector<Ref<VRDisplay>> displays;
        for (auto& platformDisplay : platformDisplays.value())
            displays.append(VRDisplay::create(context, WTFMove(platformDisplay)));
        promise.resolve(displays);
    });
}

const Vector<RefPtr<VRDisplay>>& NavigatorWebVR::activeVRDisplays(Navigator&)
{
    static auto mockVector = makeNeverDestroyed(Vector<RefPtr<VRDisplay>> { });
    return mockVector;
}

} // namespace WebCore

/*
 * Copyright (C) 2017-2018 Igalia S.L. All rights reserved.
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
#include "VRPlatformDisplay.h"

namespace WebCore {

void NavigatorWebVR::getVRDisplays(Navigator& navigator, Document& document, GetVRDisplaysPromise&& promise)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webVREnabled()) {
        promise.reject();
        return;
    }
    NavigatorWebVR::from(&navigator)->getVRDisplays(document, WTFMove(promise));
}

void NavigatorWebVR::getVRDisplays(Document& document, GetVRDisplaysPromise&& promise)
{
    document.postTask([this, promise = WTFMove(promise)] (ScriptExecutionContext& context) mutable {
        Optional<VRManager::VRDisplaysVector> platformDisplays = VRManager::singleton().getVRDisplays();
        if (!platformDisplays) {
            promise.reject();
            m_displays.clear();
            return;
        }

        Vector<Ref<VRDisplay>> oldDisplays = WTFMove(m_displays);

        // Update the vector of displays. Note that although this is O(n^2) the amount of expected
        // VRDisplays is so reduced that it'll run fast enough.
        for (auto& platformDisplay : platformDisplays.value()) {
            bool newDisplay = true;
            for (auto& oldDisplay : oldDisplays) {
                if (platformDisplay->getDisplayInfo().displayIdentifier() == oldDisplay->displayId()) {
                    newDisplay = false;
                    m_displays.append(oldDisplay.copyRef());
                    break;
                }
            }
            if (newDisplay) {
                m_displays.append(VRDisplay::create(context, WTFMove(platformDisplay)));
                m_displays.last()->platformDisplayConnected();
            }
        }
        promise.resolve(WTF::map(m_displays, [](const Ref<VRDisplay>& display) {
            return display.copyRef();
        }));
    });
}

const Vector<RefPtr<VRDisplay>>& NavigatorWebVR::activeVRDisplays(Navigator&)
{
    static auto mockVector = makeNeverDestroyed(Vector<RefPtr<VRDisplay>> { });
    return mockVector;
}

const char* NavigatorWebVR::supplementName()
{
    return "NavigatorWebVR";
}

NavigatorWebVR* NavigatorWebVR::from(Navigator* navigator)
{
    NavigatorWebVR* supplement = static_cast<NavigatorWebVR*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorWebVR>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

} // namespace WebCore

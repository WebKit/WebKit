/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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
#include "OpenXRInput.h"

#if ENABLE(WEBXR) && USE(OPENXR)
#include "OpenXRInputSource.h"
#include <openxr/openxr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRInput);

std::unique_ptr<OpenXRInput> OpenXRInput::create(XrInstance instance, XrSession session, OpenXRSystemProperties&& systemProperties)
{
    auto input = std::unique_ptr<OpenXRInput>(new OpenXRInput(instance, session));
    if (XR_FAILED(input->initialize(WTF::move(systemProperties))))
        return nullptr;
    return input;
}

OpenXRInput::OpenXRInput(XrInstance instance, XrSession session)
    : m_instance(instance)
    , m_session(session)
{
}

OpenXRInput::~OpenXRInput() = default;

XrResult OpenXRInput::initialize(OpenXRSystemProperties&& systemProperties)
{
    for (auto handedness : { PlatformXR::XRHandedness::Left, PlatformXR::XRHandedness::Right }) {
        m_handleIndex++;
        if (auto inputSource = OpenXRInputSource::create(m_instance, m_session, handedness, m_handleIndex, WTF::move(systemProperties)))
            m_inputSources.append(makeUniqueRefFromNonNullUniquePtr(WTF::move(inputSource)));
    }

    OpenXRInputSource::SuggestedBindings bindings;
    Vector<XrActionSet> actionSets;
    actionSets.reserveInitialCapacity(m_inputSources.size());
    for (const auto& inputSource : m_inputSources) {
        inputSource->suggestBindings(bindings);
        actionSets.append(inputSource->actionSet());
    }

    for (const auto& binding : bindings) {
        auto suggestedBinding = createOpenXRStruct<XrInteractionProfileSuggestedBinding, XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING>();
        if (XR_FAILED(xrStringToPath(m_instance, binding.key, &suggestedBinding.interactionProfile)))
            continue;
        suggestedBinding.countSuggestedBindings = binding.value.size();
        suggestedBinding.suggestedBindings = binding.value.span().data();
        CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBinding));
    }

    auto attachInfo = createOpenXRStruct<XrSessionActionSetsAttachInfo, XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO>();
    attachInfo.countActionSets = actionSets.size();
    attachInfo.actionSets = actionSets.span().data();
    return CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
}

Vector<PlatformXR::FrameData::InputSource> OpenXRInput::collectInputSources(const XrFrameState& frameState, XrSpace space) const
{
    Vector<XrActiveActionSet> actionSets;
    actionSets.reserveInitialCapacity(m_inputSources.size());
    for (const auto& input : m_inputSources)
        actionSets.append(XrActiveActionSet { input->actionSet(), XR_NULL_PATH });

    auto syncInfo = createOpenXRStruct<XrActionsSyncInfo, XR_TYPE_ACTIONS_SYNC_INFO>();
    syncInfo.countActiveActionSets = actionSets.size();
    syncInfo.activeActionSets = actionSets.span().data();
    CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

    Vector<PlatformXR::FrameData::InputSource> result;
    result.reserveInitialCapacity(m_inputSources.size());
    for (auto& input : m_inputSources) {
        if (auto data = input->collectInputSource(space, frameState))
            result.append(*data);
    }

    return result;
}

void OpenXRInput::updateInteractionProfile()
{
    for (auto& input : m_inputSources)
        input->updateInteractionProfile();
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)

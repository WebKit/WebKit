/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
    if (XR_FAILED(input->initialize(WTFMove(systemProperties))))
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
        if (auto inputSource = OpenXRInputSource::create(m_instance, m_session, handedness, m_handleIndex, WTFMove(systemProperties)))
            m_inputSources.append(makeUniqueRefFromNonNullUniquePtr(WTFMove(inputSource)));
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

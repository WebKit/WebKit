/*
 * Copyright (C) 2021 Igalia, S.L.
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
#include <wtf/TZoneMallocInlines.h>

using namespace WebCore;

namespace PlatformXR {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRInput);

std::unique_ptr<OpenXRInput> OpenXRInput::create(XrInstance instance, XrSession session, XrSpace space)
{
    auto input = std::unique_ptr<OpenXRInput>(new OpenXRInput(instance, session, space));
    if (XR_FAILED(input->initialize()))
        return nullptr;
    return input;
}

OpenXRInput::OpenXRInput(XrInstance instance, XrSession session, XrSpace space)
    : m_instance(instance)
    , m_session(session)
    , m_localSpace(space)
{
}

XrResult OpenXRInput::initialize()
{
    std::array<XRHandedness, 2> hands {
        XRHandedness::Left, XRHandedness::Right
    };

    for (auto handedness : hands) {
        m_handleIndex++;;
        if (auto inputSource = OpenXRInputSource::create(m_instance, m_session, handedness, m_handleIndex))
            m_inputSources.append(makeUniqueRefFromNonNullUniquePtr(WTFMove(inputSource)));
    }

    OpenXRInputSource::SuggestedBindings bindings;
    Vector<XrActionSet> actionSets;
    for (auto& input : m_inputSources) {
        input->suggestBindings(bindings);
        actionSets.append(input->actionSet());
    }
    
    for (auto& binding : bindings) {
        auto suggestedBinding = createStructure<XrInteractionProfileSuggestedBinding, XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING>();
        RETURN_RESULT_IF_FAILED(xrStringToPath(m_instance, binding.key, &suggestedBinding.interactionProfile), m_instance);
        suggestedBinding.countSuggestedBindings = binding.value.size();
        suggestedBinding.suggestedBindings = binding.value.data();
        RETURN_RESULT_IF_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBinding), m_instance);
    }

    auto attachInfo = createStructure<XrSessionActionSetsAttachInfo, XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO>();
    attachInfo.countActionSets = actionSets.size();
    attachInfo.actionSets = actionSets.data();
    RETURN_RESULT_IF_FAILED(xrAttachSessionActionSets(m_session, &attachInfo), m_instance);

    return XR_SUCCESS;
}

Vector<FrameData::InputSource> OpenXRInput::collectInputSources(const XrFrameState& frameState) const
{
    Vector<XrActiveActionSet> actionSets;
    for (auto& input : m_inputSources)
        actionSets.append(XrActiveActionSet { input->actionSet(), XR_NULL_PATH });

    auto syncInfo = createStructure<XrActionsSyncInfo, XR_TYPE_ACTIONS_SYNC_INFO>();
    syncInfo.countActiveActionSets = actionSets.size();
    syncInfo.activeActionSets = actionSets.data();
    RETURN_IF_FAILED(xrSyncActions(m_session, &syncInfo), "xrSyncActions", m_instance, { });

    Vector<FrameData::InputSource> result;
    for (auto& input : m_inputSources) {
        if (auto data = input->getInputSource(m_localSpace, frameState))
            result.append(*data);
    }

    return result;
}

void OpenXRInput::updateInteractionProfile()
{
    for (auto& input : m_inputSources)
        input->updateInteractionProfile();
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)

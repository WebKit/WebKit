/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebXRInputSource.h"

#if ENABLE(WEBXR)

#include "EventNames.h"
#if ENABLE(GAMEPAD)
#include "Gamepad.h"
#include "WebXRGamepad.h"
#endif
#include "WebXRFrame.h"
#include "WebXRSession.h"
#include "XRInputSourceEvent.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRInputSource);

Ref<WebXRInputSource> WebXRInputSource::create(Document& document, WebXRSession& session, double timestamp, const PlatformXR::Device::FrameData::InputSource& source)
{
    return adoptRef(*new WebXRInputSource(document, session, timestamp, source));
}

WebXRInputSource::WebXRInputSource(Document& document, WebXRSession& session, double timestamp, const PlatformXR::Device::FrameData::InputSource& source)
    : m_session(session)
    , m_targetRaySpace(WebXRInputSpace::create(document, session, source.pointerOrigin))
    , m_connectTime(timestamp)
#if ENABLE(GAMEPAD)
    , m_gamepad(Gamepad::create(WebXRGamepad(timestamp, timestamp, source)))
#endif
{
    update(timestamp, source);
}

WebXRInputSource::~WebXRInputSource() = default;

WebXRSession* WebXRInputSource::session()
{
    return m_session.get();
}

void WebXRInputSource::update(double timestamp, const PlatformXR::Device::FrameData::InputSource& source)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

#if !ENABLE(GAMEPAD)
    UNUSED_PARAM(timestamp);
#endif
    m_source = source;
    m_targetRaySpace->setPose(source.pointerOrigin);
    if (auto gripOrigin = source.gripOrigin) {
        if (m_gripSpace)
            m_gripSpace->setPose(*gripOrigin);
        else if (auto* document = downcast<Document>(session->scriptExecutionContext()))
            m_gripSpace = WebXRInputSpace::create(*document, *session, *gripOrigin);
    } else
        m_gripSpace = nullptr;
#if ENABLE(GAMEPAD)
    m_gamepad->updateFromPlatformGamepad(WebXRGamepad(timestamp, m_connectTime, source));
#endif

#if ENABLE(WEBXR_HANDS)
    if (source.simulateHand) {
        // FIXME: This currently creates an object just for use in testing.
        // The real implementation will use actual data and only be visible
        // if hand-tracking was requested at session start.
        if (!m_hand)
            m_hand = WebXRHand::create(*this);
    } else
        m_hand = nullptr;
#endif

}

bool WebXRInputSource::requiresInputSourceChange(const InputSource& source)
{
    return m_source.handeness != source.handeness
        || m_source.targetRayMode != source.targetRayMode
        || m_source.profiles != source.profiles
        || static_cast<bool>(m_gripSpace) != source.gripOrigin.has_value();
}

void WebXRInputSource::disconnect()
{
    m_connected = false;
#if ENABLE(GAMEPAD)
    m_gamepad->setConnected(false);
#endif
}

void WebXRInputSource::pollEvents(Vector<Ref<XRInputSourceEvent>>& events)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

    auto createEvent = [this, session](const AtomString& name) -> Ref<XRInputSourceEvent> {
        XRInputSourceEvent::Init init;
        init.frame = WebXRFrame::create(*session, WebXRFrame::IsAnimationFrame::No);
        init.inputSource = RefPtr { this };

        return XRInputSourceEvent::create(name, init);
    };

    if (!m_connected) {
        // A user agent MUST dispatch a selectend event on an XRSession when one of its XRInputSources ends 
        // when an XRInputSource that has begun a primary select action is disconnected.
        if (m_selectStarted)
            events.append(createEvent(eventNames().selectendEvent));

        // A user agent MUST dispatch a squeezeend event on an XRSession when one of its XRInputSources ends 
        // when an XRInputSource that has begun a primary squeeze action is disconnected.
        if (m_squeezeStarted)
            events.append(createEvent(eventNames().squeezeendEvent));

        return;
    }

    // https://immersive-web.github.io/webxr-gamepads-module/#xr-standard-gamepad-mapping
    // Button indices are based on the xr-standard layout.
    bool selectStarted = !m_source.buttons.isEmpty() && m_source.buttons.first().pressed;
    bool squeezeStarted = m_source.buttons.size() > 1 && m_source.buttons[1].pressed;

    if (selectStarted != m_selectStarted) {
        if (selectStarted) {
            // A user agent MUST dispatch a selectstart event on an XRSession when one of its XRInputSources begins its primary action.
            events.append(createEvent(eventNames().selectstartEvent));
        } else {
            // A user agent MUST dispatch a selectend event on an XRSession when one of its XRInputSources ends its primary action.
            // A user agent MUST dispatch a select event on an XRSession when one of its XRInputSources has fully completed a primary action
            events.append(createEvent(eventNames().selectEvent));
            events.append(createEvent(eventNames().selectendEvent));
        }
        m_selectStarted = selectStarted;
    }

    if (squeezeStarted != m_squeezeStarted) {
        if (squeezeStarted) {
            // A user agent MUST dispatch a squeezestart event on an XRSession when one of its XRInputSources begins its primary squeeze action.
            events.append(createEvent(eventNames().squeezestartEvent));
        } else {
            // A user agent MUST dispatch a selectend event on an XRSession when one of its XRInputSources ends its primary squeeze action.
            // A user agent MUST dispatch a select event on an XRSession when one of its XRInputSources has fully completed a primary squeeze action.
            events.append(createEvent(eventNames().squeezeEvent));
            events.append(createEvent(eventNames().squeezeendEvent));
        }
        m_squeezeStarted = squeezeStarted;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBXR)

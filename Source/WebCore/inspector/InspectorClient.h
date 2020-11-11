/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/Optional.h>

namespace Inspector {
class FrontendChannel;
}

namespace WebCore {

class FloatRect;
class Frame;
class InspectorController;
class Page;

class InspectorClient {
public:
    virtual ~InspectorClient() = default;

    virtual void inspectedPageDestroyed() = 0;
    virtual void frontendCountChanged(unsigned) { }

    virtual Inspector::FrontendChannel* openLocalFrontend(InspectorController*) = 0;
    virtual void bringFrontendToFront() = 0;
    virtual void didResizeMainFrame(Frame*) { }

    virtual void highlight() = 0;
    virtual void hideHighlight() = 0;

    virtual void showInspectorIndication() { }
    virtual void hideInspectorIndication() { }

    virtual bool overridesShowPaintRects() const { return false; }
    virtual void setShowPaintRects(bool) { }
    virtual void showPaintRect(const FloatRect&) { }
    virtual void didSetSearchingForNode(bool) { }
    virtual void elementSelectionChanged(bool) { }
    virtual void timelineRecordingChanged(bool) { }

    enum class DeveloperPreference {
        AdClickAttributionDebugModeEnabled,
        ITPDebugModeEnabled,
        MockCaptureDevicesEnabled,
    };
    virtual void setDeveloperPreferenceOverride(DeveloperPreference, Optional<bool>) { }

#if ENABLE(REMOTE_INSPECTOR)
    virtual bool allowRemoteInspectionToPageDirectly() const { return false; }
#endif
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::InspectorClient::DeveloperPreference> {
    using values = EnumValues<
        WebCore::InspectorClient::DeveloperPreference,
        WebCore::InspectorClient::DeveloperPreference::AdClickAttributionDebugModeEnabled,
        WebCore::InspectorClient::DeveloperPreference::ITPDebugModeEnabled,
        WebCore::InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled
    >;
};

} // namespace WTF

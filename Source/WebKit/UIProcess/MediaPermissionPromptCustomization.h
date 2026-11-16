/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <optional>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS _WKMediaCapturePermissionAction;
#endif

namespace WebKit {

// More than this and the stacked buttons push the prompt past a comfortable height, on macOS and iOS
// alike.
constexpr size_t maximumPromptAdditionalActions = 2;

struct MediaPermissionPromptAdditionalAction {
    enum class Decision : bool { Deny, Allow };

    String title;
    Decision decision { Decision::Deny };
#if PLATFORM(COCOA)
    RetainPtr<_WKMediaCapturePermissionAction> source;
#endif
};

// A null string means the prompt keeps the text WebKit would have used. Informative text has no WebKit
// default, so a null one means none is shown.
struct MediaPermissionPromptCustomization {
    String messageText;
    String informativeText;
    String allowButtonTitle;
    String denyButtonTitle;
    Vector<MediaPermissionPromptAdditionalAction> additionalActions;
};

// The device IDs are empty unless the prompt had device menus and the user granted permission to capture.
struct MediaPermissionPromptResult {
    bool granted { false };
    bool wasAnswered { false };
#if PLATFORM(COCOA)
    RetainPtr<_WKMediaCapturePermissionAction> chosenAction;
#endif
    String selectedAudioDeviceUID;
    String selectedVideoDeviceUID;
};

// No button means the prompt closed without an answer. A null device ID doesn't change the prompt's selection.
struct MediaPermissionPromptResponseForTesting {
    std::optional<size_t> buttonIndex;
    String cameraPersistentID;
    String microphonePersistentID;
};

// Lets an API test drive the capture permission prompt.
class MediaPermissionPromptForTesting : public RefCounted<MediaPermissionPromptForTesting> {
public:
    struct Device {
        String persistentID;
        String label;
    };

    virtual ~MediaPermissionPromptForTesting() = default;

    const String& title() const LIFETIME_BOUND { return m_title; }
    const String& informativeText() const LIFETIME_BOUND { return m_informativeText; }
    const Vector<String>& buttonTitles() const LIFETIME_BOUND { return m_buttonTitles; }
    bool showsPreview() const { return m_showsPreview; }

    virtual Vector<Device> cameras() const = 0;
    virtual Vector<Device> microphones() const = 0;
    virtual String selectedCameraID() const = 0;
    virtual String selectedMicrophoneID() const = 0;
    virtual unsigned audioLevelUpdateCount() const = 0;
    virtual unsigned previewLayerCount() const = 0;
    virtual bool hasPreviewLayer() const = 0;

    virtual void selectCamera(size_t index) = 0;
    virtual void selectMicrophone(size_t index) = 0;
    virtual void respond(MediaPermissionPromptResponseForTesting&&) = 0;

protected:
    MediaPermissionPromptForTesting(String&& title, String&& informativeText, Vector<String>&& buttonTitles, bool showsPreview)
        : m_title(WTF::move(title))
        , m_informativeText(WTF::move(informativeText))
        , m_buttonTitles(WTF::move(buttonTitles))
        , m_showsPreview(showsPreview)
    {
    }

private:
    const String m_title;
    const String m_informativeText;
    const Vector<String> m_buttonTitles;
    const bool m_showsPreview { false };
};

} // namespace WebKit

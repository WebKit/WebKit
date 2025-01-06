/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "PageClient.h"
#include <WebCore/PlatformTextAlternatives.h>
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

@class PlatformTextAlternatives;
@class WKWebView;

namespace API {
class Attachment;
}

namespace WebCore {
class AlternativeTextUIController;
class Color;

struct AppHighlight;
}

namespace WebKit {

struct TextAnimationData;
enum class TextAnimationType : uint8_t;

class PageClientImplCocoa : public PageClient {
public:
    PageClientImplCocoa(WKWebView *);
    virtual ~PageClientImplCocoa();

    void pageClosed() override;

    void topContentInsetDidChange() final;

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidFinishLaunching() override;
    void gpuProcessDidExit() override;
#endif

#if ENABLE(MODEL_PROCESS)
    void modelProcessDidFinishLaunching() override;
    void modelProcessDidExit() override;
#endif

    void themeColorWillChange() final;
    void themeColorDidChange() final;
    void underPageBackgroundColorWillChange() final;
    void underPageBackgroundColorDidChange() final;
    void pageExtendedBackgroundColorWillChange() final;
    void pageExtendedBackgroundColorDidChange() final;
    void sampledPageTopColorWillChange() final;
    void sampledPageTopColorDidChange() final;
    void isPlayingAudioWillChange() final;
    void isPlayingAudioDidChange() final;

    bool scrollingUpdatesDisabledForTesting() final;

#if ENABLE(ATTACHMENT_ELEMENT)
    void didInsertAttachment(API::Attachment&, const String& source) final;
    void didRemoveAttachment(API::Attachment&) final;
    void didInvalidateDataForAttachment(API::Attachment&) final;
    NSFileWrapper *allocFileWrapperInstance() const final;
    NSSet *serializableFileWrapperClasses() const final;
#endif

    std::optional<WebCore::DictationContext> addDictationAlternatives(PlatformTextAlternatives *) final;
    void replaceDictationAlternatives(PlatformTextAlternatives *, WebCore::DictationContext) final;
    void removeDictationAlternatives(WebCore::DictationContext) final;
    Vector<String> dictationAlternatives(WebCore::DictationContext) final;
    PlatformTextAlternatives *platformDictationAlternatives(WebCore::DictationContext) final;

#if ENABLE(APP_HIGHLIGHTS)
    void storeAppHighlight(const WebCore::AppHighlight&) final;
#endif

    void microphoneCaptureWillChange() final;
    void cameraCaptureWillChange() final;
    void displayCaptureWillChange() final;
    void displayCaptureSurfacesWillChange() final;
    void systemAudioCaptureWillChange() final;

    void microphoneCaptureChanged() final;
    void cameraCaptureChanged() final;
    void displayCaptureChanged() final;
    void displayCaptureSurfacesChanged() final;
    void systemAudioCaptureChanged() final;

    WindowKind windowKind() final;

#if ENABLE(WRITING_TOOLS)
    void proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestionID&, WebCore::IntRect selectionBoundsInRootView) final;

    void proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestionState, const WTF::UUID& replacementUUID) final;

    void writingToolsActiveWillChange() final;
    void writingToolsActiveDidChange() final;

    void didEndPartialIntelligenceTextAnimation() final;
    bool writingToolsTextReplacementsFinished() final;

    void addTextAnimationForAnimationID(const WTF::UUID&, const WebCore::TextAnimationData&) final;
    void removeTextAnimationForAnimationID(const WTF::UUID&) final;
#endif

#if ENABLE(SCREEN_TIME)
    void installScreenTimeWebpageController() final;
    void didChangeScreenTimeWebpageControllerURL() final;
    void updateScreenTimeWebpageControllerURL(WKWebView *);
#endif

#if ENABLE(GAMEPAD)
    void setGamepadsRecentlyAccessed(GamepadsRecentlyAccessed) final;
#if PLATFORM(VISION)
    void gamepadsConnectedStateChanged() final;
#endif
#endif

    void hasActiveNowPlayingSessionChanged(bool) final;

    void videoControlsManagerDidChange() override;

    CocoaWindow *platformWindow() const final;

    void processDidUpdateThrottleState() final;

protected:
    RetainPtr<WKWebView> webView() const { return m_webView.get(); }

    WeakObjCPtr<WKWebView> m_webView;
    std::unique_ptr<WebCore::AlternativeTextUIController> m_alternativeTextUIController;
};

} // namespace WebKit

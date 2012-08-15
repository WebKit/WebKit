/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InternalSettings_h
#define InternalSettings_h

#include "EditingBehaviorTypes.h"
#include "RefCountedSupplement.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if ENABLE(TEXT_AUTOSIZING)
#include "IntSize.h"
#endif

namespace WebCore {

typedef int ExceptionCode;

class Frame;
class Document;
class MockPagePopupDriver;
class Page;
class PagePopupController;
class Settings;

class InternalSettings : public RefCountedSupplement<Page, InternalSettings> {
public:
    class Backup {
    public:
        Backup(Page*, Settings*);
        void restoreTo(Page*, Settings*);

        double m_originalPasswordEchoDurationInSeconds;
        bool m_originalPasswordEchoEnabled;
        bool m_originalCSSExclusionsEnabled;
#if ENABLE(SHADOW_DOM)
        bool m_originalShadowDOMEnabled;
        bool m_originalAuthorShadowDOMForAnyElementEnabled;
#endif
        EditingBehaviorType m_originalEditingBehavior;
        bool m_originalFixedPositionCreatesStackingContext;
        bool m_originalSyncXHRInDocumentsEnabled;
#if ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)
        bool m_originalJavaScriptProfilingEnabled;
#endif
        bool m_originalWindowFocusRestricted;
        bool m_originalDeviceSupportsTouch;
        bool m_originalDeviceSupportsMouse;
#if ENABLE(TEXT_AUTOSIZING)
        bool m_originalTextAutosizingEnabled;
        IntSize m_originalTextAutosizingWindowSizeOverride;
        float m_originalTextAutosizingFontScaleFactor;
#endif
#if ENABLE(DIALOG_ELEMENT)
        bool m_originalDialogElementEnabled;
#endif
        bool m_canStartMedia;
    };

    typedef RefCountedSupplement<Page, InternalSettings> SuperType;
    static InternalSettings* from(Page*);

    virtual ~InternalSettings();
#if ENABLE(PAGE_POPUP)
    PagePopupController* pagePopupController();
#endif
    void reset();

    void setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode&);
    void setForceCompositingMode(bool enabled, ExceptionCode&);
    void setEnableCompositingForFixedPosition(bool enabled, ExceptionCode&);
    void setEnableCompositingForScrollableFrames(bool enabled, ExceptionCode&);
    void setAcceleratedDrawingEnabled(bool enabled, ExceptionCode&);
    void setAcceleratedFiltersEnabled(bool enabled, ExceptionCode&);
    void setMockScrollbarsEnabled(bool enabled, ExceptionCode&);
    void setPasswordEchoEnabled(bool enabled, ExceptionCode&);
    void setPasswordEchoDurationInSeconds(double durationInSeconds, ExceptionCode&);
    void setFixedElementsLayoutRelativeToFrame(bool, ExceptionCode&);
    void setUnifiedTextCheckingEnabled(bool, ExceptionCode&);
    bool unifiedTextCheckingEnabled(ExceptionCode&);
    void setPageScaleFactor(float scaleFactor, int x, int y, ExceptionCode&);
    void setTouchEventEmulationEnabled(bool enabled, ExceptionCode&);
    void setDeviceSupportsTouch(bool enabled, ExceptionCode&);
    void setDeviceSupportsMouse(bool enabled, ExceptionCode&);
    void setShadowDOMEnabled(bool enabled, ExceptionCode&);
    void setAuthorShadowDOMForAnyElementEnabled(bool);
    void setStandardFontFamily(const String& family, const String& script, ExceptionCode&);
    void setSerifFontFamily(const String& family, const String& script, ExceptionCode&);
    void setSansSerifFontFamily(const String& family, const String& script, ExceptionCode&);
    void setFixedFontFamily(const String& family, const String& script, ExceptionCode&);
    void setCursiveFontFamily(const String& family, const String& script, ExceptionCode&);
    void setFantasyFontFamily(const String& family, const String& script, ExceptionCode&);
    void setPictographFontFamily(const String& family, const String& script, ExceptionCode&);
    void setTextAutosizingEnabled(bool enabled, ExceptionCode&);
    void setTextAutosizingWindowSizeOverride(int width, int height, ExceptionCode&);
    void setTextAutosizingFontScaleFactor(float fontScaleFactor, ExceptionCode&);
    void setEnableScrollAnimator(bool enabled, ExceptionCode&);
    bool scrollAnimatorEnabled(ExceptionCode&);
    void setCSSExclusionsEnabled(bool enabled, ExceptionCode&);
    void setCSSVariablesEnabled(bool enabled, ExceptionCode&);
    bool cssVariablesEnabled(ExceptionCode&);
    void setCanStartMedia(bool, ExceptionCode&);
    void setMediaPlaybackRequiresUserGesture(bool, ExceptionCode&);
    void setEditingBehavior(const String&, ExceptionCode&);
    void setFixedPositionCreatesStackingContext(bool, ExceptionCode&);
    void setSyncXHRInDocumentsEnabled(bool, ExceptionCode&);
    void setWindowFocusRestricted(bool, ExceptionCode&);
    void setDialogElementEnabled(bool, ExceptionCode&);
    void setJavaScriptProfilingEnabled(bool enabled, ExceptionCode&);
    Vector<String> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);
    void setPagination(const String& mode, int gap, ExceptionCode& ec) { setPagination(mode, gap, 0, ec); }
    void setPagination(const String& mode, int gap, int pageLength, ExceptionCode&);
    void allowRoundingHacks() const;
    void setShouldDisplayTrackKind(const String& kind, bool enabled, ExceptionCode&);
    bool shouldDisplayTrackKind(const String& kind, ExceptionCode&);
    void setEnableMockPagePopup(bool, ExceptionCode&);
    String configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode&);
    void setMemoryInfoEnabled(bool, ExceptionCode&);
    void setThirdPartyStorageBlockingEnabled(bool, ExceptionCode&);
private:
    explicit InternalSettings(Page*);
    virtual void hostDestroyed() OVERRIDE { m_page = 0; }

    Settings* settings() const;
    Page* page() const { return m_page; }

    Page* m_page;
    Backup m_backup;
#if ENABLE(PAGE_POPUP)
    OwnPtr<MockPagePopupDriver> m_pagePopupDriver;
#endif
};

} // namespace WebCore

#endif

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef CaptionUserPreferencesMac_h
#define CaptionUserPreferencesMac_h

#if ENABLE(VIDEO_TRACK)

#include "CSSPropertyNames.h"
#include "CaptionUserPreferences.h"
#include "Color.h"
#include <wtf/HashSet.h>

namespace WebCore {

class CaptionUserPreferencesMac : public CaptionUserPreferences {
public:
    static PassOwnPtr<CaptionUserPreferencesMac> create(PageGroup* group) { return adoptPtr(new CaptionUserPreferencesMac(group)); }
    virtual ~CaptionUserPreferencesMac();

    virtual bool userPrefersCaptions() const OVERRIDE;
    virtual bool userHasCaptionPreferences() const OVERRIDE;
    virtual float captionFontSizeScale() const OVERRIDE;
    virtual String captionsStyleSheetOverride() const OVERRIDE;
    virtual void registerForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener*) OVERRIDE;
    virtual void unregisterForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener*) OVERRIDE;

    void captionPreferencesChanged();

private:
    CaptionUserPreferencesMac(PageGroup*);

    Color captionsWindowColor() const;
    Color captionsBackgroundColor() const;
    Color captionsTextColor() const;
    String captionsDefaultFont() const;
    Color captionsEdgeColorForTextColor(const Color&) const;
    String captionsTextEdgeStyle() const;
    String cssPropertyWithTextEdgeColor(CSSPropertyID, const String&, const Color&) const;
    String cssColorProperty(CSSPropertyID, const Color&) const;

    void updateCaptionStyleSheetOveride();

    HashSet<CaptionPreferencesChangedListener*> m_captionPreferenceChangeListeners;
    bool m_listeningForPreferenceChanges;
};
    
}
#endif

#endif

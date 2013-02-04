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

#ifndef CaptionUserPreferences_h
#define CaptionUserPreferences_h

#if ENABLE(VIDEO_TRACK)

#include "Language.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class PageGroup;

class CaptionPreferencesChangedListener {
public:
    virtual void captionPreferencesChanged() = 0;
protected:
    virtual ~CaptionPreferencesChangedListener() { }
};

class CaptionUserPreferences {
public:
    static PassOwnPtr<CaptionUserPreferences> create(PageGroup* group) { return adoptPtr(new CaptionUserPreferences(group)); }
    virtual ~CaptionUserPreferences() { }

    virtual bool userPrefersCaptions() const { return false; }
    virtual void setUserPrefersCaptions(bool) { }
    virtual bool userHasCaptionPreferences() const { return false; }
    virtual float captionFontSizeScale(bool& important) const { important = false; return 0.05f; }
    virtual String captionsStyleSheetOverride() const { return emptyString(); }
    virtual void registerForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener*) { }
    virtual void unregisterForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener*) { }

    virtual void setPreferredLanguage(String) const { }
    virtual Vector<String> preferredLanguages() const { return platformUserPreferredLanguages(); }

    PageGroup* pageGroup() { return m_pageGroup; }

protected:
    CaptionUserPreferences(PageGroup* group) : m_pageGroup(group) { }

private:
    PageGroup* m_pageGroup;
};
    
}
#endif

#endif

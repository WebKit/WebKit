/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebPreferences_h
#define WebPreferences_h

#include "APIObject.h"
#include "FontSmoothingLevel.h"
#include "WebPreferencesStore.h"
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebContext;

class WebPreferences : public APIObject {
public:
    static const Type APIType = TypePreferences;

    static WebPreferences* shared();

    static PassRefPtr<WebPreferences> create()
    {
        return adoptRef(new WebPreferences);
    }
    static PassRefPtr<WebPreferences> copy(WebPreferences* preferences)
    {
        return adoptRef(new WebPreferences(preferences));
    }
    ~WebPreferences();

    void addContext(WebContext*);
    void removeContext(WebContext*);

    const WebPreferencesStore& store() const { return m_store; }

    void setJavaScriptEnabled(bool);
    bool javaScriptEnabled() const;

    void setLoadsImagesAutomatically(bool);
    bool loadsImagesAutomatically() const;

    void setOfflineWebApplicationCacheEnabled(bool);
    bool offlineWebApplicationCacheEnabled() const;

    void setLocalStorageEnabled(bool);
    bool localStorageEnabled() const;

    void setXSSAuditorEnabled(bool);
    bool xssAuditorEnabled() const;

    void setFontSmoothingLevel(FontSmoothingLevel);
    FontSmoothingLevel fontSmoothingLevel() const;

private:
    WebPreferences();
    WebPreferences(WebPreferences*);

    virtual Type type() const { return APIType; }

    void update();

    HashSet<WebContext*> m_contexts;
    WebPreferencesStore m_store;
};

} // namespace WebKit

#endif // WebPreferences_h

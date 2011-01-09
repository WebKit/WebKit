/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#define DECLARE_PREFERENCE_GETTER_AND_SETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) \
    void set##KeyUpper(const Type& value); \
    Type KeyLower() const;

namespace WebKit {

class WebPageGroup;

class WebPreferences : public APIObject {
public:
    static const Type APIType = TypePreferences;

    static PassRefPtr<WebPreferences> create()
    {
        return adoptRef(new WebPreferences);
    }
    static PassRefPtr<WebPreferences> create(const String& identifier)
    {
        return adoptRef(new WebPreferences(identifier));
    }

    virtual ~WebPreferences();

    void addPageGroup(WebPageGroup*);
    void removePageGroup(WebPageGroup*);

    const WebPreferencesStore& store() const { return m_store; }

#define DECLARE_PREFERENCE_GETTER_AND_SETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) \
    void set##KeyUpper(const Type& value); \
    Type KeyLower() const; \

    FOR_EACH_WEBKIT_PREFERENCE(DECLARE_PREFERENCE_GETTER_AND_SETTERS)

#undef DECLARE_PREFERENCE_GETTER_AND_SETTERS

private:
    WebPreferences();
    WebPreferences(const String& identifier);

    void platformInitializeStore();

    virtual Type type() const { return APIType; }

    void update();

    void updateStringValueForKey(const String& key, const String& value);
    void updateBoolValueForKey(const String& key, bool value);
    void updateUInt32ValueForKey(const String& key, uint32_t value);
    void updateDoubleValueForKey(const String& key, double value);
    void platformUpdateStringValueForKey(const String& key, const String& value);
    void platformUpdateBoolValueForKey(const String& key, bool value);
    void platformUpdateUInt32ValueForKey(const String& key, uint32_t value);
    void platformUpdateDoubleValueForKey(const String& key, double value);

    HashSet<WebPageGroup*> m_pageGroups;
    WebPreferencesStore m_store;
    String m_identifier;
};

} // namespace WebKit

#endif // WebPreferences_h

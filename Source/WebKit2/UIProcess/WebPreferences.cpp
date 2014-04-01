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

#include "config.h"
#include "WebPreferences.h"

#include "WebContext.h"
#include "WebPageGroup.h"
#include <wtf/ThreadingPrimitives.h>

namespace WebKit {

// FIXME: Manipulating this variable is not thread safe.
// Instead of tracking private browsing state as a boolean preference, we should let the client provide storage sessions explicitly.
static unsigned privateBrowsingPageCount;

WebPreferences::WebPreferences(const String& identifier, const String& keyPrefix)
    : m_identifier(identifier)
    , m_keyPrefix(keyPrefix)
{
    platformInitializeStore();
}

WebPreferences::WebPreferences(const WebPreferences& other)
    : m_keyPrefix(other.m_keyPrefix)
    , m_store(other.m_store)
{
    platformInitializeStore();
}

WebPreferences::~WebPreferences()
{
    ASSERT(m_pages.isEmpty());
}

PassRefPtr<WebPreferences> WebPreferences::copy() const
{
    return adoptRef(new WebPreferences(*this));
}

void WebPreferences::addPage(WebPageProxy& webPageProxy)
{
    ASSERT(!m_pages.contains(&webPageProxy));
    m_pages.add(&webPageProxy);

    if (privateBrowsingEnabled()) {
        if (!privateBrowsingPageCount)
            WebContext::willStartUsingPrivateBrowsing();

        ++privateBrowsingPageCount;
    }
}

void WebPreferences::removePage(WebPageProxy& webPageProxy)
{
    ASSERT(m_pages.contains(&webPageProxy));
    m_pages.remove(&webPageProxy);

    if (privateBrowsingEnabled()) {
        --privateBrowsingPageCount;
        if (!privateBrowsingPageCount)
            WebContext::willStopUsingPrivateBrowsing();
    }
}

void WebPreferences::update()
{
    for (auto& webPageProxy : m_pages)
        webPageProxy->preferencesDidChange();
}

void WebPreferences::updateStringValueForKey(const String& key, const String& value)
{
    platformUpdateStringValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateBoolValueForKey(const String& key, bool value)
{
    if (key == WebPreferencesKey::privateBrowsingEnabledKey()) {
        updatePrivateBrowsingValue(value);
        return;
    }

    platformUpdateBoolValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateUInt32ValueForKey(const String& key, uint32_t value)
{
    platformUpdateUInt32ValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateDoubleValueForKey(const String& key, double value)
{
    platformUpdateDoubleValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updateFloatValueForKey(const String& key, float value)
{
    platformUpdateFloatValueForKey(key, value);
    update(); // FIXME: Only send over the changed key and value.
}

void WebPreferences::updatePrivateBrowsingValue(bool value)
{
    platformUpdateBoolValueForKey(WebPreferencesKey::privateBrowsingEnabledKey(), value);

    unsigned pagesChanged = m_pages.size();
    if (!pagesChanged)
        return;

    if (value) {
        if (!privateBrowsingPageCount)
            WebContext::willStartUsingPrivateBrowsing();
        privateBrowsingPageCount += pagesChanged;
    }

    update(); // FIXME: Only send over the changed key and value.

    if (!value) {
        ASSERT(privateBrowsingPageCount >= pagesChanged);
        privateBrowsingPageCount -= pagesChanged;
        if (!privateBrowsingPageCount)
            WebContext::willStopUsingPrivateBrowsing();
    }
}

#define DEFINE_PREFERENCE_GETTER_AND_SETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) \
    void WebPreferences::set##KeyUpper(const Type& value) \
    { \
        if (!m_store.set##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key(), value)) \
            return; \
        update##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key(), value); \
        \
    } \
    \
    Type WebPreferences::KeyLower() const \
    { \
        return m_store.get##TypeName##ValueForKey(WebPreferencesKey::KeyLower##Key()); \
    } \

FOR_EACH_WEBKIT_PREFERENCE(DEFINE_PREFERENCE_GETTER_AND_SETTERS)

#undef DEFINE_PREFERENCE_GETTER_AND_SETTERS

bool WebPreferences::anyPagesAreUsingPrivateBrowsing()
{
    return privateBrowsingPageCount;
}

} // namespace WebKit

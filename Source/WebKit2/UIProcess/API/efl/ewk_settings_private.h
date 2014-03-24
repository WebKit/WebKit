/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef ewk_settings_private_h
#define ewk_settings_private_h

#include "WKEinaSharedString.h"
#include "WKRetainPtr.h"
#include <WebKit2/WKBase.h>

class EwkView;
/**
 * \struct  Ewk_Settings
 * @brief   Contains the settings data.
 */
class EwkSettings {
public:
    explicit EwkSettings(WKPreferencesRef preferences)
        : m_preferences(preferences)
    {
        ASSERT(preferences);
        m_defaultTextEncodingName = WKPreferencesCopyDefaultTextEncodingName(preferences);
    }

    WKPreferencesRef preferences() const { return m_preferences.get(); }

    const char* defaultTextEncodingName() const;
    void setDefaultTextEncodingName(const char*);

private:
    WKRetainPtr<WKPreferencesRef> m_preferences;
    WKEinaSharedString m_defaultTextEncodingName;
};

#endif // ewk_settings_private_h

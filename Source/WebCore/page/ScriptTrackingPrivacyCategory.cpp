/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ScriptTrackingPrivacyCategory.h"

#include "AdvancedPrivacyProtections.h"
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

ASCIILiteral description(ScriptTrackingPrivacyCategory category)
{
    switch (category) {
    case ScriptTrackingPrivacyCategory::Audio:
        return "Audio"_s;
    case ScriptTrackingPrivacyCategory::Canvas:
        return "Canvas"_s;
    case ScriptTrackingPrivacyCategory::Cookies:
        return "Cookies"_s;
    case ScriptTrackingPrivacyCategory::HardwareConcurrency:
        return "HardwareConcurrency"_s;
    case ScriptTrackingPrivacyCategory::LocalStorage:
        return "LocalStorage"_s;
    case ScriptTrackingPrivacyCategory::Payments:
        return "Payments"_s;
    case ScriptTrackingPrivacyCategory::QueryParameters:
        return "QueryParameters"_s;
    case ScriptTrackingPrivacyCategory::Referrer:
        return "Referrer"_s;
    case ScriptTrackingPrivacyCategory::ScreenOrViewport:
        return "ScreenOrViewport"_s;
    case ScriptTrackingPrivacyCategory::Speech:
        return "Speech"_s;
    case ScriptTrackingPrivacyCategory::FormControls:
        return "FormControls"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ScriptTrackingPrivacyFlag scriptCategoryAsFlag(ScriptTrackingPrivacyCategory category)
{
    switch (category) {
    case ScriptTrackingPrivacyCategory::Audio:
        return ScriptTrackingPrivacyFlag::Audio;
    case ScriptTrackingPrivacyCategory::Canvas:
        return ScriptTrackingPrivacyFlag::Canvas;
    case ScriptTrackingPrivacyCategory::Cookies:
        return ScriptTrackingPrivacyFlag::Cookies;
    case ScriptTrackingPrivacyCategory::HardwareConcurrency:
        return ScriptTrackingPrivacyFlag::HardwareConcurrency;
    case ScriptTrackingPrivacyCategory::LocalStorage:
        return ScriptTrackingPrivacyFlag::LocalStorage;
    case ScriptTrackingPrivacyCategory::Payments:
        return ScriptTrackingPrivacyFlag::Payments;
    case ScriptTrackingPrivacyCategory::QueryParameters:
        return ScriptTrackingPrivacyFlag::QueryParameters;
    case ScriptTrackingPrivacyCategory::Referrer:
        return ScriptTrackingPrivacyFlag::Referrer;
    case ScriptTrackingPrivacyCategory::ScreenOrViewport:
        return ScriptTrackingPrivacyFlag::ScreenOrViewport;
    case ScriptTrackingPrivacyCategory::Speech:
        return ScriptTrackingPrivacyFlag::Speech;
    case ScriptTrackingPrivacyCategory::FormControls:
        return ScriptTrackingPrivacyFlag::FormControls;
    }
    ASSERT_NOT_REACHED();
    return ScriptTrackingPrivacyFlag::FormControls;
}

bool shouldEnableScriptTrackingPrivacy(ScriptTrackingPrivacyCategory category, OptionSet<AdvancedPrivacyProtections> protections)
{
    if (protections.contains(AdvancedPrivacyProtections::BaselineProtections))
        return true;

    return category != ScriptTrackingPrivacyCategory::FormControls;
}

String makeLogMessage(const URL& url, ScriptTrackingPrivacyCategory category)
{
    if (category == ScriptTrackingPrivacyCategory::Cookies)
        return makeString("Prevented "_s, url.string(), " from setting long-lived cookies"_s);
    return makeString("Prevented "_s, url.string(), " from accessing "_s, description(category));
}

} // namespace WebCore

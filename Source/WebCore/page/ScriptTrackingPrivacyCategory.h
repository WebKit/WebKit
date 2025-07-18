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

#pragma once

#include <wtf/HashTraits.h>
#include <wtf/OptionSet.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

enum class AdvancedPrivacyProtections : uint16_t;

enum class ScriptTrackingPrivacyCategory : uint8_t {
    Audio,
    Canvas,
    Cookies,
    HardwareConcurrency,
    LocalStorage,
    Payments,
    QueryParameters,
    Referrer,
    ScreenOrViewport,
    Speech,
    FormControls,
};

enum class ScriptTrackingPrivacyFlag : uint16_t {
    Audio                   = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Audio),
    Canvas                  = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Canvas),
    Cookies                 = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Cookies),
    HardwareConcurrency     = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::HardwareConcurrency),
    LocalStorage            = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::LocalStorage),
    Payments                = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Payments),
    QueryParameters         = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::QueryParameters),
    Referrer                = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Referrer),
    ScreenOrViewport        = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::ScreenOrViewport),
    Speech                  = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::Speech),
    FormControls            = 1 << static_cast<uint8_t>(ScriptTrackingPrivacyCategory::FormControls),
};

using ScriptTrackingPrivacyFlags = OptionSet<ScriptTrackingPrivacyFlag>;

String makeLogMessage(const URL&, ScriptTrackingPrivacyCategory);
ASCIILiteral description(ScriptTrackingPrivacyCategory);
WEBCORE_EXPORT ScriptTrackingPrivacyFlag scriptCategoryAsFlag(ScriptTrackingPrivacyCategory);

bool shouldEnableScriptTrackingPrivacy(ScriptTrackingPrivacyCategory, OptionSet<AdvancedPrivacyProtections>);

} // namespace WebCore

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebCore::ScriptTrackingPrivacyCategory> : public IntHash<WebCore::ScriptTrackingPrivacyCategory> { };

template<typename T> struct HashTraits;
template<> struct HashTraits<WebCore::ScriptTrackingPrivacyCategory> : public StrongEnumHashTraits<WebCore::ScriptTrackingPrivacyCategory> { };

} // namespace WTF

/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FeaturePolicy.h"

#include "Document.h"
#include "HTMLParserIdioms.h"
#include "SecurityOrigin.h"

namespace WebCore {

static bool isAllowedByFeaturePolicy(const FeaturePolicy::AllowRule& rule, const SecurityOriginData& origin)
{
    switch (rule.type) {
    case FeaturePolicy::AllowRule::Type::None:
        return false;
    case FeaturePolicy::AllowRule::Type::All:
        return true;
    case FeaturePolicy::AllowRule::Type::List:
        return rule.allowedList.contains(origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static inline void processOriginItem(Document& document, FeaturePolicy::AllowRule& rule, StringView item)
{
    if (rule.type == FeaturePolicy::AllowRule::Type::None)
        return;

    item = item.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    // FIXME: Support 'src'.
    if (item == "'src'")
        return;

    if (item == "*") {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    if (item == "'self'") {
        rule.allowedList.add(document.securityOrigin().data());
        return;
    }
    if (item == "'none'") {
        rule.type = FeaturePolicy::AllowRule::Type::None;
        return;
    }
    URL url { { }, item.toString() };
    if (url.isValid())
        rule.allowedList.add(SecurityOriginData::fromURL(url));
}

static inline void updateList(Document& document, FeaturePolicy::AllowRule& rule, StringView value)
{
    // We keep the empty string value equivalent to '*' for existing websites.
    if (value.isEmpty()) {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    while (!value.isEmpty()) {
        auto position = value.find(isHTMLSpace<UChar>);
        if (position == notFound) {
            processOriginItem(document, rule, value);
            return;
        }

        processOriginItem(document, rule, value.substring(0, position));
        value = value.substring(position + 1).stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    }
}

FeaturePolicy FeaturePolicy::parse(Document& document, StringView allowAttributeValue)
{
    FeaturePolicy policy;
    bool isCameraInitialized = false;
    bool isMicrophoneInitialized = false;
    bool isDisplayCaptureInitialized = false;
    for (auto allowItem : allowAttributeValue.split(';')) {
        auto item = allowItem.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
        if (item.startsWith("camera")) {
            isCameraInitialized = true;
            updateList(document, policy.m_cameraRule, item.substring(7));
            continue;
        }
        if (item.startsWith("microphone")) {
            isMicrophoneInitialized = true;
            updateList(document, policy.m_microphoneRule, item.substring(11));
            continue;
        }
        if (item.startsWith("display-capture")) {
            isDisplayCaptureInitialized = true;
            updateList(document, policy.m_displayCaptureRule, item.substring(16));
            continue;
        }
    }

    // By default, camera, microphone and display-capture policy is 'self'
    if (!isCameraInitialized)
        policy.m_cameraRule.allowedList.add(document.securityOrigin().data());
    if (!isMicrophoneInitialized)
        policy.m_microphoneRule.allowedList.add(document.securityOrigin().data());
    if (!isDisplayCaptureInitialized)
        policy.m_displayCaptureRule.allowedList.add(document.securityOrigin().data());

    return policy;
}

bool FeaturePolicy::allows(Type type, const SecurityOriginData& origin) const
{
    switch (type) {
    case Type::Camera:
        return isAllowedByFeaturePolicy(m_cameraRule, origin);
    case Type::Microphone:
        return isAllowedByFeaturePolicy(m_microphoneRule, origin);
    case Type::DisplayCapture:
        return isAllowedByFeaturePolicy(m_displayCaptureRule, origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

}

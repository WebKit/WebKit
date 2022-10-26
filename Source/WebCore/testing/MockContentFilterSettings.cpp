/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "MockContentFilterSettings.h"

#if ENABLE(CONTENT_FILTERING)

#include "ContentFilter.h"
#include "ContentFilterUnblockHandler.h"
#include "MockContentFilter.h"
#include "MockContentFilterManager.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MockContentFilterSettings& MockContentFilterSettings::singleton()
{
    MockContentFilter::ensureInstalled();
    static NeverDestroyed<MockContentFilterSettings> settings;
    return settings;
}

void MockContentFilterSettings::reset()
{
    singleton() = MockContentFilterSettings();
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setEnabled(bool enabled)
{
    m_enabled = enabled;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setBlockedString(const String& blockedString)
{
    m_blockedString = blockedString;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setDecisionPoint(DecisionPoint decisionPoint)
{
    m_decisionPoint = decisionPoint;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setDecision(Decision decision)
{
    m_decision = decision;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setUnblockRequestDecision(Decision unblockRequestDecision)
{
    m_unblockRequestDecision = unblockRequestDecision;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

void MockContentFilterSettings::setModifiedRequestURL(const String& modifiedRequestURL)
{
    m_modifiedRequestURL = modifiedRequestURL;
    MockContentFilterManager::singleton().notifySettingsChanged(singleton());
}

const String& MockContentFilterSettings::unblockRequestURL() const
{
    static NeverDestroyed<String> unblockRequestURL = makeString(ContentFilter::urlScheme(), "://", unblockURLHost());
    return unblockRequestURL;
}

}; // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)

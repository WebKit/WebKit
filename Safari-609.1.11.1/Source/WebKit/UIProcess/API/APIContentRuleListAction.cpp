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
#include "APIContentRuleListAction.h"

namespace API {

Ref<ContentRuleListAction> ContentRuleListAction::create(WebCore::ContentRuleListResults::Result&& result)
{
    return adoptRef(*new ContentRuleListAction(WTFMove(result)));
}

ContentRuleListAction::ContentRuleListAction(WebCore::ContentRuleListResults::Result&& result)
    : m_result(WTFMove(result))
{
}

ContentRuleListAction::~ContentRuleListAction() = default;

bool ContentRuleListAction::blockedLoad() const
{
    return m_result.blockedLoad;
}

bool ContentRuleListAction::madeHTTPS() const
{
    return m_result.madeHTTPS;
}

bool ContentRuleListAction::blockedCookies() const
{
    return m_result.blockedCookies;
}

const Vector<WTF::String>& ContentRuleListAction::notifications() const
{
    return m_result.notifications;
}

} // namespace API

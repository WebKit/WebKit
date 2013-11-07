/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PolicyCallback.h"

#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFormElement.h"

namespace WebCore {

PolicyCallback::PolicyCallback()
{
}

PolicyCallback::~PolicyCallback()
{
}

void PolicyCallback::clear()
{
    clearRequest();
    m_navigationFunction = nullptr;
    m_newWindowFunction = nullptr;
    m_contentFunction = nullptr;
}

void PolicyCallback::set(const ResourceRequest& request, PassRefPtr<FormState> formState,
    NavigationPolicyDecisionFunction function)
{
    m_request = request;
    m_formState = formState;
    m_frameName = String();

    m_navigationFunction = std::move(function);
    m_newWindowFunction = nullptr;
    m_contentFunction = nullptr;
}

void PolicyCallback::set(const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& navigationAction, NewWindowPolicyDecisionFunction function)
{
    m_request = request;
    m_formState = formState;
    m_frameName = frameName;
    m_navigationAction = navigationAction;

    m_navigationFunction = nullptr;
    m_newWindowFunction = std::move(function);
    m_contentFunction = nullptr;
}

void PolicyCallback::set(ContentPolicyDecisionFunction function)
{
    m_request = ResourceRequest();
    m_formState = nullptr;
    m_frameName = String();

    m_navigationFunction = nullptr;
    m_newWindowFunction = nullptr;
    m_contentFunction = std::move(function);
}

void PolicyCallback::call(bool shouldContinue)
{
    if (m_navigationFunction)
        m_navigationFunction(m_request, m_formState.get(), shouldContinue);
    if (m_newWindowFunction)
        m_newWindowFunction(m_request, m_formState.get(), m_frameName, m_navigationAction, shouldContinue);
    ASSERT(!m_contentFunction);
}

void PolicyCallback::call(PolicyAction action)
{
    ASSERT(!m_navigationFunction);
    ASSERT(!m_newWindowFunction);
    ASSERT(m_contentFunction);
    m_contentFunction(action);
}

void PolicyCallback::clearRequest()
{
    m_request = ResourceRequest();
    m_formState = nullptr;
    m_frameName = String();
}

void PolicyCallback::cancel()
{
    clearRequest();
    if (m_navigationFunction)
        m_navigationFunction(m_request, m_formState.get(), false);
    if (m_newWindowFunction)
        m_newWindowFunction(m_request, m_formState.get(), m_frameName, m_navigationAction, false);
    if (m_contentFunction)
        m_contentFunction(PolicyIgnore);
}

} // namespace WebCore

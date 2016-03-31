/*
 * Copyright (C) 2006-2016 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "NavigationAction.h"

#include "Event.h"
#include "FrameLoader.h"
#include "ScriptController.h"

namespace WebCore {

static NavigationType navigationType(FrameLoadType frameLoadType, bool isFormSubmission, bool haveEvent)
{
    if (isFormSubmission)
        return NavigationType::FormSubmitted;
    if (haveEvent)
        return NavigationType::LinkClicked;
    if (frameLoadType == FrameLoadType::Reload || frameLoadType == FrameLoadType::ReloadFromOrigin)
        return NavigationType::Reload;
    if (isBackForwardLoadType(frameLoadType))
        return NavigationType::BackForward;
    return NavigationType::Other;
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, NavigationType type, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, const AtomicString& downloadAttribute)
    : m_resourceRequest(resourceRequest)
    , m_type(type)
    , m_event(event)
    , m_processingUserGesture(ScriptController::processingUserGesture())
    , m_shouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy)
    , m_downloadAttribute(downloadAttribute)
{
}

NavigationAction::NavigationAction()
    : NavigationAction(ResourceRequest(), NavigationType::Other, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest)
    : NavigationAction(resourceRequest, NavigationType::Other, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
    : NavigationAction(resourceRequest, NavigationType::Other, nullptr, shouldOpenExternalURLsPolicy, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, NavigationType type)
    : NavigationAction(resourceRequest, type, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, NavigationType type, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
    : NavigationAction(resourceRequest, type, event, shouldOpenExternalURLsPolicy, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType, bool isFormSubmission)
    : NavigationAction(resourceRequest, navigationType(frameLoadType, isFormSubmission, 0), nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, NavigationType type, Event* event)
    : NavigationAction(resourceRequest, type, event, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, NavigationType type, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
    : NavigationAction(resourceRequest, type, nullptr, shouldOpenExternalURLsPolicy, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType, bool isFormSubmission, Event* event)
    : NavigationAction(resourceRequest, navigationType(frameLoadType, isFormSubmission, event), event, ShouldOpenExternalURLsPolicy::ShouldNotAllow, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType, bool isFormSubmission, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
    : NavigationAction(resourceRequest, navigationType(frameLoadType, isFormSubmission, event), event, shouldOpenExternalURLsPolicy, nullAtom)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType, bool isFormSubmission, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, const AtomicString& downloadAttribute)
    : NavigationAction(resourceRequest, navigationType(frameLoadType, isFormSubmission, event), event, shouldOpenExternalURLsPolicy, downloadAttribute)
{
}

NavigationAction NavigationAction::copyWithShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy) const
{
    NavigationAction result(*this);
    result.m_shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    return result;
}

}

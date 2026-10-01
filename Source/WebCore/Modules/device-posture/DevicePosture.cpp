/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DevicePosture.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "DocumentPage.h"
#include "Event.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "Navigator.h"
#include "Page.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DevicePosture);

Ref<DevicePosture> DevicePosture::create(Navigator& navigator)
{
    Ref devicePosture = adoptRef(*new DevicePosture(navigator));
    devicePosture->suspendIfNeeded();
    return devicePosture;
}

DevicePosture::DevicePosture(Navigator& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_navigator(navigator)
{
}

DevicePosture::~DevicePosture()
{
}

bool DevicePosture::virtualHasPendingActivity() const
{
    return hasEventListeners();
}

Navigator* DevicePosture::navigator()
{
    return m_navigator.get();
}

ScriptExecutionContext* DevicePosture::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

DevicePostureType DevicePosture::type() const
{
    RefPtr navigator = m_navigator.get();
    if (!navigator)
        return DevicePostureType::Continuous;

    RefPtr document = navigator->document();
    if (!document)
        return DevicePostureType::Continuous;

    RefPtr page = document->page();
    if (!page)
        return DevicePostureType::Continuous;

    return page->chrome().client().devicePostureType();
}

void DevicePosture::typeChanged()
{
    queueTaskToDispatchEvent(*this, TaskSource::UserInteraction, Event::create(eventNames().changeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

} // namespace WebCore

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
#include "InspectorAuditDOMObject.h"

#include "Node.h"
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

#define ERROR_IF_NO_ACTIVE_AUDIT() \
    if (!m_auditAgent.hasActiveAudit()) \
        return Exception { NotAllowedError, "Cannot be called outside of a Web Inspector Audit"_s };

InspectorAuditDOMObject::InspectorAuditDOMObject(InspectorAuditAgent& auditAgent)
    : m_auditAgent(auditAgent)
{
}

ExceptionOr<bool> InspectorAuditDOMObject::hasEventListeners(Node& node, const String& type)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (EventTargetData* eventTargetData = node.eventTargetData()) {
        Vector<AtomString> eventTypes;
        if (type.isNull())
            eventTypes = eventTargetData->eventListenerMap.eventTypes();
        else
            eventTypes.append(type);

        for (AtomString& type : eventTypes) {
            for (const RefPtr<RegisteredEventListener>& listener : node.eventListeners(type)) {
                if (listener->callback().type() == EventListener::JSEventListenerType)
                    return true;
            }
        }
    }

    return false;
}

} // namespace WebCore

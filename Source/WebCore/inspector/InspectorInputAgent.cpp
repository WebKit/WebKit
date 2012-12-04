/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorInputAgent.h"

#include "EventHandler.h"
#include "Frame.h"
#include "Page.h"
#include "PlatformEvent.h"
#include "PlatformKeyboardEvent.h"

#include <wtf/CurrentTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

InspectorInputAgent::InspectorInputAgent(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState, Page* page)
    : InspectorBaseAgent<InspectorInputAgent>("Input", instrumentingAgents, inspectorState)
    , m_page(page)
{
}

InspectorInputAgent::~InspectorInputAgent()
{
}

void InspectorInputAgent::dispatchKeyEvent(ErrorString* error, const String& type, const int* modifiers, const double* timestamp, const String* text, const String* unmodifiedText, const String* keyIdentifier, const int* windowsVirtualKeyCode, const int* nativeVirtualKeyCode, const int* macCharCode, const bool* autoRepeat, const bool* isKeypad, const bool* isSystemKey)
{
    PlatformEvent::Type convertedType;
    if (type == "keyDown")
        convertedType = PlatformEvent::KeyDown;
    else if (type == "keyUp")
        convertedType = PlatformEvent::KeyUp;
    else if (type == "char")
        convertedType = PlatformEvent::Char;
    else if (type == "rawKeyDown")
        convertedType = PlatformEvent::RawKeyDown;
    else {
        *error = "Unrecognized type: " + type;
        return;
    }

    PlatformKeyboardEvent event(
        convertedType,
        text ? *text : "",
        unmodifiedText ? *unmodifiedText : "",
        keyIdentifier ? *keyIdentifier : "",
        windowsVirtualKeyCode ? *windowsVirtualKeyCode : 0,
        nativeVirtualKeyCode ? *nativeVirtualKeyCode : 0,
        macCharCode ? *macCharCode : 0,
        autoRepeat ? *autoRepeat : false,
        isKeypad ? *isKeypad : false,
        isSystemKey ? *isSystemKey : false,
        static_cast<PlatformEvent::Modifiers>(modifiers ? *modifiers : 0),
        timestamp ? *timestamp : currentTime());
    m_page->mainFrame()->eventHandler()->keyEvent(event);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

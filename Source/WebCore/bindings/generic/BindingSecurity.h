/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef BindingSecurity_h
#define BindingSecurity_h

#include "BindingSecurityBase.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "GenericBinding.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ScriptController.h"
#include "Settings.h"

namespace WebCore {

class DOMWindow;
class Node;

class BindingSecurity : public BindingSecurityBase {
public:
    static bool canAccessFrame(BindingState*, Frame*, bool reportError);
    static bool shouldAllowAccessToNode(BindingState*, Node* target);
    static bool allowPopUp(BindingState*);
    static bool allowSettingFrameSrcToJavascriptUrl(BindingState*, HTMLFrameElementBase*, const String& value);
    static bool allowSettingSrcToJavascriptURL(BindingState*, Element*, const String& name, const String& value);

private:
    static bool canAccessWindow(BindingState*, DOMWindow* target);
};

inline bool BindingSecurity::canAccessWindow(BindingState* state, DOMWindow* targetWindow)
{
    DOMWindow* active = activeWindow(state);
    return canAccess(active, targetWindow);
}

inline bool BindingSecurity::canAccessFrame(BindingState* state, Frame* target, bool reportError)
{
    // The subject is detached from a frame, deny accesses.
    if (!target)
        return false;

    if (!canAccessWindow(state, getDOMWindow(target))) {
        if (reportError)
            immediatelyReportUnsafeAccessTo(state, target);
        return false;
    }
    return true;
}

inline bool BindingSecurity::shouldAllowAccessToNode(BindingState* state, Node* node)
{
    if (!node)
        return false;

    Frame* target = getFrame(node);
    if (!target)
        return false;

    return canAccessFrame(state, target, true);
}

inline bool BindingSecurity::allowPopUp(BindingState* state)
{
    if (ScriptController::processingUserGesture())
        return true;

    Frame* frame = firstFrame(state);
    ASSERT(frame);
    Settings* settings = frame->settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

inline bool BindingSecurity::allowSettingFrameSrcToJavascriptUrl(BindingState* state, HTMLFrameElementBase* frame, const String& value)
{
    if (protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(value))) {
        Node* contentDoc = frame->contentDocument();
        if (contentDoc && !shouldAllowAccessToNode(state, contentDoc))
            return false;
    }
    return true;
}

inline bool BindingSecurity::allowSettingSrcToJavascriptURL(BindingState* state, Element* element, const String& name, const String& value)
{
    if ((element->hasTagName(HTMLNames::iframeTag) || element->hasTagName(HTMLNames::frameTag)) && equalIgnoringCase(name, "src"))
        return allowSettingFrameSrcToJavascriptUrl(state, static_cast<HTMLFrameElementBase*>(element), value);
    return true;
}

}

#endif // BindingSecurity_h

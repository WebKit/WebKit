/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSHTMLFrameElement.h"

#include "Document.h"
#include "HTMLFrameElement.h"
#include "PlatformString.h"
#include "kjs_binding.h"
#include "kjs_dom.h"

namespace WebCore {

static inline bool allowSettingJavascriptURL(KJS::ExecState* exec, HTMLFrameElement* imp, String value)
{
    if (value.startsWith("javascript:", false)) {
        if (!checkNodeSecurity(exec, imp->contentDocument()))
            return false;
    }
    return true;
} 

void JSHTMLFrameElement::setSrc(KJS::ExecState* exec, KJS::JSValue* value)
{
    HTMLFrameElement* imp = static_cast<HTMLFrameElement*>(impl());
    String srcValue = KJS::valueToStringWithNullCheck(exec, value);

    if (!allowSettingJavascriptURL(exec, imp, srcValue))
        return;

    imp->setSrc(srcValue);
    return;
}

void JSHTMLFrameElement::setLocation(KJS::ExecState* exec, KJS::JSValue* value)
{
    HTMLFrameElement* imp = static_cast<HTMLFrameElement*>(impl());
    String locationValue = KJS::valueToStringWithNullCheck(exec, value);

    if (!allowSettingJavascriptURL(exec, imp, locationValue))
        return;

    imp->setLocation(locationValue);
    return;
}

} // namespace WebCore

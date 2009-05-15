/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "JSHTMLInputElement.h"

#include "Document.h"
#include "HTMLInputElement.h"
#include "Settings.h"

using namespace JSC;

namespace WebCore {

static bool needsGmailQuirk(HTMLInputElement* input)
{
    Document* document = input->document();

    const KURL& url = document->url();
    if (url.host() != "mail.google.com")
        return false;

    // As with other site-specific quirks, allow website developers to turn this off.
    // In theory, this allows website developers to check if their fixes are effective.
    Settings* settings = document->settings();
    if (!settings)
        return false;
    if (!settings->needsSiteSpecificQuirks())
        return false;

    return true;
}

JSValue JSHTMLInputElement::type(ExecState* exec) const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    const AtomicString& type = input->type();

    DEFINE_STATIC_LOCAL(const AtomicString, url, ("url"));
    DEFINE_STATIC_LOCAL(const AtomicString, text, ("text"));

    if (type == url && needsGmailQuirk(input))
        return jsString(exec, text);
    return jsString(exec, type);
}

JSValue JSHTMLInputElement::selectionStart(ExecState* exec) const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        return throwError(exec, TypeError);

    return jsNumber(exec, input->selectionStart());
}

void JSHTMLInputElement::setSelectionStart(ExecState* exec, JSValue value)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        throwError(exec, TypeError);

    input->setSelectionStart(value.toInt32(exec));
}

JSValue JSHTMLInputElement::selectionEnd(ExecState* exec) const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        return throwError(exec, TypeError);

    return jsNumber(exec, input->selectionEnd());
}

void JSHTMLInputElement::setSelectionEnd(ExecState* exec, JSValue value)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        throwError(exec, TypeError);

    input->setSelectionEnd(value.toInt32(exec));
}

JSValue JSHTMLInputElement::setSelectionRange(ExecState* exec, const ArgList& args)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        return throwError(exec, TypeError);

    int start = args.at(0).toInt32(exec);
    int end = args.at(1).toInt32(exec);

    input->setSelectionRange(start, end);
    return jsUndefined();
}

} // namespace WebCore

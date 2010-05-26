/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * This file is derived by hand from an automatically generated file.
 * Keeping it up-to-date could potentially be done by adding
 * a make_names.pl generator, or by writing a separate
 * generater which takes JSHTMLElementWrapperFactory.h as input.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitDOMEventTarget.h"

#include "WebKitDOMEvent.h"

typedef WebKitDOMEventTargetIface WebKitDOMEventTargetInterface;
G_DEFINE_INTERFACE(WebKitDOMEventTarget, webkit_dom_event_target, G_TYPE_OBJECT)

static void
webkit_dom_event_target_default_init(WebKitDOMEventTargetIface* iface)
{
}

void webkit_dom_event_target_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebKitDOMEventTargetIface* iface;

    g_return_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target));
    g_return_if_fail(WEBKIT_DOM_IS_EVENT(event));

    iface = WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target);

    if (iface->dispatch_event)
        iface->dispatch_event(target, event, error);
}

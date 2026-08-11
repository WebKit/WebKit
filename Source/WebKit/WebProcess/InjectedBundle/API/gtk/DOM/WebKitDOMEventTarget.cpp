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
#include "WebKitDOMEventTarget.h"

#include "DOMObjectCache.h"
#include <WebCore/EventTarget.h>
#include "WebKitDOMEvent.h"
#include "WebKitDOMEventTargetPrivate.h"
#include "WebKitDOMPrivate.h"
#include <wtf/glib/GRefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

typedef WebKitDOMEventTargetIface WebKitDOMEventTargetInterface;

G_DEFINE_INTERFACE(WebKitDOMEventTarget, webkit_dom_event_target, G_TYPE_OBJECT)

static void webkit_dom_event_target_default_init(WebKitDOMEventTargetIface*)
{
}

gboolean webkit_dom_event_target_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target), FALSE);
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT(event), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);

    return WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target)->dispatch_event(target, event, error);
}

gboolean webkit_dom_event_target_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GCallback handler, gboolean useCapture, gpointer userData)
{

    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target), FALSE);
    g_return_val_if_fail(eventName, FALSE);

    GRefPtr<GClosure> closure = adoptGRef(g_cclosure_new(handler, userData, 0));
    return WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target)->add_event_listener(target, eventName, closure.get(), useCapture);
}

gboolean webkit_dom_event_target_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GCallback handler, gboolean useCapture)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target), FALSE);
    g_return_val_if_fail(eventName, FALSE);

    GRefPtr<GClosure> closure = adoptGRef(g_cclosure_new(handler, 0, 0));
    return WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target)->remove_event_listener(target, eventName, closure.get(), useCapture);
}

gboolean webkit_dom_event_target_add_event_listener_with_closure(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target), FALSE);
    g_return_val_if_fail(eventName, FALSE);
    g_return_val_if_fail(handler, FALSE);

    return WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target)->add_event_listener(target, eventName, handler, useCapture);
}

gboolean webkit_dom_event_target_remove_event_listener_with_closure(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_EVENT_TARGET(target), FALSE);
    g_return_val_if_fail(eventName, FALSE);
    g_return_val_if_fail(handler, FALSE);

    return WEBKIT_DOM_EVENT_TARGET_GET_IFACE(target)->remove_event_listener(target, eventName, handler, useCapture);
}

namespace WebKit {

WebKitDOMEventTarget* kit(WebCore::EventTarget* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_EVENT_TARGET(ret);

    return wrap(obj);
}

WebCore::EventTarget* core(WebKitDOMEventTarget* request)
{
    return request ? static_cast<WebCore::EventTarget*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

} // namespace WebKit

G_GNUC_END_IGNORE_DEPRECATIONS;

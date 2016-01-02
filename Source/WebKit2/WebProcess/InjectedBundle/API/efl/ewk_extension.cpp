/*
 * Copyright (C) 2014 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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
#include "ewk_extension.h"

#include "InjectedBundle.h"
#include "NotImplemented.h"
#include "WKBundle.h"
#include "WKBundleAPICast.h"
#include "WKRetainPtr.h"
#include "WebPage.h"
#include "ewk_extension_private.h"
#include "ewk_page_private.h"
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

static inline EwkExtension* toEwkExtension(const void* clientInfo)
{
    return const_cast<EwkExtension*>(static_cast<const EwkExtension*>(clientInfo));
}

EwkExtension::EwkExtension(InjectedBundle* bundle)
    : m_bundle(bundle)
{
    WKBundleClientV1 wkBundleClient = {
        {
            1, // version
            this, // clientInfo
        },
        didCreatePage,
        willDestroyPage,
        0, // didInitializePageGroup
        didReceiveMessage,
        didReceiveMessageToPage
    };
    WKBundleSetClient(toAPI(bundle), &wkBundleClient.base);
}

void EwkExtension::append(Ewk_Extension_Client* client)
{
    m_clients.append(client);
}

void EwkExtension::remove(Ewk_Extension_Client* client)
{
    m_clients.removeFirst(client);
}

void EwkExtension::didCreatePage(WKBundleRef, WKBundlePageRef wkPage, const void* clientInfo)
{
    EwkExtension* self = toEwkExtension(clientInfo);
    WebPage* page = toImpl(wkPage);

    auto ewkPage = std::make_unique<EwkPage>(page);
    for (auto& it : self->m_clients) {
        if (it->page_add)
            it->page_add(ewkPage.get(), it->data);
    }

    self->m_pageMap.add(page, WTFMove(ewkPage));
}

void EwkExtension::willDestroyPage(WKBundleRef, WKBundlePageRef wkPage, const void* clientInfo)
{
    EwkExtension* self = toEwkExtension(clientInfo);
    WebPage* page = toImpl(wkPage);

    for (auto& it : self->m_clients) {
        if (it->page_del)
            it->page_del(self->m_pageMap.get(page), it->data);
    }
    self->m_pageMap.remove(page);
}

void EwkExtension::didReceiveMessage(WKBundleRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    EwkExtension* self = toEwkExtension(clientInfo);
    CString name = toImpl(messageName)->string().utf8();
    Eina_Value* value = nullptr;

    // FIXME: Need to support other types.
    if (messageBody && WKStringGetTypeID() == WKGetTypeID(messageBody)) {
        value = eina_value_new(EINA_VALUE_TYPE_STRING);
        CString body = toImpl(static_cast<WKStringRef>(messageBody))->string().utf8();

        eina_value_set(value, body.data());
    }

    for (auto& it : self->m_clients) {
        if (it->message_received)
            it->message_received(name.data(), value, it->data);
    }

    if (value)
        eina_value_free(value);
}

void EwkExtension::didReceiveMessageToPage(WKBundleRef, WKBundlePageRef, WKStringRef, WKTypeRef, const void*)
{
    notImplemented();
}

Eina_Bool ewk_extension_client_add(Ewk_Extension* extension, Ewk_Extension_Client* client)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(extension, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(client, false);

    extension->append(client);

    return true;
}

Eina_Bool ewk_extension_client_del(Ewk_Extension* extension, Ewk_Extension_Client* client)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(extension, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(client, false);

    extension->remove(client);

    return true;
}

Eina_Bool ewk_extension_message_post(Ewk_Extension *extension, const char *name, const Eina_Value *body)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(extension, false);

    const Eina_Value_Type* type = eina_value_type_get(body);
    if (type != EINA_VALUE_TYPE_STRINGSHARE && type != EINA_VALUE_TYPE_STRING)
        return false;

    const char* value;
    eina_value_get(body, &value);

    WKRetainPtr<WKTypeRef> messageBody = adoptWK(WKStringCreateWithUTF8CString(value));
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString(name));
    WKBundlePostMessage(toAPI(extension->bundle()), messageName.get(), messageBody.get());

    return true;
}

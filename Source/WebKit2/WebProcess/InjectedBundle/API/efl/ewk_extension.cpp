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

#include "ewk_extension_private.h"

using namespace WebKit;

void EwkExtension::append(Ewk_Extension_Client* client)
{
    m_clients.append(client);
}

void EwkExtension::remove(Ewk_Extension_Client* client)
{
    m_clients.removeFirst(client);
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

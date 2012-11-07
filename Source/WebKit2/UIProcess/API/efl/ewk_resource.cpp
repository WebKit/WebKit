/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "ewk_resource.h"

#include "ewk_resource_private.h"
#include <wtf/text/CString.h>

EwkResource::EwkResource(WKURLRef url, bool isMainResource)
    : m_url(url)
    , m_isMainResource(isMainResource)
{ }

const char* EwkResource::url() const
{
    return m_url;
}

bool EwkResource::isMainResource() const
{
    return m_isMainResource;
}

const char* ewk_resource_url_get(const Ewk_Resource* resource)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkResource, resource, impl, 0);

    return impl->url();
}

Eina_Bool ewk_resource_main_resource_get(const Ewk_Resource* resource)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkResource, resource, impl, false);

    return impl->isMainResource();
}

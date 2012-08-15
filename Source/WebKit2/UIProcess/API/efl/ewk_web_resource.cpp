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
#include "ewk_web_resource.h"

#include "WKEinaSharedString.h"
#include "ewk_web_resource_private.h"
#include <wtf/text/CString.h>

struct _Ewk_Web_Resource {
    unsigned int __ref; /**< the reference count of the object */
    WKEinaSharedString url;
    bool isMainResource;

    _Ewk_Web_Resource(const char* url, bool isMainResource)
        : __ref(1)
        , url(url)
        , isMainResource(isMainResource)
    { }

    ~_Ewk_Web_Resource()
    {
        ASSERT(!__ref);
    }
};

void ewk_web_resource_ref(Ewk_Web_Resource* resource)
{
    EINA_SAFETY_ON_NULL_RETURN(resource);

    ++resource->__ref;
}

void ewk_web_resource_unref(Ewk_Web_Resource* resource)
{
    EINA_SAFETY_ON_NULL_RETURN(resource);

    if (--resource->__ref)
        return;

    delete resource;
}

const char* ewk_web_resource_url_get(const Ewk_Web_Resource* resource)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(resource, 0);

    return resource->url;
}

/**
 * @internal
 * Constructs a Ewk_Web_Resource.
 */
Ewk_Web_Resource* ewk_web_resource_new(const char* url, bool isMainResource)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);

    return new Ewk_Web_Resource(url, isMainResource);
}

Eina_Bool ewk_web_resource_main_resource_get(const Ewk_Web_Resource* resource)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(resource, false);

    return resource->isMainResource;
}

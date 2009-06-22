/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "ApplicationCacheResource.h"
#include <stdio.h>

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

namespace WebCore {

ApplicationCacheResource::ApplicationCacheResource(const KURL& url, const ResourceResponse& response, unsigned type, PassRefPtr<SharedBuffer> data)
    : SubstituteResource(url, response, data)
    , m_type(type)
    , m_storageID(0)
{
}

void ApplicationCacheResource::addType(unsigned type) 
{
    // Caller should take care of storing the new type in database.
    m_type |= type; 
}

#ifndef NDEBUG
void ApplicationCacheResource::dumpType(unsigned type)
{
    if (type & Master)
        printf("master ");
    if (type & Manifest)
        printf("manifest ");
    if (type & Explicit)
        printf("explicit ");
    if (type & Foreign)
        printf("foreign ");
    if (type & Fallback)
        printf("fallback ");
    
    printf("\n");
}
#endif

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)

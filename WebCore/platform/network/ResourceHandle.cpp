/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"

#include "LoaderFunctions.h"
#include "Logging.h"

namespace WebCore {

ResourceHandle::ResourceHandle(const ResourceRequest& request, ResourceHandleClient* client)
    : d(new ResourceHandleInternal(this, request, client))
{
}

PassRefPtr<ResourceHandle> ResourceHandle::create(const ResourceRequest& request, ResourceHandleClient* client, DocLoader* dl)
{
    RefPtr<ResourceHandle> newLoader(new ResourceHandle(request, client));
    
    if (newLoader->start(dl))
        return newLoader.release();

    return 0;
}

const HTTPHeaderMap& ResourceHandle::requestHeaders() const
{
    return d->m_request.httpHeaderFields();
}

const KURL& ResourceHandle::url() const
{
    return d->m_request.url();
}

PassRefPtr<FormData> ResourceHandle::postData() const
{
    return d->m_request.httpBody();
}

const String& ResourceHandle::method() const
{
    return d->m_request.httpMethod();
}

ResourceHandleClient* ResourceHandle::client() const
{
    return d->m_client;
}

} // namespace WebCore

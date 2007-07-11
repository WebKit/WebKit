/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Trolltech AS
 *
 * All rights reserved.
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

#include "Frame.h"
#include "DocLoader.h"
#include "ResourceHandle.h"
#include "DeprecatedString.h"
#include "ResourceHandleInternal.h"
#include "qwebnetworkinterface_p.h"
#include "qwebpage_p.h"
#include "ChromeClientQt.h"
#include "FrameLoaderClientQt.h"
#include "Page.h"

#include "NotImplemented.h"

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    if (d->m_job)
        cancel();
}

bool ResourceHandle::start(Frame* frame)
{
    if (!frame)
        return false;

    Page *page = frame->page();
    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!page)
        return false;

    // check for (probably) broken requests
    if (d->m_request.httpMethod() != "GET" && d->m_request.httpMethod() != "POST") {
        notImplemented();
        return false;
    }

    getInternal()->m_frame = static_cast<FrameLoaderClientQt*>(frame->loader()->client())->webFrame();
    return QWebNetworkManager::self()->add(this, getInternal()->m_frame->page()->d->networkInterface);
}

void ResourceHandle::cancel()
{
    QWebNetworkManager::self()->cancel(this);
}

bool ResourceHandle::loadsBlocked()
{
    notImplemented();
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest& request)
{
    notImplemented();
    return false;
}

bool ResourceHandle::supportsBufferedData()
{
    notImplemented();
    return false;
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    notImplemented();
    return 0;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& e, ResourceResponse& r, Vector<char>& data)
{
    notImplemented();
}

 
void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
}

} // namespace WebCore

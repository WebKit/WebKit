/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#if PLATFORM(KDE)
#include <kio/job.h>
#endif

#include <QRegExp>

#include "FrameQt.h"
#include "DocLoader.h"
#include "ResourceHandle.h"
#include "DeprecatedString.h"
#include "ResourceHandleManagerQt.h"
#include "ResourceHandleInternal.h"

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start(Frame* frame)
{
    ASSERT(frame);
    FrameQt* f = QtFrame(frame);

    ResourceHandleManager::self()->add(this, f ? f->client() : 0);
    return true;
}

void ResourceHandle::cancel()
{
    ResourceHandleManager::self()->cancel(this);
}

void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
    // FIXME: wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), defers);
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

} // namespace WebCore

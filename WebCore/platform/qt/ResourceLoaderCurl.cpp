/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 
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

#include <QDebug>
#include "config.h"
#include "ResourceLoader.h"

#include "DocLoader.h"
#include "ResourceLoaderInternal.h"
#include "ResourceLoaderManager.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

ResourceLoaderInternal::~ResourceLoaderInternal()
{
}

ResourceLoader::~ResourceLoader()
{
    cancel();
}

bool ResourceLoader::start(DocLoader* docLoader)
{
    ResourceLoaderManager::get()->add(this);
    return true;
}

void ResourceLoader::cancel()
{
    ResourceLoaderManager::get()->cancel(this);
}

void ResourceLoader::assembleResponseHeaders() const
{
    if (!d->assembledResponseHeaders) {
        d->responseHeaders = DeprecatedString::fromUtf8(d->response.toUtf8(), d->response.length());
        d->assembledResponseHeaders = true;
 
        // TODO: Move the client activation to receivedResponse(), once
        // we use KIO, and receivedResponse() is called only once.
        if (d->client) {
            d->client->receivedResponse(const_cast<ResourceLoader *>(this), (char *) d->response.data());
        }

        d->response = QString(); // Reset
    }
}

void ResourceLoader::retrieveCharset() const
{
    if (!d->retrievedCharset) {
        d->retrievedCharset = true;
    }

    // TODO: We can just parse the headers here, but once we use KIO
    // we can set the response parameter to sth. else than a "char*".
    // I save my time but not implementing it for now :-)
    notImplemented();
}

void ResourceLoader::receivedResponse(char* response)
{
    Q_ASSERT(method() == "POST");

    d->assembledResponseHeaders = false;
    d->retrievedCharset = false;

    // TODO: This is flawed:
    // - usually receivedResponse() should be called _once_, when the
    //   response is available - seems very unflexible to do that with libcurl
    //   (so let's wait until it dies and do it properly with KIO then.)
    // - QString::fromLatin1() is also wrong, of course.
    //
    // Anyway, let's collect the response data here, as the ResourceLoaderManager
    // calls us for every line of the header it receives.
    d->response += QString::fromLatin1(response);
}

} // namespace WebCore

// vim: ts=4 sw=4 et

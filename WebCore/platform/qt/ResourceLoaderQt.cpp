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

#include <kio/job.h>

#include "DocLoader.h"
#include "ResourceLoader.h"
#include "DeprecatedString.h"
#include "ResourceLoaderManager.h"
#include "ResourceLoaderInternal.h"

namespace WebCore {

ResourceLoaderInternal::~ResourceLoaderInternal()
{
}

ResourceLoader::~ResourceLoader()
{
    cancel();
}

bool ResourceLoader::start(DocLoader*)
{
    ResourceLoaderManager::self()->add(this);
    return true;
}

void ResourceLoader::cancel()
{
    ResourceLoaderManager::self()->cancel(this);
}

void ResourceLoader::assembleResponseHeaders() const
{
    if (!d->assembledResponseHeaders) {
        d->responseHeaders = d->m_response;
        d->assembledResponseHeaders = true;
        d->m_response = QString(); // Reset
    }
}

void ResourceLoader::retrieveCharset() const
{
    if (!d->retrievedCharset) {
        d->retrievedCharset = true;

        int pos = d->responseHeaders.find("content-type:", 0, false);

        if (pos > -1) {
            pos += 13;
            int index = d->responseHeaders.find('\n', pos);
            QString type = d->responseHeaders.mid(pos, (index - pos));
            index = type.indexOf(';');

            if (index > -1) {
                QString encoding = type.mid(index + 1).remove(QRegExp("charset[ ]*=[ ]*", Qt::CaseInsensitive)).trimmed();
                d->metaData.set("charset", encoding);
            }
        }
    }
}

void ResourceLoader::receivedResponse(QString response)
{
    Q_ASSERT(method() == "POST");

    d->assembledResponseHeaders = false;
    d->retrievedCharset = false;
    d->m_response = response;

    if (d->client)
        d->client->receivedResponse(const_cast<ResourceLoader*>(this), response);
}

} // namespace WebCore

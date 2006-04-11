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
#include "TransferJob.h"
#include "TransferJobInternal.h"

#include "KWQLoader.h"
#include "Logging.h"

namespace WebCore {

TransferJob::TransferJob(TransferJobClient* client, const String& method, const KURL& url)
    : d(new TransferJobInternal(this, client, method, url))
{
}

TransferJob::TransferJob(TransferJobClient* client, const String& method, const KURL& url, const FormData& postData)
    : d(new TransferJobInternal(this, client, method, url, postData))
{
}

bool TransferJob::isErrorPage() const
{
    return d->status != 0;
}

int TransferJob::error() const
{
    return d->status;
}

void TransferJob::setError(int e)
{
    d->status = e;
}

String TransferJob::queryMetaData(const String& key) const
{
    if (key == "HTTP-Headers") {
        assembleResponseHeaders();
        return d->responseHeaders;
    } 

    if (key == "charset")
        // this will put it in the regular metadata dictionary
        retrieveCharset();

    return d->metaData.get(key); 
}

void TransferJob::addMetaData(const String& key, const String& value)
{
    d->metaData.set(key, value);
}

void TransferJob::addMetaData(const HashMap<String, String>& keysAndValues)
{
    HashMap<String, String>::const_iterator end = keysAndValues.end();
    for (HashMap<String, String>::const_iterator it = keysAndValues.begin(); it != end; ++it)
        d->metaData.set(it->first, it->second);
}

void TransferJob::kill()
{
    delete this;
}

KURL TransferJob::url() const
{
    return d->URL;
}

FormData TransferJob::postData() const
{
    return d->postData;
}

String TransferJob::method() const
{
    return d->method;
}

TransferJobClient* TransferJob::client() const
{
    return d->client;
}

} // namespace WebCore

/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <kwqdebug.h>

#include <qstring.h>
#include <jobclasses.h>

#include <Foundation/Foundation.h>

#include <WebFoundation/IFURLHandleClient.h>

namespace KIO {

class TransferJobPrivate
{
public:
    TransferJobPrivate(const KURL &kurl)
    {
        status = 0;
        metaData = [[NSMutableDictionary alloc] initWithCapacity:17];
        url = [kurl.getNSURL() copy];
        handle = nil;
    }

    ~TransferJobPrivate()
    {
        [metaData release];
        [url release];
        [handle release];
    }

    int status;
    NSMutableDictionary *metaData;
    NSURL *url;
    IFURLHandle *handle;
};

TransferJob::TransferJob(const KURL &url, bool reload, bool showProgressInfo)
{
    d = new TransferJobPrivate(url);
}

TransferJob::~TransferJob()
{
    delete d;
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

const QString &TransferJob::errorText() const
{
    _logNotYetImplemented();
    static QString text("DEFAULT_ERROR_TEXT");
    return text;
}

QString TransferJob::queryMetaData(const QString &key) const
{
    NSString *value;
    
    value = [d->metaData objectForKey:key.getNSString()]; 
    return value ? QString::fromNSString(value) : QString::null;
}
 
void TransferJob::addMetaData(const QString &key, const QString &value)
{
    NSString *immutableValue = [value.getNSString() copy];
    [d->metaData setObject:immutableValue forKey:key.getNSString()];
    [immutableValue release];
}

void TransferJob::kill()
{
    [d->handle cancelLoadInBackground];
}

void TransferJob::begin(id <IFURLHandleClient> client, void *userData)
{
    NSDictionary *attributes;
    
    attributes = [NSDictionary dictionaryWithObject:[NSValue valueWithPointer:userData] forKey:IFURLHandleUserData];
    d->handle = [[IFURLHandle alloc] initWithURL:d->url attributes:attributes flags:0];    
    [d->handle addClient:client];
    [d->handle loadInBackground];
}

IFURLHandle *TransferJob::handle() const
{
    return d->handle;
}

NSURL *TransferJob::url() const
{
    return [[d->url retain] autorelease];
}

} // namespace KIO

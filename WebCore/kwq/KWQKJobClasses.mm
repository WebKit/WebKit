/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQLogging.h"

#import "KWQResourceLoader.h"
#import "KWQString.h"
#import "KWQKJobClasses.h"

namespace KIO {

class TransferJobPrivate
{
public:
    TransferJobPrivate(const KURL &kurl)
        : status(0)
        , metaData([[NSMutableDictionary alloc] initWithCapacity:17])
        , URL(kurl)
        , loader(nil)
    {
    }

    ~TransferJobPrivate()
    {
        [metaData release];
        [loader release];
    }

    int status;
    NSMutableDictionary *metaData;
    KURL URL;
    KWQResourceLoader *loader;
};

TransferJob::TransferJob(const KURL &url, bool reload, bool showProgressInfo)
{
    d = new TransferJobPrivate(url);
}

TransferJob::TransferJob(const KURL &url, const QByteArray &postData, bool showProgressInfo)
{
    d = new TransferJobPrivate(url);
}

TransferJob::~TransferJob()
{
    [d->loader jobWillBeDeallocated];
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

QString TransferJob::errorText() const
{
    LOG(NotYetImplemented, "not yet implemented");
    return QString::null;
}

QString TransferJob::queryMetaData(const QString &key) const
{
    NSString *value = [d->metaData objectForKey:key.getNSString()]; 
    return value ? QString::fromNSString(value) : QString::null;
}

void TransferJob::addMetaData(const QString &key, const QString &value)
{
    [d->metaData setObject:value.getNSString() forKey:key.getNSString()];
}

void TransferJob::addMetaData(const QMap<QString, QString> &keysAndValues)
{
    QMapConstIterator<QString, QString> it = keysAndValues.begin();
    QMapConstIterator<QString, QString> end = keysAndValues.end();
    while (it != end) {
        [d->metaData setObject:it.data().getNSString() forKey:it.key().getNSString()];
        ++it;
    }
}

void TransferJob::kill()
{
    delete this;
}

void TransferJob::setLoader(KWQResourceLoader *loader)
{
    [loader retain];
    [d->loader release];
    d->loader = loader;
}

KURL TransferJob::url() const
{
    return d->URL;
}

} // namespace KIO

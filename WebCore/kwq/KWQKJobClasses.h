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

#ifndef JOBCLASSES_H_
#define JOBCLASSES_H_

#include "KWQMap.h"
#include "KWQObject.h"
#include "KWQString.h"

#include "KWQKURL.h"

#ifdef __OBJC__
@class KWQResourceLoader;
#else
class KWQResourceLoader;
#endif

namespace KIO {

class TransferJobPrivate;

class Job : public QObject {
public:
    virtual int error() const = 0;
    virtual QString errorText() const = 0;
    virtual void kill() = 0;
};

class TransferJob : public Job {
public:
    TransferJob(const KURL &, bool reload = false, bool showProgressInfo = true);
    TransferJob(const KURL &, const QByteArray &postData, bool showProgressInfo = true);
    ~TransferJob();

    int error() const;
    void setError(int);
    QString errorText() const;
    bool isErrorPage() const;
    QString queryMetaData(const QString &key) const;
    void addMetaData(const QString &key, const QString &value);
    void addMetaData(const QMap<QString, QString> &value);
    void kill();

    void setLoader(KWQResourceLoader *);
    
    KURL url() const;

private:
    TransferJobPrivate *d;
};

} // namespace KIO

#endif

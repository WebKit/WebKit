/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include <kxmlcore/HashMap.h>
#include "KWQObject.h"
#include "KWQString.h"
#include "KWQKURL.h"
#include "PlatformString.h"

#ifdef __OBJC__
@class KWQResourceLoader;
@class NSData;
@class NSURLResponse;
#else
class KWQResourceLoader;
class NSData;
class NSURLResponse;
#endif

namespace WebCore {
    class FormData;
}

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
    TransferJob(const KURL &, bool reload, bool deliverAllData=false);
    TransferJob(const KURL &, const WebCore::FormData& postData, bool deliverAllData=false);
    ~TransferJob();

    int error() const;
    void setError(int);
    QString errorText() const;
    bool isErrorPage() const;
    QString queryMetaData(const QString &key) const;
    void addMetaData(const QString &key, const QString &value);
    void addMetaData(const HashMap<WebCore::DOMString, WebCore::DOMString> &value);
    void kill();

    void setLoader(KWQResourceLoader *);
    void cancel();
    
    KURL url() const;

    void emitData(const char *, int);
    void emitRedirection(const KURL &);
    void emitResult(NSData *allData=0);
    void emitReceivedResponse(NSURLResponse *);

    WebCore::FormData postData() const;
    QString method() const;

private:
    void assembleResponseHeaders() const;
    void retrieveCharset() const;

    TransferJobPrivate *d;

    bool m_deliverAllData;
    
    KWQSignal m_data;
    KWQSignal m_redirection;
    KWQSignal m_result;
    KWQSignal m_receivedResponse;    
};

} // namespace KIO

#endif

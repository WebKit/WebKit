/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBTransaction_h
#define WebIDBTransaction_h

#include "WebExceptionCode.h"
#include <public/WebString.h>

namespace WebCore { class IDBTransactionBackendInterface; }

namespace WebKit {

class WebIDBObjectStore;
class WebIDBTransactionCallbacks;

// See comment in WebIDBFactory for a high level overview of these classes.
class WebIDBTransaction {
public:
    virtual ~WebIDBTransaction() { }

    enum TaskType {
        NormalTask = 0,
        PreemptiveTask
    };

    virtual WebIDBObjectStore* objectStore(long long, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void commit() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void abort() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void didCompleteTaskEvents() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void setCallbacks(WebIDBTransactionCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // FIXME: this is never called from WebCore. Find a cleaner solution.
    virtual WebCore::IDBTransactionBackendInterface* getIDBTransactionBackendInterface() const
    {
        return 0;
    }

    virtual void addPendingEvents(int) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBTransaction() {}
};

} // namespace WebKit

#endif // WebIDBTransaction_h

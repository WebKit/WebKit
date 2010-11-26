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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef IDBFactoryBackendProxy_h
#define IDBFactoryBackendProxy_h

#include "IDBFactoryBackendInterface.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebKit { class WebIDBFactory; }

namespace WebCore {

class DOMStringList;

class IDBFactoryBackendProxy : public IDBFactoryBackendInterface {
public:
    static PassRefPtr<IDBFactoryBackendInterface> create();
    virtual ~IDBFactoryBackendProxy();

    PassRefPtr<DOMStringList> databases(void) const;
    virtual void open(const String& name, PassRefPtr<IDBCallbacks>, PassRefPtr<SecurityOrigin>, Frame*, const String& dataDir, int64_t maximumSize);

private:
    IDBFactoryBackendProxy();

    // We don't own this pointer (unlike all the other proxy classes which do).
    WebKit::WebIDBFactory* m_webIDBFactory;
};

} // namespace WebCore

#endif

#endif // IDBFactoryBackendProxy_h


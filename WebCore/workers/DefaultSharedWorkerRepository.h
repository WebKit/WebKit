/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DefaultSharedWorkerRepository_h
#define DefaultSharedWorkerRepository_h

#if ENABLE(SHARED_WORKERS)

#include "SharedWorkerRepository.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

    class KURL;
    class ScriptExecutionContext;
    class SecurityOrigin;
    class SharedWorkerProxy;

    struct SecurityOriginHash;
    struct SecurityOriginTraits;

    // Platform-specific implementation of the SharedWorkerRepository static interface.
    class DefaultSharedWorkerRepository : public Noncopyable {
    public:
        // Invoked once the worker script has been loaded to fire up the worker thread.
        void workerScriptLoaded(SharedWorkerProxy&, const String& userAgent, const String& workerScript, PassOwnPtr<MessagePortChannel>);

        // Internal implementation of SharedWorkerRepository::connect()
        void connectToWorker(PassRefPtr<SharedWorker>, PassOwnPtr<MessagePortChannel>, const KURL&, const String& name, ExceptionCode&);

        static DefaultSharedWorkerRepository& instance();
    private:
        DefaultSharedWorkerRepository();
        ~DefaultSharedWorkerRepository();

        PassRefPtr<SharedWorkerProxy> getProxy(const String& name, const KURL&);
        // Mutex used to protect internal data structures.
        Mutex m_lock;

        typedef HashMap<String, RefPtr<SharedWorkerProxy> > SharedWorkerNameMap;
        typedef HashMap<RefPtr<SecurityOrigin>, SharedWorkerNameMap*, SecurityOriginHash> SharedWorkerProxyCache;

        // Items in this cache may be freed on another thread, so all keys and values must be either copied before insertion or thread safe.
        SharedWorkerProxyCache m_cache;
    };

} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)

#endif // DefaultSharedWorkerRepository_h

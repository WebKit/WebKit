/*
 * Copyright (C) 2012 Motorola Mobility Inc.
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

#ifndef PublicURLManager_h
#define PublicURLManager_h

#if ENABLE(BLOB)
#include "ActiveDOMObject.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class URL;
class ScriptExecutionContext;
class SecurityOrigin;
class URLRegistry;
class URLRegistrable;

class PublicURLManager : public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PublicURLManager(ScriptExecutionContext*);

    static std::unique_ptr<PublicURLManager> create(ScriptExecutionContext*);

    void registerURL(SecurityOrigin*, const URL&, URLRegistrable*);
    void revoke(const URL&);

    // ActiveDOMObject interface.
    virtual void stop() override;
private:
    
    typedef HashSet<String> URLSet;
    typedef HashMap<URLRegistry*, URLSet > RegistryURLMap;
    RegistryURLMap m_registryToURL;
    bool m_isStopped;
};

} // namespace WebCore

#endif // BLOB
#endif // PUBLICURLMANAGER_h

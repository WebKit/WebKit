/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebKeyValueStorageManager_h
#define WebKeyValueStorageManager_h

#include "WebProcessSupplement.h"
#include <WebCore/StorageTrackerClient.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebProcess;
struct SecurityOriginData;

class WebKeyValueStorageManager : public WebCore::StorageTrackerClient, public WebProcessSupplement {
    WTF_MAKE_NONCOPYABLE(WebKeyValueStorageManager);
public:
    explicit WebKeyValueStorageManager(WebProcess*);

    static const AtomicString& supplementName();

    const String& localStorageDirectory() const { return m_localStorageDirectory; }

private:
    // WebProcessSupplement
    virtual void initialize(const WebProcessCreationParameters&) OVERRIDE;

    // CoreIPC::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    void didReceiveWebKeyValueStorageManagerMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);

    void getKeyValueStorageOrigins(uint64_t callbackID);
    void deleteEntriesForOrigin(const SecurityOriginData&);
    void deleteAllEntries();

    void dispatchDidGetKeyValueStorageOrigins(const Vector<SecurityOriginData>& identifiers, uint64_t callbackID);

    // WebCore::StorageTrackerClient
    virtual void dispatchDidModifyOrigin(const String&) OVERRIDE;
    virtual void didFinishLoadingOrigins() OVERRIDE;

    Vector<uint64_t> m_originsRequestCallbackIDs;
    String m_localStorageDirectory;
    WebProcess* m_process;
};

} // namespace WebKit

#endif // WebKeyValueStorageManager_h

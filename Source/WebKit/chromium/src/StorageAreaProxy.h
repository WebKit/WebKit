/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StorageAreaProxy_h
#define StorageAreaProxy_h

#include "StorageArea.h"

namespace WebKit {
class WebStorageArea;
class WebStorageNamespace;
}

namespace WebCore {

class Frame;
class KURL;
class Page;
class SecurityOrigin;
class Storage;

class StorageAreaProxy : public StorageArea {
public:
    StorageAreaProxy(WebKit::WebStorageArea*, StorageType);
    virtual ~StorageAreaProxy();

    // The HTML5 DOM Storage API
    virtual unsigned length(Frame* sourceFrame) const;
    virtual String key(unsigned index, Frame* sourceFrame) const;
    virtual String getItem(const String& key, Frame* sourceFrame) const;
    virtual String setItem(const String& key, const String& value, ExceptionCode& ec, Frame* sourceFrame);
    virtual String removeItem(const String& key, Frame* sourceFrame);
    virtual bool clear(Frame* sourceFrame);
    virtual bool contains(const String& key, Frame* sourceFrame) const;

    virtual bool disabledByPrivateBrowsingInFrame(const Frame*) const { return false; }

    static void dispatchLocalStorageEvent(
            const String& pageGroupName, const String& key, const String& oldValue, const String& newValue,
            SecurityOrigin*, const KURL& pageURL, WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess);
    static void dispatchSessionStorageEvent(
            const String& pageGroupName, const String& key, const String& oldValue, const String& newValue,
            SecurityOrigin*, const KURL& pageURL, const WebKit::WebStorageNamespace&,
            WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess);

private:
    void storageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Frame* sourceFrame);
    bool canAccessStorage(Frame*) const;

    static bool isEventSource(Storage*, WebKit::WebStorageArea* sourceAreaInstance);

    OwnPtr<WebKit::WebStorageArea> m_storageArea;
    StorageType m_storageType;
};

} // namespace WebCore

#endif // StorageAreaProxy_h

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

#if ENABLE(DOM_STORAGE)

#include "StorageArea.h"

namespace WebKit { class WebStorageArea; }

namespace WebCore {

class Frame;
class SecurityOrigin;

class StorageAreaProxy : public StorageArea {
public:
    StorageAreaProxy(WebKit::WebStorageArea*, StorageType);
    virtual ~StorageAreaProxy();

    // The HTML5 DOM Storage API
    virtual unsigned length() const;
    virtual String key(unsigned index) const;
    virtual String getItem(const String& key) const;
    virtual String setItem(const String& key, const String& value, ExceptionCode& ec, Frame* sourceFrame);
    virtual String removeItem(const String& key, Frame* sourceFrame);
    virtual bool clear(Frame* sourceFrame);
    virtual bool contains(const String& key) const;

private:
    void storageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Frame* sourceFrame);

    OwnPtr<WebKit::WebStorageArea> m_storageArea;
    StorageType m_storageType;
};

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)

#endif // StorageAreaProxy_h

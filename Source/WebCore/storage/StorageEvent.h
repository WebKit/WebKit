/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Event.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class Storage;

class StorageEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(StorageEvent);
public:
    static Ref<StorageEvent> create(const AtomString& type, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea);
    static Ref<StorageEvent> createForBindings();

    struct Init : EventInit {
        String key;
        String oldValue;
        String newValue;
        String url;
        RefPtr<Storage> storageArea;
    };

    static Ref<StorageEvent> create(const AtomString&, const Init&, IsTrusted = IsTrusted::No);
    virtual ~StorageEvent();

    const String& key() const { return m_key; }
    const String& oldValue() const { return m_oldValue; }
    const String& newValue() const { return m_newValue; }
    const String& url() const { return m_url; }
    Storage* storageArea() const { return m_storageArea.get(); }

    void initStorageEvent(const AtomString& type, bool canBubble, bool cancelable, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea);

    // Needed once we support init<blank>EventNS
    // void initStorageEventNS(in DOMString namespaceURI, in DOMString typeArg, in boolean canBubbleArg, in boolean cancelableArg, in DOMString keyArg, in DOMString oldValueArg, in DOMString newValueArg, in DOMString urlArg, Storage storageAreaArg);

    EventInterface eventInterface() const override;

private:
    StorageEvent();
    StorageEvent(const AtomString& type, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea);
    StorageEvent(const AtomString&, const Init&, IsTrusted);

    String m_key;
    String m_oldValue;
    String m_newValue;
    String m_url;
    RefPtr<Storage> m_storageArea;
};

} // namespace WebCore

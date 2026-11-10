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
    WTF_MAKE_TZONE_ALLOCATED(StorageEvent);
public:
    NODELETE static Ref<StorageEvent> create(AtomStringImpl* type, StringImpl* key, StringImpl* oldValue, StringImpl* newValue, StringImpl* url, Storage* storageArea);
    NODELETE static Ref<StorageEvent> createForBindings();

    struct Init : EventInit {
        RefPtr<StringImpl> key;
        RefPtr<StringImpl> oldValue;
        RefPtr<StringImpl> newValue;
        RefPtr<StringImpl> url;
        RefPtr<Storage> storageArea;
    };

    NODELETE static Ref<StorageEvent> create(AtomStringImpl*, Init&&, IsTrusted = IsTrusted::No);
    virtual ~StorageEvent();

    const RefPtr<StringImpl>& key() const LIFETIME_BOUND { return m_key; }
    const RefPtr<StringImpl>& oldValue() const LIFETIME_BOUND { return m_oldValue; }
    const RefPtr<StringImpl>& newValue() const LIFETIME_BOUND { return m_newValue; }
    const RefPtr<StringImpl>& url() const LIFETIME_BOUND { return m_url; }
    Storage* storageArea() const { return m_storageArea; }

    void initStorageEvent(AtomStringImpl* type, bool canBubble, bool cancelable, StringImpl* key, StringImpl* oldValue, StringImpl* newValue, StringImpl* url, Storage* storageArea);

private:
    StorageEvent();
    StorageEvent(AtomStringImpl* type, StringImpl* key, StringImpl* oldValue, StringImpl* newValue, StringImpl* url, Storage* storageArea);
    StorageEvent(AtomStringImpl*, Init&&, IsTrusted);

    RefPtr<StringImpl> m_key;
    RefPtr<StringImpl> m_oldValue;
    RefPtr<StringImpl> m_newValue;
    RefPtr<StringImpl> m_url;
    RefPtr<Storage> m_storageArea;
};

} // namespace WebCore

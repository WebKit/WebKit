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

#include "config.h"
#include "StorageEvent.h"

#include "Storage.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StorageEvent);

Ref<StorageEvent> StorageEvent::createForBindings()
{
    return adoptRef(*new StorageEvent);
}

StorageEvent::StorageEvent() = default;

StorageEvent::~StorageEvent() = default;

Ref<StorageEvent> StorageEvent::create(const AtomString& type, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea)
{
    return adoptRef(*new StorageEvent(type, key, oldValue, newValue, url, storageArea));
}

Ref<StorageEvent> StorageEvent::create(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new StorageEvent(type, initializer, isTrusted));
}

StorageEvent::StorageEvent(const AtomString& type, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea)
    : Event(type, CanBubble::No, IsCancelable::No)
    , m_key(key)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
    , m_url(url)
    , m_storageArea(storageArea)
{
}

StorageEvent::StorageEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_key(initializer.key)
    , m_oldValue(initializer.oldValue)
    , m_newValue(initializer.newValue)
    , m_url(initializer.url)
    , m_storageArea(initializer.storageArea)
{
}

void StorageEvent::initStorageEvent(const AtomString& type, bool canBubble, bool cancelable, const String& key, const String& oldValue, const String& newValue, const String& url, Storage* storageArea)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_key = key;
    m_oldValue = oldValue;
    m_newValue = newValue;
    m_url = url;
    m_storageArea = storageArea;
}

EventInterface StorageEvent::eventInterface() const
{
    return StorageEventInterfaceType;
}

} // namespace WebCore

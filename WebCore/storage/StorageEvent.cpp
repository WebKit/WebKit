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

#if ENABLE(DOM_STORAGE)

#include "DOMWindow.h"
#include "Storage.h"

namespace WebCore {

StorageEvent::StorageEvent(const AtomicString& type, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source, Storage* storageArea)
    : Event(type, false, true)
    , m_key(key)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
    , m_uri(uri)
    , m_source(source)
    , m_storageArea(storageArea)
{
}

void StorageEvent::initStorageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source, Storage* storageArea)
{
    if (dispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_key = key;
    m_oldValue = oldValue;
    m_newValue = newValue;
    m_uri = uri;
    m_source = source;
    m_storageArea = storageArea;
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)


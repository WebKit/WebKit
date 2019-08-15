/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "Storage.h"

#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "StorageArea.h"
#include "StorageType.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Storage);

Ref<Storage> Storage::create(DOMWindow& window, Ref<StorageArea>&& storageArea)
{
    return adoptRef(*new Storage(window, WTFMove(storageArea)));
}

Storage::Storage(DOMWindow& window, Ref<StorageArea>&& storageArea)
    : DOMWindowProperty(&window)
    , m_storageArea(WTFMove(storageArea))
{
    ASSERT(frame());

    m_storageArea->incrementAccessCount();
}

Storage::~Storage()
{
    m_storageArea->decrementAccessCount();
}

unsigned Storage::length() const
{
    return m_storageArea->length();
}

String Storage::key(unsigned index) const
{
    return m_storageArea->key(index);
}

String Storage::getItem(const String& key) const
{
    return m_storageArea->item(key);
}

ExceptionOr<void> Storage::setItem(const String& key, const String& value)
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { InvalidAccessError };

    bool quotaException = false;
    m_storageArea->setItem(frame, key, value, quotaException);
    if (quotaException)
        return Exception { QuotaExceededError };
    return { };
}

ExceptionOr<void> Storage::removeItem(const String& key)
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { InvalidAccessError };

    m_storageArea->removeItem(frame, key);
    return { };
}

ExceptionOr<void> Storage::clear()
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { InvalidAccessError };

    m_storageArea->clear(frame);
    return { };
}

bool Storage::contains(const String& key) const
{
    return m_storageArea->contains(key);
}

bool Storage::isSupportedPropertyName(const String& propertyName) const
{
    return m_storageArea->contains(propertyName);
}

Vector<AtomString> Storage::supportedPropertyNames() const
{
    unsigned length = m_storageArea->length();

    Vector<AtomString> result;
    result.reserveInitialCapacity(length);

    for (unsigned i = 0; i < length; ++i)
        result.uncheckedAppend(m_storageArea->key(i));

    return result;
}

} // namespace WebCore

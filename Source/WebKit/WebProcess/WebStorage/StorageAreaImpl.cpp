/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StorageAreaImpl.h"

#include "StorageAreaMap.h"
#include <WebCore/Document.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

Ref<StorageAreaImpl> StorageAreaImpl::create(StorageAreaMap& storageAreaMap)
{
    return adoptRef(*new StorageAreaImpl(storageAreaMap));
}

StorageAreaImpl::StorageAreaImpl(StorageAreaMap& storageAreaMap)
    : m_storageAreaMap(storageAreaMap)
{
    storageAreaMap.incrementUseCount();
}

StorageAreaImpl::~StorageAreaImpl()
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        storageAreaMap->decrementUseCount();
}

unsigned StorageAreaImpl::length()
{
    RefPtr storageAreaMap = m_storageAreaMap.get();
    return storageAreaMap ? storageAreaMap->length() : 0;
}

String StorageAreaImpl::key(unsigned index)
{
    RefPtr storageAreaMap = m_storageAreaMap.get();
    return storageAreaMap ? storageAreaMap->key(index) : nullString();
}

String StorageAreaImpl::item(const String& key)
{
    RefPtr storageAreaMap = m_storageAreaMap.get();
    return storageAreaMap ? storageAreaMap->item(key) : nullString();
}

void StorageAreaImpl::setItem(LocalFrame& sourceFrame, const String& key, const String& value, bool& quotaException)
{
    ASSERT(!value.isNull());

    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        storageAreaMap->setItem(sourceFrame, this, key, value, quotaException);
}

void StorageAreaImpl::removeItem(LocalFrame& sourceFrame, const String& key)
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        storageAreaMap->removeItem(sourceFrame, this, key);
}

void StorageAreaImpl::clear(LocalFrame& sourceFrame)
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        storageAreaMap->clear(sourceFrame, this);
}

bool StorageAreaImpl::contains(const String& key)
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        return storageAreaMap->contains(key);

    return false;
}

StorageType StorageAreaImpl::storageType() const
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        return storageAreaMap->type();

    // We probably need an Invalid type.
    return StorageType::Local;
}

size_t StorageAreaImpl::memoryBytesUsedByCache()
{
    return 0;
}

void StorageAreaImpl::prewarm()
{
    if (RefPtr storageAreaMap = m_storageAreaMap.get())
        storageAreaMap->connect();
}

} // namespace WebKit

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
#include "StorageAreaProxy.h"

#include "SecurityOriginData.h"
#include "StorageManagerMessages.h"
#include "StorageNamespaceProxy.h"
#include "WebProcess.h"
#include <WebCore/ExceptionCode.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/SecurityOrigin.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateStorageAreaID()
{
    static uint64_t storageAreaID;
    return ++storageAreaID;
}

PassRefPtr<StorageAreaProxy> StorageAreaProxy::create(StorageNamespaceProxy* storageNamespaceProxy, PassRefPtr<SecurityOrigin> securityOrigin)
{
    return adoptRef(new StorageAreaProxy(storageNamespaceProxy, securityOrigin));
}

StorageAreaProxy::StorageAreaProxy(StorageNamespaceProxy* storageNamespaceProxy, PassRefPtr<SecurityOrigin> securityOrigin)
    : m_storageType(storageNamespaceProxy->storageType())
    , m_quotaInBytes(storageNamespaceProxy->quotaInBytes())
    , m_storageAreaID(generateStorageAreaID())
{
    WebProcess::shared().connection()->send(Messages::StorageManager::CreateStorageArea(m_storageAreaID, storageNamespaceProxy->storageNamespaceID(), SecurityOriginData::fromSecurityOrigin(securityOrigin.get())), 0);
}

StorageAreaProxy::~StorageAreaProxy()
{
    WebProcess::shared().connection()->send(Messages::StorageManager::DestroyStorageArea(m_storageAreaID), 0);
}

unsigned StorageAreaProxy::length(ExceptionCode& ec, Frame* sourceFrame)
{
    ec = 0;
    if (!canAccessStorage(sourceFrame)) {
        ec = SECURITY_ERR;
        return 0;
    }

    if (disabledByPrivateBrowsingInFrame(sourceFrame))
        return 0;

    loadValuesIfNeeded();
    return m_values->size();
}

String StorageAreaProxy::key(unsigned index, ExceptionCode&, Frame* sourceFrame)
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
    return String();
}

String StorageAreaProxy::getItem(const String& key, ExceptionCode&, Frame* sourceFrame)
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
    return String();
}

void StorageAreaProxy::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* sourceFrame)
{
    ec = 0;
    if (!canAccessStorage(sourceFrame)) {
        ec = SECURITY_ERR;
        return;
    }

    ASSERT(!value.isNull());

    if (disabledByPrivateBrowsingInFrame(sourceFrame)) {
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }

    loadValuesIfNeeded();

    // FIXME: Actually set the value.
    ASSERT_NOT_REACHED();
}

void StorageAreaProxy::removeItem(const String& key, ExceptionCode&, Frame* sourceFrame)
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

void StorageAreaProxy::clear(ExceptionCode&, Frame* sourceFrame)
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

bool StorageAreaProxy::contains(const String& key, ExceptionCode&, Frame* sourceFrame)
{
    // FIXME: Implement this.
    return false;
}

bool StorageAreaProxy::canAccessStorage(Frame* frame)
{
    return frame && frame->page();
}

size_t StorageAreaProxy::memoryBytesUsedByCache()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
    return 0;
}

void StorageAreaProxy::incrementAccessCount()
{
    // Storage access is handled in the UI process, so there's nothing to do here.
}

void StorageAreaProxy::decrementAccessCount()
{
    // Storage access is handled in the UI process, so there's nothing to do here.
}

void StorageAreaProxy::closeDatabaseIfIdle()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

bool StorageAreaProxy::disabledByPrivateBrowsingInFrame(const Frame* sourceFrame) const
{
    if (!sourceFrame->page()->settings()->privateBrowsingEnabled())
        return false;

    if (m_storageType != LocalStorage)
        return true;

    return !SchemeRegistry::allowsLocalStorageAccessInPrivateBrowsing(sourceFrame->document()->securityOrigin()->protocol());
}

void StorageAreaProxy::loadValuesIfNeeded()
{
    if (m_values)
        return;

    HashMap<String, String> values;
    // FIXME: This should use a special sendSync flag to indicate that we don't want to process incoming messages while waiting for a reply.
    // (This flag does not yet exist).
    WebProcess::shared().connection()->sendSync(Messages::StorageManager::GetValues(m_storageAreaID), Messages::StorageManager::GetValues::Reply(values), 0);

    // FIXME: Don't copy the hash map.
    m_values = adoptPtr(new HashMap<String, String>(values));
}

} // namespace WebKit

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
#include "StorageManager.h"

#include "StorageManagerMessages.h"
#include "WebProcessProxy.h"
#include "WorkQueue.h"

namespace WebKit {

PassRefPtr<StorageManager> StorageManager::create()
{
    return adoptRef(new StorageManager);
}

StorageManager::StorageManager()
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageManager"))
{
}

StorageManager::~StorageManager()
{
}

void StorageManager::processWillOpenConnection(WebProcessProxy* webProcessProxy)
{
    webProcessProxy->connection()->addQueueClient(this);
}

void StorageManager::processWillCloseConnection(WebProcessProxy* webProcessProxy)
{
    webProcessProxy->connection()->removeQueueClient(this);
}

void StorageManager::didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, OwnPtr<CoreIPC::MessageDecoder>& decoder)
{
    if (decoder->messageReceiverName() == Messages::StorageManager::messageReceiverName()) {
        didReceiveStorageManagerMessageOnConnectionWorkQueue(connection, decoder);
        return;
    }
}

void StorageManager::didCloseOnConnectionWorkQueue(CoreIPC::Connection*)
{
}

void StorageManager::createStorageArea(CoreIPC::Connection*, uint64_t storageAreaID, uint64_t storageNamespaceID, const SecurityOriginData&)
{
    UNUSED_PARAM(storageAreaID);
    UNUSED_PARAM(storageNamespaceID);
}

void StorageManager::destroyStorageArea(CoreIPC::Connection*, uint64_t)
{
}

} // namespace WebKit

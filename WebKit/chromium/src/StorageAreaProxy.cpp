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

#include "config.h"
#include "StorageAreaProxy.h"

#if ENABLE(DOM_STORAGE)

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "SecurityOrigin.h"
#include "StorageAreaImpl.h"

#include "WebStorageArea.h"
#include "WebString.h"
#include "WebURL.h"

namespace WebCore {

StorageAreaProxy::StorageAreaProxy(WebKit::WebStorageArea* storageArea)
    : m_storageArea(storageArea)
{
}

StorageAreaProxy::~StorageAreaProxy()
{
}

unsigned StorageAreaProxy::length() const
{
    return m_storageArea->length();
}

String StorageAreaProxy::key(unsigned index) const
{
    return m_storageArea->key(index);
}

String StorageAreaProxy::getItem(const String& key) const
{
    return m_storageArea->getItem(key);
}

void StorageAreaProxy::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    bool quotaException = false;
    m_storageArea->setItem(key, value, frame->document()->url(), quotaException);
    ec = quotaException ? QUOTA_EXCEEDED_ERR : 0;
}

void StorageAreaProxy::removeItem(const String& key, Frame* frame)
{
    m_storageArea->removeItem(key, frame->document()->url());
}

void StorageAreaProxy::clear(Frame* frame)
{
    m_storageArea->clear(frame->document()->url());
}

bool StorageAreaProxy::contains(const String& key) const
{
    return !getItem(key).isNull();
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)

/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebStorageNamespaceImpl.h"

#if ENABLE(DOM_STORAGE)

#include "SecurityOrigin.h"

#include "WebStorageAreaImpl.h"
#include "WebString.h"

namespace WebKit {

WebStorageNamespace* WebStorageNamespace::createLocalStorageNamespace(const WebString& path, unsigned quota)
{
    return new WebStorageNamespaceImpl(WebCore::StorageNamespaceImpl::localStorageNamespace(path, quota));
}

WebStorageNamespace* WebStorageNamespace::createSessionStorageNamespace(unsigned quota)
{
    return new WebStorageNamespaceImpl(WebCore::StorageNamespaceImpl::sessionStorageNamespace(quota));
}

WebStorageNamespaceImpl::WebStorageNamespaceImpl(PassRefPtr<WebCore::StorageNamespace> storageNamespace)
    : m_storageNamespace(storageNamespace)
{
}

WebStorageNamespaceImpl::~WebStorageNamespaceImpl()
{
}

WebStorageArea* WebStorageNamespaceImpl::createStorageArea(const WebString& originString)
{
    WTF::String originWebCoreString = originString;
    if (originWebCoreString == "file://") {
        // FIXME: We should really be passing around WebSecurityOrigin objects
        //        to represent security origins instead of strings.  One issue
        //        with using strings is that createFromString(toString) does
        //        not round-trip for file URLs because file:// looks like a
        //        directory (which is sandboxed).
        //
        // For the time being, we work around this issue by using "file:///a",
        // which does not look like a directory.  We should fix this when
        // jorlow gets back from vactation.
        originWebCoreString = "file:///a";
    }
    RefPtr<WebCore::SecurityOrigin> origin = WebCore::SecurityOrigin::createFromString(originWebCoreString);
    return new WebStorageAreaImpl(m_storageNamespace->storageArea(origin.release()));
}

WebStorageNamespace* WebStorageNamespaceImpl::copy()
{
    return new WebStorageNamespaceImpl(m_storageNamespace->copy());
}

void WebStorageNamespaceImpl::close()
{
    m_storageNamespace->close();
}

} // namespace WebKit

#endif // ENABLE(DOM_STORAGE)

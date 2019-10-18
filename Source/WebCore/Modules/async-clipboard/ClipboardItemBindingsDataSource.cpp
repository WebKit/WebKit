/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ClipboardItemBindingsDataSource.h"

#include "Blob.h"
#include "JSBlob.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "SharedBuffer.h"
#include <wtf/Function.h>

namespace WebCore {

ClipboardItemBindingsDataSource::ClipboardItemBindingsDataSource(ClipboardItem& item, Vector<KeyValuePair<String, RefPtr<DOMPromise>>>&& itemPromises)
    : ClipboardItemDataSource(item)
    , m_itemPromises(WTFMove(itemPromises))
{
}

ClipboardItemBindingsDataSource::~ClipboardItemBindingsDataSource() = default;

Vector<String> ClipboardItemBindingsDataSource::types() const
{
    return m_itemPromises.map([&] (auto& typeAndItem) {
        return typeAndItem.key;
    });
}

void ClipboardItemBindingsDataSource::getType(const String& type, Ref<DeferredPromise>&& promise)
{
    auto matchIndex = m_itemPromises.findMatching([&] (auto& item) {
        return type == item.key;
    });

    if (matchIndex == notFound) {
        promise->reject(NotFoundError);
        return;
    }

    auto itemPromise = m_itemPromises[matchIndex].value;
    itemPromise->whenSettled([itemPromise, promise = makeRefPtr(promise.get()), type] () mutable {
        if (itemPromise->status() != DOMPromise::Status::Fulfilled) {
            promise->reject(AbortError);
            return;
        }

        auto result = itemPromise->result();
        if (!result) {
            promise->reject(TypeError);
            return;
        }

        String string;
        result.getString(itemPromise->globalObject()->globalExec(), string);
        if (!string.isNull()) {
            promise->resolve<IDLInterface<Blob>>(ClipboardItem::blobFromString(string, type));
            return;
        }

        if (!result.isObject()) {
            promise->reject(TypeError);
            return;
        }

        if (auto blob = JSBlob::toWrapped(result.getObject()->vm(), result.getObject()))
            promise->resolve<IDLInterface<Blob>>(*blob);
        else
            promise->reject(TypeError);
    });
}

} // namespace WebCore

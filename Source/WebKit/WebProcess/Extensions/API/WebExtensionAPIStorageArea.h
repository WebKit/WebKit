/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)s
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIStorageArea.h"
#include "JSWebExtensionWrappable.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionDataType.h"

namespace WebKit {

class WebExtensionAPIStorageArea : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPIStorageArea, storageArea, storageArea);

public:
#if PLATFORM(COCOA)
    bool isPropertyAllowed(const ASCIILiteral& propertyName, WebPage*);

    void get(WebPage&, id items, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getKeys(WebPage&, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getBytesInUse(WebPage&, id keys, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void set(WebPage&, NSDictionary *items, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void remove(WebPage&, id keys, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void clear(WebPage&, Ref<WebExtensionCallbackHandler>&&);

    double quotaBytes();

    // Exposed only by storage.session.
    void setAccessLevel(WebPage&, NSDictionary *, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    // Exposed only by storage.sync.
    double quotaBytesPerItem();
    double maxItems();
    double maxWriteOperationsPerHour();
    double maxWriteOperationsPerMinute();

    WebExtensionAPIEvent& onChanged();

private:
    explicit WebExtensionAPIStorageArea(const WebExtensionAPIObject& parentObject, WebExtensionDataType type)
        : WebExtensionAPIObject(parentObject)
        , m_type(type)
    {
        setPropertyPath(toAPIString(type), &parentObject);
    }

    WebExtensionDataType m_type { WebExtensionDataType::Local };
    RefPtr<WebExtensionAPIEvent> m_onChanged;
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

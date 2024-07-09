/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrappable.h"
#include "JSWebExtensionWrapper.h"
#include "WebExtensionContentWorldType.h"
#include "WebExtensionContextProxy.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

class WebExtensionAPIRuntime;
class WebExtensionAPIWebPageRuntime;

class WebExtensionAPIObject {
public:
    WebExtensionAPIObject(WebExtensionContentWorldType contentWorldType)
        : m_contentWorldType(contentWorldType)
    {
        // This should only be called when creating a namespace object for web pages.
        ASSERT(contentWorldType == WebExtensionContentWorldType::WebPage);
    }

    WebExtensionAPIObject(WebExtensionContentWorldType contentWorldType, WebExtensionContextProxy& context)
        : m_contentWorldType(contentWorldType)
        , m_extensionContext(&context)
    {
    }

    WebExtensionAPIObject(WebExtensionContentWorldType contentWorldType, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context)
        : m_contentWorldType(contentWorldType)
        , m_runtime(&runtime)
        , m_extensionContext(&context)
    {
    }

    WebExtensionAPIObject(const WebExtensionAPIObject& parentObject)
        : m_contentWorldType(parentObject.contentWorldType())
        , m_runtime(&parentObject.runtime())
        , m_extensionContext(parentObject.m_extensionContext) // Using parentObject.extensionContext() is not safe for APIWebPage objects.
    {
    }

    virtual ~WebExtensionAPIObject() = default;

    bool isForMainWorld() const { return m_contentWorldType == WebExtensionContentWorldType::Main; }
    WebExtensionContentWorldType contentWorldType() const { return m_contentWorldType; }

    virtual WebExtensionAPIRuntimeBase& runtime() const { return *m_runtime; }

    WebExtensionContextProxy& extensionContext() const { return *m_extensionContext; }
    bool hasExtensionContext() const { return !!m_extensionContext; }

    const String& propertyPath() const { return m_propertyPath; }
    void setPropertyPath(const String& propertyName, const WebExtensionAPIObject* parentObject = nullptr)
    {
        ASSERT(!propertyName.isEmpty());

        if (parentObject && !parentObject->propertyPath().isEmpty())
            m_propertyPath = makeString(parentObject->propertyPath(), '.', propertyName);
        else
            m_propertyPath = propertyName;
    }

private:
    WebExtensionContentWorldType m_contentWorldType { WebExtensionContentWorldType::Main };
    RefPtr<WebExtensionAPIRuntimeBase> m_runtime;
    RefPtr<WebExtensionContextProxy> m_extensionContext;
    String m_propertyPath;
};

} // namespace WebKit

#define WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(ImplClass, ScriptClass, PropertyName) \
public: \
    template<typename... Args> \
    static Ref<ImplClass> create(Args&&... args) \
    { \
        return adoptRef(*new ImplClass(std::forward<Args>(args)...)); \
    } \
\
private: \
    explicit ImplClass(WebExtensionContentWorldType contentWorldType) \
        : WebExtensionAPIObject(contentWorldType) \
    { \
        setPropertyPath(#PropertyName##_s); \
    } \
\
    explicit ImplClass(WebExtensionContentWorldType contentWorldType, WebExtensionContextProxy& context) \
        : WebExtensionAPIObject(contentWorldType, context) \
    { \
        setPropertyPath(#PropertyName##_s); \
    } \
\
    explicit ImplClass(WebExtensionContentWorldType contentWorldType, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context) \
        : WebExtensionAPIObject(contentWorldType, runtime, context) \
    { \
        setPropertyPath(#PropertyName##_s); \
    } \
\
    explicit ImplClass(const WebExtensionAPIObject& parentObject) \
        : WebExtensionAPIObject(parentObject) \
    { \
        setPropertyPath(#PropertyName##_s, &parentObject); \
    } \
\
    JSClassRef wrapperClass() final { return JS##ImplClass::ScriptClass##Class(); } \
\
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

// End of macro.

// Needs to be at the end to allow WebExtensionAPIRuntime.h to include this header
// and avoid having all APIs need to include it.
#include "WebExtensionAPIRuntime.h"

#endif // ENABLE(WK_WEB_EXTENSIONS)

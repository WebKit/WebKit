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
#include "WebExtensionContextProxy.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebKit {

class WebExtensionAPIRuntime;

class WebExtensionAPIObject {
public:
    enum class ForMainWorld : bool { No, Yes };

    WebExtensionAPIObject(ForMainWorld forMainWorld, WebExtensionContextProxy& context)
        : m_forMainWorld(forMainWorld)
        , m_extensionContext(&context)
    {
    }

    WebExtensionAPIObject(ForMainWorld forMainWorld, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context)
        : m_forMainWorld(forMainWorld)
        , m_runtime(&runtime)
        , m_extensionContext(&context)
    {
    }

    virtual ~WebExtensionAPIObject() = default;

    ForMainWorld forMainWorld() const { return m_forMainWorld; }
    bool isForMainWorld() const { return m_forMainWorld == ForMainWorld::Yes; }

    virtual WebExtensionAPIRuntimeBase& runtime() { return *m_runtime; }
    WebExtensionContextProxy& extensionContext() { return *m_extensionContext; }

private:
    ForMainWorld m_forMainWorld = ForMainWorld::Yes;
    RefPtr<WebExtensionAPIRuntimeBase> m_runtime;
    RefPtr<WebExtensionContextProxy> m_extensionContext;
};

} // namespace WebKit

#define WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(ImplClass, ScriptClass) \
public: \
    using ForMainWorld = WebExtensionAPIObject::ForMainWorld; \
\
    template<typename... Args> \
    static Ref<ImplClass> create(Args&&... args) \
    { \
        return adoptRef(*new ImplClass(std::forward<Args>(args)...)); \
    } \
\
    virtual ~ImplClass() = default; \
\
private: \
    explicit ImplClass(ForMainWorld forMainWorld, WebExtensionContextProxy& context) \
        : WebExtensionAPIObject(forMainWorld, context) \
    { \
    } \
\
    explicit ImplClass(ForMainWorld forMainWorld, WebExtensionAPIRuntimeBase& runtime, WebExtensionContextProxy& context) \
        : WebExtensionAPIObject(forMainWorld, runtime, context) \
    { \
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

/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/ContextDestructionObserver.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GPU;
class ScriptExecutionContext;
class ServiceWorkerContainer;
class StorageManager;
class WebCoreOpaqueRoot;
class WebLockManager;
template<typename> class ExceptionOr;

class NavigatorBase : public RefCounted<NavigatorBase>, public ContextDestructionObserver, public CanMakeCheckedPtr<NavigatorBase> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(NavigatorBase);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(NavigatorBase);
public:
    virtual ~NavigatorBase();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static String appName();
    String appVersion() const;
    virtual const String& userAgent() const = 0;
    virtual String platform() const;

    static String appCodeName();
    static String product();
    static String productSub();
    static String vendor();
    static String vendorSub();

    virtual bool onLine() const = 0;

    static String language();
    static Vector<String> languages();

    StorageManager& storage();
    WebLockManager& locks();

    int hardwareConcurrency(ScriptExecutionContext&);

protected:
    explicit NavigatorBase(ScriptExecutionContext*);

private:
    RefPtr<StorageManager> m_storageManager;
    RefPtr<WebLockManager> m_webLockManager;

public:
    ServiceWorkerContainer& serviceWorker();
    ExceptionOr<ServiceWorkerContainer&> serviceWorker(ScriptExecutionContext&);

private:
    std::unique_ptr<ServiceWorkerContainer> m_serviceWorkerContainer;
};

WebCoreOpaqueRoot root(NavigatorBase*);

} // namespace WebCore

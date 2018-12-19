/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "ExceptionOr.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class ScriptExecutionContext;
class ServiceWorkerContainer;

class NavigatorBase : public RefCounted<NavigatorBase> {
public:
    virtual ~NavigatorBase();

    static String appName();
    String appVersion() const;
    virtual const String& userAgent() const = 0;
    virtual const String& platform() const;

    static String appCodeName();
    static String product();
    static String productSub();
    static String vendor();
    static String vendorSub();

    virtual bool onLine() const = 0;

    static String language();
    static Vector<String> languages();

protected:
    explicit NavigatorBase(ScriptExecutionContext*);

#if ENABLE(SERVICE_WORKER)
public:
    ServiceWorkerContainer& serviceWorker();
    ExceptionOr<ServiceWorkerContainer&> serviceWorker(ScriptExecutionContext&);

private:
    UniqueRef<ServiceWorkerContainer> m_serviceWorkerContainer;
#endif
};

} // namespace WebCore

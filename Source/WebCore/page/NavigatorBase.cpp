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
 *
 */

#include "config.h"
#include "NavigatorBase.h"

#include "ServiceWorkerContainer.h"
#include <mutex>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
#include <wtf/text/WTFString.h>

#if OS(LINUX)
#include "sys/utsname.h"
#include <wtf/StdLibExtras.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "Device.h"
#endif

#ifndef WEBCORE_NAVIGATOR_PLATFORM
#if PLATFORM(IOS_FAMILY)
#define WEBCORE_NAVIGATOR_PLATFORM deviceName()
#elif OS(MAC_OS_X) && (CPU(PPC) || CPU(PPC64))
#define WEBCORE_NAVIGATOR_PLATFORM "MacPPC"_s
#elif OS(MAC_OS_X) && (CPU(X86) || CPU(X86_64))
#define WEBCORE_NAVIGATOR_PLATFORM "MacIntel"_s
#elif OS(WINDOWS)
#define WEBCORE_NAVIGATOR_PLATFORM "Win32"_s
#else
#define WEBCORE_NAVIGATOR_PLATFORM emptyString()
#endif
#endif // ifndef WEBCORE_NAVIGATOR_PLATFORM

#ifndef WEBCORE_NAVIGATOR_PRODUCT
#define WEBCORE_NAVIGATOR_PRODUCT "Gecko"_s
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT

#ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB
#define WEBCORE_NAVIGATOR_PRODUCT_SUB "20030107"_s
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB

#ifndef WEBCORE_NAVIGATOR_VENDOR
#define WEBCORE_NAVIGATOR_VENDOR "Apple Computer, Inc."_s
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR

#ifndef WEBCORE_NAVIGATOR_VENDOR_SUB
#define WEBCORE_NAVIGATOR_VENDOR_SUB emptyString()
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR_SUB

namespace WebCore {

NavigatorBase::NavigatorBase(ScriptExecutionContext* context)
#if ENABLE(SERVICE_WORKER)
    : m_serviceWorkerContainer(makeUniqueRef<ServiceWorkerContainer>(context, *this))
#endif
{
#if !ENABLE(SERVICE_WORKER)
    UNUSED_PARAM(context);
#endif
}

NavigatorBase::~NavigatorBase() = default;

String NavigatorBase::appName()
{
    return "Netscape"_s;
}

String NavigatorBase::appVersion() const
{
    // Version is everything in the user agent string past the "Mozilla/" prefix.
    const String& agent = userAgent();
    return agent.substring(agent.find('/') + 1);
}

String NavigatorBase::platform()
{
#if OS(LINUX)
    if (!String(WEBCORE_NAVIGATOR_PLATFORM).isEmpty())
        return WEBCORE_NAVIGATOR_PLATFORM;
    struct utsname osname;
    static NeverDestroyed<String> platformName(uname(&osname) >= 0 ? String(osname.sysname) + " "_str + String(osname.machine) : emptyString());
    return platformName;
#else
    return WEBCORE_NAVIGATOR_PLATFORM;
#endif
}

String NavigatorBase::appCodeName()
{
    return "Mozilla"_s;
}

String NavigatorBase::product()
{
    return WEBCORE_NAVIGATOR_PRODUCT;
}

String NavigatorBase::productSub()
{
    return WEBCORE_NAVIGATOR_PRODUCT_SUB;
}

String NavigatorBase::vendor()
{
    return WEBCORE_NAVIGATOR_VENDOR;
}

String NavigatorBase::vendorSub()
{
    return WEBCORE_NAVIGATOR_VENDOR_SUB;
}

String NavigatorBase::language()
{
    return defaultLanguage();
}

Vector<String> NavigatorBase::languages()
{
    // We intentionally expose only the primary language for privacy reasons.
    return { defaultLanguage() };
}

#if ENABLE(SERVICE_WORKER)
ServiceWorkerContainer& NavigatorBase::serviceWorker()
{
    return m_serviceWorkerContainer;
}

ExceptionOr<ServiceWorkerContainer&> NavigatorBase::serviceWorker(ScriptExecutionContext& context)
{
    if (is<Document>(context) && downcast<Document>(context).isSandboxed(SandboxOrigin))
        return Exception { SecurityError, "Service Worker is disabled because the context is sandboxed and lacks the 'allow-same-origin' flag" };
    return m_serviceWorkerContainer.get();
}
#endif

} // namespace WebCore

/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebProcess.h"

#include "InjectedBundle.h"
#include "QtBuiltinBundle.h"
#include "WKBundleAPICast.h"
#include "WebProcessCreationParameters.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <WebCore/CookieJarQt.h>
#include <WebCore/PageCache.h>
#include <WebCore/RuntimeEnabledFeatures.h>

#if defined(Q_OS_MACX)
#include <dispatch/dispatch.h>
#endif

using namespace WebCore;

namespace WebKit {

static const int DefaultPageCacheCapacity = 20;

void WebProcess::platformSetCacheModel(CacheModel)
{
    // FIXME: see bug 73918
    pageCache()->setCapacity(DefaultPageCacheCapacity);
}

void WebProcess::platformClearResourceCaches(ResourceCachesToClear)
{
}

#if defined(Q_OS_MACX)
static void parentProcessDiedCallback(void*)
{
    QCoreApplication::quit();
}
#endif

void WebProcess::platformInitializeWebProcess(const WebProcessCreationParameters& parameters, CoreIPC::ArgumentDecoder* arguments)
{
    m_networkAccessManager = new QNetworkAccessManager;
    ASSERT(!parameters.cookieStorageDirectory.isEmpty() && !parameters.cookieStorageDirectory.isNull());
    WebCore::SharedCookieJarQt* jar = WebCore::SharedCookieJarQt::create(parameters.cookieStorageDirectory);
    m_networkAccessManager->setCookieJar(jar);
    // Do not let QNetworkAccessManager delete the jar.
    jar->setParent(0);

#if defined(Q_OS_MACX)
    pid_t ppid = getppid();
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, ppid, DISPATCH_PROC_EXIT, queue);
    if (source) {
        dispatch_source_set_event_handler_f(source, parentProcessDiedCallback);
        dispatch_resume(source);
    }
#endif

    // Disable runtime enabled features that have no WebKit2 implementation yet.
#if ENABLE(DEVICE_ORIENTATION)
    WebCore::RuntimeEnabledFeatures::setDeviceMotionEnabled(false);
    WebCore::RuntimeEnabledFeatures::setDeviceOrientationEnabled(false);
#endif
#if ENABLE(SPEECH_INPUT)
    WebCore::RuntimeEnabledFeatures::setSpeechInputEnabled(false);
#endif

    // We'll only install the Qt builtin bundle if we don't have one given by the UI process.
    // Currently only WTR provides its own bundle.
    if (parameters.injectedBundlePath.isEmpty()) {
        m_injectedBundle = InjectedBundle::create(String());
        m_injectedBundle->setSandboxExtension(SandboxExtension::create(parameters.injectedBundlePathExtensionHandle));
        QtBuiltinBundle::shared().initialize(toAPI(m_injectedBundle.get()));
    }
}

void WebProcess::platformTerminate()
{
    delete m_networkAccessManager;
    m_networkAccessManager = 0;
    WebCore::SharedCookieJarQt::shared()->destroy();
}

} // namespace WebKit

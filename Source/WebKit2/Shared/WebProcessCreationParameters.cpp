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
#include "WebProcessCreationParameters.h"

#include "ArgumentCoders.h"
#if USE(CFURLSTORAGESESSIONS) && PLATFORM(WIN)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

WebProcessCreationParameters::WebProcessCreationParameters()
    : shouldTrackVisitedLinks(false)
    , shouldAlwaysUseComplexTextCodePath(false)
    , shouldUseFontSmoothing(true)
    , defaultRequestTimeoutInterval(INT_MAX)
#if PLATFORM(MAC)
    , nsURLCacheMemoryCapacity(0)
    , nsURLCacheDiskCapacity(0)
#elif PLATFORM(WIN)
    , shouldPaintNativeControls(false)
#endif
{
}

void WebProcessCreationParameters::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(injectedBundlePath);
    encoder->encode(injectedBundlePathExtensionHandle);
    encoder->encode(applicationCacheDirectory);
    encoder->encode(databaseDirectory);
    encoder->encode(localStorageDirectory);
    encoder->encode(urlSchemesRegistererdAsEmptyDocument);
    encoder->encode(urlSchemesRegisteredAsSecure);
    encoder->encode(urlSchemesForWhichDomainRelaxationIsForbidden);
    encoder->encode(mimeTypesWithCustomRepresentation);
    encoder->encodeEnum(cacheModel);
    encoder->encode(shouldTrackVisitedLinks);
    encoder->encode(shouldAlwaysUseComplexTextCodePath);
    encoder->encode(shouldUseFontSmoothing);
    encoder->encode(iconDatabaseEnabled);
    encoder->encode(languages);
    encoder->encode(textCheckerState);
    encoder->encode(fullKeyboardAccessEnabled);
    encoder->encode(defaultRequestTimeoutInterval);
#if PLATFORM(MAC) || USE(CFURLSTORAGESESSIONS)
    encoder->encode(uiProcessBundleIdentifier);
#endif
#if PLATFORM(MAC)
    encoder->encode(parentProcessName);
    encoder->encode(presenterApplicationPid);
    encoder->encode(nsURLCachePath);
    encoder->encode(nsURLCacheMemoryCapacity);
    encoder->encode(nsURLCacheDiskCapacity);
    encoder->encode(acceleratedCompositingPort);
    encoder->encode(uiProcessBundleResourcePath);
    encoder->encode(webInspectorBaseDirectory);
#elif PLATFORM(WIN)
    encoder->encode(shouldPaintNativeControls);
    encoder->encode(cfURLCachePath);
    encoder->encode(cfURLCacheDiskCapacity);
    encoder->encode(cfURLCacheMemoryCapacity);
    encoder->encode(initialHTTPCookieAcceptPolicy);
#if USE(CFURLSTORAGESESSIONS)
    CFDataRef storageSession = serializedDefaultStorageSession.get();
    encoder->encodeBool(storageSession);
    if (storageSession)
        CoreIPC::encode(encoder, storageSession);
#endif // USE(CFURLSTORAGESESSIONS)
#endif
#if PLATFORM(QT)
    encoder->encode(cookieStorageDirectory);
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    encoder->encode(notificationPermissions);
#endif
}

bool WebProcessCreationParameters::decode(CoreIPC::ArgumentDecoder* decoder, WebProcessCreationParameters& parameters)
{
    if (!decoder->decode(parameters.injectedBundlePath))
        return false;
    if (!decoder->decode(parameters.injectedBundlePathExtensionHandle))
        return false;
    if (!decoder->decode(parameters.applicationCacheDirectory))
        return false;
    if (!decoder->decode(parameters.databaseDirectory))
        return false;
    if (!decoder->decode(parameters.localStorageDirectory))
        return false;
    if (!decoder->decode(parameters.urlSchemesRegistererdAsEmptyDocument))
        return false;
    if (!decoder->decode(parameters.urlSchemesRegisteredAsSecure))
        return false;
    if (!decoder->decode(parameters.urlSchemesForWhichDomainRelaxationIsForbidden))
        return false;
    if (!decoder->decode(parameters.mimeTypesWithCustomRepresentation))
        return false;
    if (!decoder->decodeEnum(parameters.cacheModel))
        return false;
    if (!decoder->decode(parameters.shouldTrackVisitedLinks))
        return false;
    if (!decoder->decode(parameters.shouldAlwaysUseComplexTextCodePath))
        return false;
    if (!decoder->decode(parameters.shouldUseFontSmoothing))
        return false;
    if (!decoder->decode(parameters.iconDatabaseEnabled))
        return false;

    if (!decoder->decode(parameters.languages))
        return false;
    if (!decoder->decode(parameters.textCheckerState))
        return false;
    if (!decoder->decode(parameters.fullKeyboardAccessEnabled))
        return false;
    if (!decoder->decode(parameters.defaultRequestTimeoutInterval))
        return false;
#if PLATFORM(MAC) || USE(CFURLSTORAGESESSIONS)
    if (!decoder->decode(parameters.uiProcessBundleIdentifier))
        return false;
#endif

#if PLATFORM(MAC)
    if (!decoder->decode(parameters.parentProcessName))
        return false;
    if (!decoder->decode(parameters.presenterApplicationPid))
        return false;
    if (!decoder->decode(parameters.nsURLCachePath))
        return false;
    if (!decoder->decode(parameters.nsURLCacheMemoryCapacity))
        return false;
    if (!decoder->decode(parameters.nsURLCacheDiskCapacity))
        return false;
    if (!decoder->decode(parameters.acceleratedCompositingPort))
        return false;
    if (!decoder->decode(parameters.uiProcessBundleResourcePath))
        return false;
    if (!decoder->decode(parameters.webInspectorBaseDirectory))
        return false;
#elif PLATFORM(WIN)
    if (!decoder->decode(parameters.shouldPaintNativeControls))
        return false;
    if (!decoder->decode(parameters.cfURLCachePath))
        return false;
    if (!decoder->decode(parameters.cfURLCacheDiskCapacity))
        return false;
    if (!decoder->decode(parameters.cfURLCacheMemoryCapacity))
        return false;
    if (!decoder->decode(parameters.initialHTTPCookieAcceptPolicy))
        return false;
#if USE(CFURLSTORAGESESSIONS)
    bool hasStorageSession = false;
    if (!decoder->decode(hasStorageSession))
        return false;
    if (hasStorageSession && !CoreIPC::decode(decoder, parameters.serializedDefaultStorageSession))
        return false;
#endif // USE(CFURLSTORAGESESSIONS)
#endif

#if PLATFORM(QT)
    if (!decoder->decode(parameters.cookieStorageDirectory))
        return false;
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    if (!decoder->decode(parameters.notificationPermissions))
        return false;
#endif

    return true;
}

} // namespace WebKit

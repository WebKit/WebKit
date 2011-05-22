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

#ifndef WebProcessCreationParameters_h
#define WebProcessCreationParameters_h

#include "CacheModel.h"
#include "SandboxExtension.h"
#include "TextCheckerState.h"
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "MachPort.h"
#endif

namespace CoreIPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

namespace WebKit {

struct WebProcessCreationParameters {
    WebProcessCreationParameters();

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebProcessCreationParameters&);

    String injectedBundlePath;
    SandboxExtension::Handle injectedBundlePathExtensionHandle;

    String applicationCacheDirectory;    
    String databaseDirectory;
    String localStorageDirectory;
    Vector<String> urlSchemesRegistererdAsEmptyDocument;
    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesForWhichDomainRelaxationIsForbidden;

    // MIME types for which the UI process will handle showing the data.
    Vector<String> mimeTypesWithCustomRepresentation;

    CacheModel cacheModel;
    bool shouldTrackVisitedLinks;

    bool shouldAlwaysUseComplexTextCodePath;

    bool iconDatabaseEnabled;

    String languageCode;

    TextCheckerState textCheckerState;

    double defaultRequestTimeoutInterval;

#if USE(CFURLSTORAGESESSIONS)
    String uiProcessBundleIdentifier;
#endif

#if PLATFORM(MAC)
    String parentProcessName;

    pid_t presenterApplicationPid;

    // FIXME: These should be merged with CFURLCache counterparts below.
    String nsURLCachePath;
    uint64_t nsURLCacheMemoryCapacity;
    uint64_t nsURLCacheDiskCapacity;

    CoreIPC::MachPort acceleratedCompositingPort;

    String uiProcessBundleResourcePath;

#elif PLATFORM(WIN)
    String cfURLCachePath;
    uint64_t cfURLCacheDiskCapacity;
    uint64_t cfURLCacheMemoryCapacity;

    uint32_t initialHTTPCookieAcceptPolicy;

    bool shouldPaintNativeControls;

#if USE(CFURLSTORAGESESSIONS)
    RetainPtr<CFDataRef> serializedDefaultStorageSession;
#endif // USE(CFURLSTORAGESESSIONS)
#endif // PLATFORM(WIN)
};

} // namespace WebKit

#endif // WebProcessCreationParameters_h

/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#include "SandboxExtension.h"

namespace WebKit {

struct WebProcessDataStoreParameters {
    String applicationCacheDirectory;
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    String applicationCacheFlatFileSubdirectoryName;
    String webSQLDatabaseDirectory;
    SandboxExtension::Handle webSQLDatabaseDirectoryExtensionHandle;
    String mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    String mediaKeyStorageDirectory;
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    String javaScriptConfigurationDirectory;
    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
    bool resourceLoadStatisticsEnabled { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<WebProcessDataStoreParameters> decode(Decoder&);
};

template<class Encoder>
void WebProcessDataStoreParameters::encode(Encoder& encoder) const
{
    encoder << applicationCacheDirectory;
    encoder << applicationCacheDirectoryExtensionHandle;
    encoder << applicationCacheFlatFileSubdirectoryName;
    encoder << webSQLDatabaseDirectory;
    encoder << webSQLDatabaseDirectoryExtensionHandle;
    encoder << mediaCacheDirectory;
    encoder << mediaCacheDirectoryExtensionHandle;
    encoder << mediaKeyStorageDirectory;
    encoder << mediaKeyStorageDirectoryExtensionHandle;
    encoder << javaScriptConfigurationDirectory;
    encoder << javaScriptConfigurationDirectoryExtensionHandle;
    encoder << resourceLoadStatisticsEnabled;
}

template<class Decoder>
Optional<WebProcessDataStoreParameters> WebProcessDataStoreParameters::decode(Decoder& decoder)
{
    WebProcessDataStoreParameters parameters;

    if (!decoder.decode(parameters.applicationCacheDirectory))
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> applicationCacheDirectoryExtensionHandle;
    decoder >> applicationCacheDirectoryExtensionHandle;
    if (!applicationCacheDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.applicationCacheDirectoryExtensionHandle = WTFMove(*applicationCacheDirectoryExtensionHandle);

    if (!decoder.decode(parameters.applicationCacheFlatFileSubdirectoryName))
        return WTF::nullopt;

    if (!decoder.decode(parameters.webSQLDatabaseDirectory))
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> webSQLDatabaseDirectoryExtensionHandle;
    decoder >> webSQLDatabaseDirectoryExtensionHandle;
    if (!webSQLDatabaseDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.webSQLDatabaseDirectoryExtensionHandle = WTFMove(*webSQLDatabaseDirectoryExtensionHandle);

    if (!decoder.decode(parameters.mediaCacheDirectory))
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> mediaCacheDirectoryExtensionHandle;
    decoder >> mediaCacheDirectoryExtensionHandle;
    if (!mediaCacheDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.mediaCacheDirectoryExtensionHandle = WTFMove(*mediaCacheDirectoryExtensionHandle);

    if (!decoder.decode(parameters.mediaKeyStorageDirectory))
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> mediaKeyStorageDirectoryExtensionHandle;
    decoder >> mediaKeyStorageDirectoryExtensionHandle;
    if (!mediaKeyStorageDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.mediaKeyStorageDirectoryExtensionHandle = WTFMove(*mediaKeyStorageDirectoryExtensionHandle);

    if (!decoder.decode(parameters.javaScriptConfigurationDirectory))
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> javaScriptConfigurationDirectoryExtensionHandle;
    decoder >> javaScriptConfigurationDirectoryExtensionHandle;
    if (!javaScriptConfigurationDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.javaScriptConfigurationDirectoryExtensionHandle = WTFMove(*javaScriptConfigurationDirectoryExtensionHandle);

    if (!decoder.decode(parameters.resourceLoadStatisticsEnabled))
        return WTF::nullopt;

    return parameters;
}

} // namespace WebKit

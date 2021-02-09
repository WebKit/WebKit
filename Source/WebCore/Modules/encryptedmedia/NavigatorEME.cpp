/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NavigatorEME.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMLogging.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaKeySystemAccess.h"
#include "Logging.h"
#include <wtf/text/StringBuilder.h>

namespace WTF {

template<typename>
struct LogArgument;

template<typename T>
struct LogArgument<Vector<T>> {
    static String toString(const Vector<T>& value)
    {
        StringBuilder builder;
        builder.append("[");
        for (auto item : value)
            builder.append(LogArgument<T>::toString(item));
        builder.append("]");
        return builder.toString();
    }
};

template<typename T>
struct LogArgument<Optional<T>> {
    static String toString(const Optional<T>& value)
    {
        return value ? "nullopt"_s : LogArgument<T>::toString(value.value());
    }
};

}

namespace WebCore {

template<typename... Arguments>
inline void infoLog(Logger& logger, const Arguments&... arguments)
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    logger.info(LogEME, arguments...);
#else
    UNUSED_PARAM(logger);
#endif
}

static void tryNextSupportedConfiguration(RefPtr<CDM>&& implementation, Vector<MediaKeySystemConfiguration>&& supportedConfigurations, RefPtr<DeferredPromise>&&, Ref<Logger>&&, WTF::Logger::LogSiteIdentifier&&);

void NavigatorEME::requestMediaKeySystemAccess(Navigator& navigator, Document& document, const String& keySystem, Vector<MediaKeySystemConfiguration>&& supportedConfigurations, Ref<DeferredPromise>&& promise)
{
    // https://w3c.github.io/encrypted-media/#dom-navigator-requestmediakeysystemaccess
    // W3C Editor's Draft 09 November 2016
    auto identifier = WTF::Logger::LogSiteIdentifier("NavigatorEME", __func__, &navigator);
    Ref<Logger> logger = document.logger();

    infoLog(logger, identifier, "keySystem(", keySystem, "), supportedConfigurations(", supportedConfigurations, ")");

    // When this method is invoked, the user agent must run the following steps:
    // 1. If keySystem is the empty string, return a promise rejected with a newly created TypeError.
    // 2. If supportedConfigurations is empty, return a promise rejected with a newly created TypeError.
    if (keySystem.isEmpty() || supportedConfigurations.isEmpty()) {
        infoLog(logger, identifier, "Rejected: empty keySystem(", keySystem.isEmpty(), ") or empty supportedConfigurations(", supportedConfigurations.isEmpty(), ")");
        promise->reject(TypeError);
        return;
    }

    document.postTask([keySystem, supportedConfigurations = WTFMove(supportedConfigurations), promise = WTFMove(promise), &document, logger = WTFMove(logger), identifier = WTFMove(identifier)] (ScriptExecutionContext&) mutable {
        // 3. Let document be the calling context's Document.
        // 4. Let origin be the origin of document.
        // 5. Let promise be a new promise.
        // 6. Run the following steps in parallel:
        // 6.1. If keySystem is not one of the Key Systems supported by the user agent, reject promise with a NotSupportedError.
        //      String comparison is case-sensitive.
        if (!CDM::supportsKeySystem(keySystem)) {
            infoLog(logger, identifier, "Rejected: keySystem(", keySystem, ") not supported");
            promise->reject(NotSupportedError);
            return;
        }

        // 6.2. Let implementation be the implementation of keySystem.
        auto implementation = CDM::create(document, keySystem);
        tryNextSupportedConfiguration(WTFMove(implementation), WTFMove(supportedConfigurations), WTFMove(promise), WTFMove(logger), WTFMove(identifier));
    });
}

static void tryNextSupportedConfiguration(RefPtr<CDM>&& implementation, Vector<MediaKeySystemConfiguration>&& supportedConfigurations, RefPtr<DeferredPromise>&& promise, Ref<Logger>&& logger, WTF::Logger::LogSiteIdentifier&& identifier)
{
    // 6.3. For each value in supportedConfigurations:
    if (!supportedConfigurations.isEmpty()) {
        // 6.3.1. Let candidate configuration be the value.
        // 6.3.2. Let supported configuration be the result of executing the Get Supported Configuration
        //        algorithm on implementation, candidate configuration, and origin.
        MediaKeySystemConfiguration candidateConfiguration = WTFMove(supportedConfigurations.first());
        supportedConfigurations.remove(0);

        CDM::SupportedConfigurationCallback callback = [implementation = implementation, supportedConfigurations = WTFMove(supportedConfigurations), promise, logger = WTFMove(logger), identifier = WTFMove(identifier)] (Optional<MediaKeySystemConfiguration> supportedConfiguration) mutable {
            // 6.3.3. If supported configuration is not NotSupported, run the following steps:
            if (supportedConfiguration) {
                // 6.3.3.1. Let access be a new MediaKeySystemAccess object, and initialize it as follows:
                // 6.3.3.1.1. Set the keySystem attribute to keySystem.
                // 6.3.3.1.2. Let the configuration value be supported configuration.
                // 6.3.3.1.3. Let the cdm implementation value be implementation.

                // Obtain reference to the key system string before the `implementation` RefPtr<> is cleared out.
                const String& keySystem = implementation->keySystem();
                auto access = MediaKeySystemAccess::create(keySystem, WTFMove(supportedConfiguration.value()), implementation.releaseNonNull());

                // 6.3.3.2. Resolve promise with access and abort the parallel steps of this algorithm.
                infoLog(logger, identifier, "Resolved: keySystem(", keySystem, "), supportedConfiguration(", supportedConfiguration, ")");
                promise->resolveWithNewlyCreated<IDLInterface<MediaKeySystemAccess>>(WTFMove(access));
                return;
            }

            tryNextSupportedConfiguration(WTFMove(implementation), WTFMove(supportedConfigurations), WTFMove(promise), WTFMove(logger), WTFMove(identifier));
        };
        implementation->getSupportedConfiguration(WTFMove(candidateConfiguration), WTFMove(callback));
        return;
    }

    // 6.4. Reject promise with a NotSupportedError.
    infoLog(logger, identifier, "Rejected: empty supportedConfigurations");
    promise->reject(NotSupportedError);
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)

/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebDriverService.h"

#include "Capabilities.h"
#include "CommandResult.h"
#include <wtf/JSONValues.h>

namespace WebDriver {

void WebDriverService::platformInit()
{
}

Capabilities WebDriverService::platformCapabilities()
{
    Capabilities capabilities;
    capabilities.platformName = String("linux");
    capabilities.setWindowRect = false;
    return capabilities;
}

bool WebDriverService::platformValidateCapability(const String& name, const Ref<JSON::Value>& value) const
{
    if (name != "wpe:browserOptions")
        return true;

    auto browserOptions = value->asObject();
    if (!browserOptions)
        return false;

    if (browserOptions->isNull())
        return true;

    // If browser options are provided, binary is required.
    auto binary = browserOptions->getString("binary"_s);
    if (!binary)
        return false;

    if (auto browserArgumentsValue = browserOptions->getValue("args"_s)) {
        auto browserArguments = browserArgumentsValue->asArray();
        if (!browserArguments)
            return false;

        unsigned browserArgumentsLength = browserArguments->length();
        for (unsigned i = 0; i < browserArgumentsLength; ++i) {
            auto argument = browserArguments->get(i)->asString();
            if (!argument)
                return false;
        }
    }

    if (auto certificatesValue = browserOptions->getValue("certificates"_s)) {
        auto certificates = certificatesValue->asArray();
        if (!certificates)
            return false;

        unsigned certificatesLength = certificates->length();
        for (unsigned i = 0; i < certificatesLength; ++i) {
            auto certificate = certificates->get(i)->asObject();
            if (!certificate)
                return false;

            auto host = certificate->getString("host"_s);
            if (!host)
                return false;

            auto certificateFile = certificate->getString("certificateFile"_s);
            if (!certificateFile)
                return false;
        }
    }

    return true;
}

bool WebDriverService::platformMatchCapability(const String&, const Ref<JSON::Value>&) const
{
    return true;
}

void WebDriverService::platformParseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    capabilities.browserBinary = String("MiniBrowser");
    capabilities.browserArguments = Vector<String> { "--automation"_s };

    auto browserOptions = matchedCapabilities.getObject("wpe:browserOptions"_s);
    if (!browserOptions)
        return;

    auto browserBinary = browserOptions->getString("binary"_s);
    if (!!browserBinary) {
        capabilities.browserBinary = browserBinary;
        capabilities.browserArguments = WTF::nullopt;
    }

    auto browserArguments = browserOptions->getArray("args"_s);
    if (browserArguments && browserArguments->length()) {
        unsigned browserArgumentsLength = browserArguments->length();
        capabilities.browserArguments = Vector<String>();
        capabilities.browserArguments->reserveInitialCapacity(browserArgumentsLength);
        for (unsigned i = 0; i < browserArgumentsLength; ++i) {
            auto argument = browserArguments->get(i)->asString();
            ASSERT(!argument.isNull());
            capabilities.browserArguments->uncheckedAppend(WTFMove(argument));
        }
    }

    auto certificates = browserOptions->getArray("certificates"_s);
    if (certificates && certificates->length()) {
        unsigned certificatesLength = certificates->length();
        capabilities.certificates = Vector<std::pair<String, String>>();
        capabilities.certificates->reserveInitialCapacity(certificatesLength);
        for (unsigned i = 0; i < certificatesLength; ++i) {
            auto certificate = certificates->get(i)->asObject();
            ASSERT(certificate);

            auto host = certificate->getString("host"_s);
            ASSERT(!host.isNull());

            auto certificateFile = certificate->getString("certificateFile"_s);
            ASSERT(!certificateFile.isNull());

            capabilities.certificates->uncheckedAppend({ WTFMove(host), WTFMove(certificateFile) });
        }
    }
}

} // namespace WebDriver

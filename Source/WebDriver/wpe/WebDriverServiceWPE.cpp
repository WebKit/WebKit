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
#include <wtf/text/StringToIntegerConversion.h>

namespace WebDriver {

void WebDriverService::platformInit()
{
}

Capabilities WebDriverService::platformCapabilities()
{
    Capabilities capabilities;
    capabilities.platformName = "linux"_si;
    capabilities.setWindowRect = false;
    return capabilities;
}

bool WebDriverService::platformValidateCapability(StringImpl* name, const Ref<JSON::Value>& value) const
{
    if (!equal(name, "wpe:browserOptions"_s))
        return true;

    auto browserOptions = value->asObject();
    if (!browserOptions)
        return false;

    if (browserOptions->isNull())
        return true;

    if (auto binaryValue = browserOptions->getValue("binary"_si)) {
        if (!binaryValue->asString())
            return false;
    }

    if (auto browserArgumentsValue = browserOptions->getValue("args"_si)) {
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

    if (auto certificatesValue = browserOptions->getValue("certificates"_si)) {
        auto certificates = certificatesValue->asArray();
        if (!certificates)
            return false;

        unsigned certificatesLength = certificates->length();
        for (unsigned i = 0; i < certificatesLength; ++i) {
            auto certificate = certificates->get(i)->asObject();
            if (!certificate)
                return false;

            auto host = certificate->getString("host"_si);
            if (!host)
                return false;

            auto certificateFile = certificate->getString("certificateFile"_si);
            if (!certificateFile)
                return false;
        }
    }

    return true;
}

bool WebDriverService::platformMatchCapability(StringImpl*, const Ref<JSON::Value>&) const
{
    return true;
}

void WebDriverService::platformParseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    capabilities.browserBinary = "MiniBrowser"_si;
    capabilities.browserArguments = Vector<RefPtr<StringImpl>> { "--automation"_si };

    auto browserOptions = matchedCapabilities.getObject("wpe:browserOptions"_si);
    if (!browserOptions)
        return;

    auto browserBinary = browserOptions->getString("binary"_si);
    if (!!browserBinary) {
        capabilities.browserBinary = browserBinary;
        capabilities.browserArguments = std::nullopt;
    }

    auto browserArguments = browserOptions->getArray("args"_si);
    if (browserArguments && browserArguments->length()) {
        unsigned browserArgumentsLength = browserArguments->length();
        capabilities.browserArguments = Vector<RefPtr<StringImpl>>();
        capabilities.browserArguments->reserveInitialCapacity(browserArgumentsLength);
        for (unsigned i = 0; i < browserArgumentsLength; ++i) {
            auto argument = browserArguments->get(i)->asString();
            ASSERT(argument);
            capabilities.browserArguments->append(WTF::move(argument));
        }
    }

    auto certificates = browserOptions->getArray("certificates"_si);
    if (certificates && certificates->length()) {
        unsigned certificatesLength = certificates->length();
        capabilities.certificates = Vector<std::pair<RefPtr<StringImpl>, RefPtr<StringImpl>>>();
        capabilities.certificates->reserveInitialCapacity(certificatesLength);
        for (unsigned i = 0; i < certificatesLength; ++i) {
            auto certificate = certificates->get(i)->asObject();
            ASSERT(certificate);

            auto host = certificate->getString("host"_si);
            ASSERT(host);

            auto certificateFile = certificate->getString("certificateFile"_si);
            ASSERT(certificateFile);

            capabilities.certificates->append({ WTF::move(host), WTF::move(certificateFile) });
        }
    }

    RefPtr<StringImpl> targetString;
    if (browserOptions->getString("targetAddress"_si, targetString)) {
        auto position = reverseFind(targetString, ':');
        if (position != notFound) {
            capabilities.targetAddr = left(targetString, position);
            capabilities.targetPort = parseIntegerAllowingTrailingJunk<uint16_t>(StringView { targetString }.substring(position + 1)).value_or(0);
        }
    }
}

} // namespace WebDriver

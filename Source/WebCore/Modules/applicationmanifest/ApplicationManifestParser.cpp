/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ApplicationManifestParser.h"

#if ENABLE(APPLICATION_MANIFEST)

#include "SecurityOrigin.h"
#include <JavaScriptCore/ConsoleMessage.h>

namespace WebCore {

ApplicationManifest ApplicationManifestParser::parse(ScriptExecutionContext& scriptExecutionContext, const String& source, const URL& manifestURL, const URL& documentURL)
{
    ApplicationManifestParser parser { &scriptExecutionContext };
    return parser.parseManifest(source, manifestURL, documentURL);
}

ApplicationManifest ApplicationManifestParser::parse(const String& source, const URL& manifestURL, const URL& documentURL)
{
    ApplicationManifestParser parser { nullptr };
    return parser.parseManifest(source, manifestURL, documentURL);
}

ApplicationManifestParser::ApplicationManifestParser(RefPtr<ScriptExecutionContext> consoleContext)
    : m_consoleContext(consoleContext)
{
}

ApplicationManifest ApplicationManifestParser::parseManifest(const String& text, const URL& manifestURL, const URL& documentURL)
{
    m_manifestURL = manifestURL;

    RefPtr<JSON::Value> jsonValue;
    if (!JSON::Value::parseJSON(text, jsonValue)) {
        logDeveloperWarning(ASCIILiteral("The manifest is not valid JSON data."));
        jsonValue = JSON::Object::create();
    }

    RefPtr<JSON::Object> manifest;
    if (!jsonValue->asObject(manifest)) {
        logDeveloperWarning(ASCIILiteral("The manifest is not a JSON value of type \"object\"."));
        manifest = JSON::Object::create();
    }

    ApplicationManifest parsedManifest;

    parsedManifest.startURL = parseStartURL(*manifest, documentURL);
    parsedManifest.display = parseDisplay(*manifest);
    parsedManifest.name = parseName(*manifest);
    parsedManifest.description = parseDescription(*manifest);
    parsedManifest.shortName = parseShortName(*manifest);
    parsedManifest.scope = parseScope(*manifest, documentURL, parsedManifest.startURL);

    return parsedManifest;
}

void ApplicationManifestParser::logManifestPropertyNotAString(const String& propertyName)
{
    logDeveloperWarning("The value of \"" + propertyName + "\" is not a string.");
}

void ApplicationManifestParser::logManifestPropertyInvalidURL(const String& propertyName)
{
    logDeveloperWarning("The value of \"" + propertyName + "\" is not a valid URL.");
}

void ApplicationManifestParser::logDeveloperWarning(const String& message)
{
    if (m_consoleContext)
        m_consoleContext->addConsoleMessage(std::make_unique<Inspector::ConsoleMessage>(JSC::MessageSource::Other, JSC::MessageType::Log, JSC::MessageLevel::Warning, ASCIILiteral("Parsing application manifest ") + m_manifestURL.string() + ASCIILiteral(": ") + message));
}

URL ApplicationManifestParser::parseStartURL(const JSON::Object& manifest, const URL& documentURL)
{
    RefPtr<JSON::Value> value;
    if (!manifest.getValue("start_url", value))
        return documentURL;

    String stringValue;
    if (!value->asString(stringValue)) {
        logManifestPropertyNotAString(ASCIILiteral("start_url"));
        return documentURL;
    }

    if (stringValue.isEmpty())
        return documentURL;

    URL startURL(m_manifestURL, stringValue);
    if (!startURL.isValid()) {
        logManifestPropertyInvalidURL(ASCIILiteral("start_url"));
        return documentURL;
    }

    if (!protocolHostAndPortAreEqual(startURL, documentURL)) {
        auto startURLOrigin = SecurityOrigin::create(startURL);
        auto documentOrigin = SecurityOrigin::create(documentURL);
        logDeveloperWarning(ASCIILiteral("The start_url's origin of \"") + startURLOrigin->toString() + ASCIILiteral("\" is different from the document's origin of \"") + documentOrigin->toString() + ASCIILiteral("\"."));
        return documentURL;
    }

    return startURL;
}

ApplicationManifest::Display ApplicationManifestParser::parseDisplay(const JSON::Object& manifest)
{
    RefPtr<JSON::Value> value;
    if (!manifest.getValue(ASCIILiteral("display"), value))
        return ApplicationManifest::Display::Browser;

    String stringValue;
    if (!value->asString(stringValue)) {
        logManifestPropertyNotAString(ASCIILiteral("display"));
        return ApplicationManifest::Display::Browser;
    }

    stringValue = stringValue.stripWhiteSpace().convertToASCIILowercase();

    if (stringValue == "fullscreen")
        return ApplicationManifest::Display::Fullscreen;
    if (stringValue == "standalone")
        return ApplicationManifest::Display::Standalone;
    if (stringValue == "minimal-ui")
        return ApplicationManifest::Display::MinimalUI;
    if (stringValue == "browser")
        return ApplicationManifest::Display::Browser;

    logDeveloperWarning(ASCIILiteral("\"") + stringValue + ASCIILiteral("\" is not a valid display mode."));
    return ApplicationManifest::Display::Browser;
}

String ApplicationManifestParser::parseName(const JSON::Object& manifest)
{
    return parseGenericString(manifest, ASCIILiteral("name"));
}

String ApplicationManifestParser::parseDescription(const JSON::Object& manifest)
{
    return parseGenericString(manifest, ASCIILiteral("description"));
}

String ApplicationManifestParser::parseShortName(const JSON::Object& manifest)
{
    return parseGenericString(manifest, ASCIILiteral("short_name"));
}

static bool isInScope(const URL& scopeURL, const URL& targetURL)
{
    // 1. If scopeURL is undefined (i.e., it is unbounded because of an error or it was not declared in the manifest), return true.
    if (scopeURL.isNull() || scopeURL.isEmpty())
        return true;

    // 2. Let target be a new URL using targetURL as input. If target is failure, return false.
    if (!targetURL.isValid())
        return false;

    // 3. Let scopePath be the elements of scopes's path, separated by U+002F (/).
    auto scopePath = scopeURL.path();

    // 4. Let targetPath be the elements of target's path, separated by U+002F (/).
    auto targetPath = targetURL.path();

    // 5. If target is same origin as scope and targetPath starts with scopePath, return true.
    if (protocolHostAndPortAreEqual(scopeURL, targetURL) && targetPath.startsWith(scopePath))
        return true;

    // 6. Otherwise, return false.
    return false;
}

URL ApplicationManifestParser::parseScope(const JSON::Object& manifest, const URL& documentURL, const URL& startURL)
{
    URL defaultScope { startURL, "./" };

    RefPtr<JSON::Value> value;
    if (!manifest.getValue("scope", value))
        return defaultScope;

    String stringValue;
    if (!value->asString(stringValue)) {
        logManifestPropertyNotAString(ASCIILiteral("scope"));
        return defaultScope;
    }

    if (stringValue.isEmpty())
        return defaultScope;

    URL scopeURL(m_manifestURL, stringValue);
    if (!scopeURL.isValid()) {
        logManifestPropertyInvalidURL(ASCIILiteral("scope"));
        return defaultScope;
    }

    if (!protocolHostAndPortAreEqual(scopeURL, documentURL)) {
        auto scopeURLOrigin = SecurityOrigin::create(scopeURL);
        auto documentOrigin = SecurityOrigin::create(documentURL);
        logDeveloperWarning(ASCIILiteral("The scope's origin of \"") + scopeURLOrigin->toString() + ASCIILiteral("\" is different from the document's origin of \"") + documentOrigin->toString() + ASCIILiteral("\"."));
        return defaultScope;
    }

    if (!isInScope(scopeURL, startURL)) {
        logDeveloperWarning(ASCIILiteral("The start URL is not within scope of the provided scope URL."));
        return defaultScope;
    }

    return scopeURL;
}

String ApplicationManifestParser::parseGenericString(const JSON::Object& manifest, const String& propertyName)
{
    RefPtr<JSON::Value> value;
    if (!manifest.getValue(propertyName, value))
        return { };

    String stringValue;
    if (!value->asString(stringValue)) {
        logManifestPropertyNotAString(propertyName);
        return { };
    }

    return stringValue.stripWhiteSpace();
}

} // namespace WebCore

#endif // ENABLE(APPLICATION_MANIFEST)

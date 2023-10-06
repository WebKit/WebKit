/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "CSSParser.h"
#include "Color.h"
#include "Document.h"
#include "SecurityOrigin.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <optional>
#include <wtf/SortedArrayMap.h>

namespace WebCore {

ApplicationManifest ApplicationManifestParser::parse(Document& document, const String& source, const URL& manifestURL, const URL& documentURL)
{
    ApplicationManifestParser parser { &document };
    return parser.parseManifest(source, manifestURL, documentURL);
}

ApplicationManifest ApplicationManifestParser::parse(const String& source, const URL& manifestURL, const URL& documentURL)
{
    ApplicationManifestParser parser { nullptr };
    return parser.parseManifest(source, manifestURL, documentURL);
}

ApplicationManifestParser::ApplicationManifestParser(RefPtr<Document> document)
    : m_document(document)
{
}

ApplicationManifest ApplicationManifestParser::parseManifest(const String& text, const URL& manifestURL, const URL& documentURL)
{
    m_manifestURL = manifestURL;

    auto jsonValue = JSON::Value::parseJSON(text);
    if (!jsonValue) {
        logDeveloperWarning("The manifest is not valid JSON data."_s);
        jsonValue = JSON::Object::create();
    }

    auto manifest = jsonValue->asObject();
    if (!manifest) {
        logDeveloperWarning("The manifest is not a JSON value of type \"object\"."_s);
        manifest = JSON::Object::create();
    }

    ApplicationManifest parsedManifest;

    parsedManifest.rawJSON = text;
    parsedManifest.manifestURL = manifestURL;
    parsedManifest.startURL = parseStartURL(*manifest, documentURL);
    parsedManifest.display = parseDisplay(*manifest);
    parsedManifest.name = parseName(*manifest);
    parsedManifest.description = parseDescription(*manifest);
    parsedManifest.shortName = parseShortName(*manifest);
    if (auto parsedScope = parseScope(*manifest, documentURL, parsedManifest.startURL))
        parsedManifest.scope = WTFMove(*parsedScope);
    else {
        parsedManifest.scope = URL { parsedManifest.startURL, "./"_s };
        parsedManifest.isDefaultScope = true;
    }
    parsedManifest.backgroundColor = parseColor(*manifest, "background_color"_s);
    parsedManifest.themeColor = parseColor(*manifest, "theme_color"_s);
    parsedManifest.categories = parseCategories(*manifest);
    parsedManifest.icons = parseIcons(*manifest);
    parsedManifest.shortcuts = parseShortcuts(*manifest);
    parsedManifest.id = parseId(*manifest, parsedManifest.startURL);
    parsedManifest.orientation = parseOrientation(*manifest);

    if (m_document)
        m_document->processApplicationManifest(parsedManifest);

    return parsedManifest;
}

void ApplicationManifestParser::logManifestPropertyNotAString(const String& propertyName)
{
    logDeveloperWarning(makeString("The value of \""_s, propertyName, "\" is not a string."_s));
}

void ApplicationManifestParser::logManifestPropertyInvalidURL(const String& propertyName)
{
    logDeveloperWarning(makeString("The value of \""_s, propertyName, "\" is not a valid URL."_s));
}

void ApplicationManifestParser::logDeveloperWarning(const String& message)
{
    if (m_document)
        m_document->addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(JSC::MessageSource::Other, JSC::MessageType::Log, JSC::MessageLevel::Warning, makeString("Parsing application manifest "_s, m_manifestURL.string(), ": "_s, message)));
}

URL ApplicationManifestParser::parseStartURL(const JSON::Object& manifest, const URL& documentURL)
{
    auto value = manifest.getValue("start_url"_s);
    if (!value)
        return documentURL;

    auto stringValue = value->asString();
    if (!stringValue) {
        logManifestPropertyNotAString("start_url"_s);
        return documentURL;
    }

    if (stringValue.isEmpty())
        return documentURL;

    URL startURL(m_manifestURL, stringValue);
    if (!startURL.isValid()) {
        logManifestPropertyInvalidURL("start_url"_s);
        return documentURL;
    }

    if (!protocolHostAndPortAreEqual(startURL, documentURL)) {
        auto startURLOrigin = SecurityOrigin::create(startURL);
        auto documentOrigin = SecurityOrigin::create(documentURL);
        logDeveloperWarning(makeString("The start_url's origin of \""_s, startURLOrigin->toString(), "\" is different from the document's origin of \""_s, documentOrigin->toString(), "\"."_s));
        return documentURL;
    }

    return startURL;
}

ApplicationManifest::Display ApplicationManifestParser::parseDisplay(const JSON::Object& manifest)
{
    auto value = manifest.getValue("display"_s);
    if (!value)
        return ApplicationManifest::Display::Browser;

    auto stringValue = value->asString();
    if (!stringValue) {
        logManifestPropertyNotAString("display"_s);
        return ApplicationManifest::Display::Browser;
    }

    static constexpr std::pair<ComparableLettersLiteral, ApplicationManifest::Display> displayValueMappings[] = {
        { "browser", ApplicationManifest::Display::Browser },
        { "fullscreen", ApplicationManifest::Display::Fullscreen },
        { "minimal-ui", ApplicationManifest::Display::MinimalUI },
        { "standalone", ApplicationManifest::Display::Standalone },
    };
    static constexpr SortedArrayMap displayValues { displayValueMappings };

    if (auto* displayValue = displayValues.tryGet(StringView(stringValue).trim(isUnicodeCompatibleASCIIWhitespace<UChar>)))
        return *displayValue;

    logDeveloperWarning(makeString("\""_s, stringValue, "\" is not a valid display mode."_s));
    return ApplicationManifest::Display::Browser;
}

const std::optional<ScreenOrientationLockType> ApplicationManifestParser::parseOrientation(const JSON::Object& manifest)
{
    auto value = manifest.getValue("orientation"_s);
    if (!value)
        return std::nullopt;

    auto stringValue = value->asString();
    if (!stringValue) {
        logManifestPropertyNotAString("orientation"_s);
        return std::nullopt;
    }

    static constexpr std::pair<ComparableLettersLiteral, WebCore::ScreenOrientationLockType> orientationValueMappings[] = {
        { "any", WebCore::ScreenOrientationLockType::Any },
        { "landscape", WebCore::ScreenOrientationLockType::Landscape },
        { "landscape-primary", WebCore::ScreenOrientationLockType::LandscapePrimary },
        { "landscape-secondary", WebCore::ScreenOrientationLockType::LandscapeSecondary },
        { "natural", WebCore::ScreenOrientationLockType::Natural },
        { "portrait", WebCore::ScreenOrientationLockType::Portrait },
        { "portrait-primary", WebCore::ScreenOrientationLockType::PortraitPrimary },
        { "portrait-secondary", WebCore::ScreenOrientationLockType::PortraitSecondary },
    };

    static SortedArrayMap orientationValues { orientationValueMappings };

    if (auto* orientationValue = orientationValues.tryGet(StringView(stringValue).trim(isUnicodeCompatibleASCIIWhitespace<UChar>)))
        return *orientationValue;

    logDeveloperWarning(makeString("\""_s, stringValue, "\" is not a valid orientation."_s));
    return std::nullopt;
}

String ApplicationManifestParser::parseName(const JSON::Object& manifest)
{
    return parseGenericString(manifest, "name"_s);
}

String ApplicationManifestParser::parseDescription(const JSON::Object& manifest)
{
    return parseGenericString(manifest, "description"_s);
}

String ApplicationManifestParser::parseShortName(const JSON::Object& manifest)
{
    return parseGenericString(manifest, "short_name"_s);
}

Vector<String> ApplicationManifestParser::parseCategories(const JSON::Object& manifest)
{
    auto manifestCategories = manifest.getValue("categories"_s);

    Vector<String> categoryResources;
    if (!manifestCategories)
        return categoryResources;

    auto manifestCategoriesArray = manifestCategories->asArray();
    if (!manifestCategoriesArray) {
        logDeveloperWarning("The value of categories is not a valid array."_s);
        return categoryResources;
    }

    for (const auto& categoryValue : *manifestCategoriesArray) {
        auto categoryObject = categoryValue->asString();
        if (!categoryObject)
            continue;

        categoryResources.append(WTFMove(categoryObject));
    }

    return categoryResources;
}

Vector<ApplicationManifest::Icon> ApplicationManifestParser::parseIcons(const JSON::Object& manifest)
{
    auto manifestIcons = manifest.getValue("icons"_s);

    Vector<ApplicationManifest::Icon> imageResources;
    if (!manifestIcons)
        return imageResources;

    auto manifestIconsArray = manifestIcons->asArray();
    if (!manifestIconsArray) {
        logDeveloperWarning("The value of icons is not a valid array."_s);
        return imageResources;
    }

    for (const auto& iconValue : *manifestIconsArray) {
        ApplicationManifest::Icon currentIcon;
        auto iconObject = iconValue->asObject();
        if (!iconObject)
            continue;
        const auto& iconJSON = *iconObject;

        auto srcValue = iconJSON.getValue("src"_s);
        if (!srcValue)
            continue;
        auto srcStringValue = srcValue->asString();
        if (!srcStringValue) {
            logManifestPropertyNotAString("src"_s);
            continue;
        }
        URL srcURL(m_manifestURL, srcStringValue);
        if (srcURL.isEmpty())
            continue;
        if (!srcURL.isValid()) {
            logManifestPropertyInvalidURL("src"_s);
            continue;
        }
        currentIcon.src = srcURL;

        currentIcon.sizes = parseGenericString(iconJSON, "sizes"_s).split(' ');

        currentIcon.type = parseGenericString(iconJSON, "type"_s);

        auto purposeValue = iconJSON.getValue("purpose"_s);
        OptionSet<ApplicationManifest::Icon::Purpose> purposes;

        if (!purposeValue) {
            purposes.add(ApplicationManifest::Icon::Purpose::Any);
            currentIcon.purposes = purposes;
        } else {
            auto purposeStringValue = purposeValue->asString();
            if (!purposeStringValue) {
                logManifestPropertyNotAString("purpose"_s);
                purposes.add(ApplicationManifest::Icon::Purpose::Any);
                currentIcon.purposes = purposes;
            } else {
                for (auto keyword : StringView(purposeStringValue).trim(isUnicodeCompatibleASCIIWhitespace<UChar>).splitAllowingEmptyEntries(' ')) {
                    if (equalLettersIgnoringASCIICase(keyword, "monochrome"_s))
                        purposes.add(ApplicationManifest::Icon::Purpose::Monochrome);
                    else if (equalLettersIgnoringASCIICase(keyword, "maskable"_s))
                        purposes.add(ApplicationManifest::Icon::Purpose::Maskable);
                    else if (equalLettersIgnoringASCIICase(keyword, "any"_s))
                        purposes.add(ApplicationManifest::Icon::Purpose::Any);
                    else
                        logDeveloperWarning(makeString("\""_s, purposeStringValue, "\" is not a valid purpose."_s));
                }

                if (purposes.isEmpty())
                    continue;

                currentIcon.purposes = purposes;
            }
        }

        imageResources.append(WTFMove(currentIcon));
    }

    return imageResources;
}

Vector<ApplicationManifest::Shortcut> ApplicationManifestParser::parseShortcuts(const JSON::Object& manifest)
{
    auto manifestShortcuts = manifest.getValue("shortcuts"_s);

    Vector<ApplicationManifest::Shortcut> shortcutResources;
    if (!manifestShortcuts)
        return shortcutResources;

    auto manifestShortcutsArray = manifestShortcuts->asArray();
    if (!manifestShortcutsArray) {
        logDeveloperWarning("The value of shortcuts is not a valid array."_s);
        return shortcutResources;
    }

    for (const auto& shortcutValue : *manifestShortcutsArray) {
        ApplicationManifest::Shortcut currentShortcut;
        auto shortcutObject = shortcutValue->asObject();
        if (!shortcutObject)
            continue;
        const auto& shortcutJSON = *shortcutObject;

        auto urlValue = shortcutJSON.getValue("url"_s);
        if (!urlValue)
            continue;
        auto urlStringValue = urlValue->asString();
        if (!urlStringValue) {
            logManifestPropertyNotAString("url"_s);
            continue;
        }

        URL shortcutURL(m_manifestURL, urlStringValue);
        if (shortcutURL.isEmpty())
            continue;
        if (!shortcutURL.isValid()) {
            logManifestPropertyInvalidURL("url"_s);
            continue;
        }
        currentShortcut.url = WTFMove(shortcutURL);
        currentShortcut.name = parseGenericString(shortcutJSON, "name"_s);
        currentShortcut.icons = parseIcons(shortcutJSON);

        shortcutResources.append(WTFMove(currentShortcut));
    }

    return shortcutResources;
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

URL ApplicationManifestParser::parseId(const JSON::Object& manifest, const URL& startURL)
{
    auto idValue = manifest.getValue("id"_s);
    if (!idValue)
        return startURL;

    auto idStringValue = idValue->asString();
    if (!idStringValue) {
        logManifestPropertyNotAString("id"_s);
        return startURL;
    }

    if (idStringValue.isEmpty())
        return startURL;

    auto baseOrigin = SecurityOrigin::create(startURL);

    URL idURL(baseOrigin->toURL(), idStringValue);

    if (!idURL.isValid()) {
        logManifestPropertyInvalidURL("id"_s);
        return startURL;
    }

    if (!protocolHostAndPortAreEqual(idURL, startURL))
        return startURL;

    return idURL;
}

std::optional<URL> ApplicationManifestParser::parseScope(const JSON::Object& manifest, const URL& documentURL, const URL& startURL)
{
    auto value = manifest.getValue("scope"_s);
    if (!value)
        return std::nullopt;

    auto stringValue = value->asString();
    if (!stringValue) {
        logManifestPropertyNotAString("scope"_s);
        return std::nullopt;
    }

    if (stringValue.isEmpty())
        return std::nullopt;

    URL scopeURL(m_manifestURL, stringValue);
    if (!scopeURL.isValid()) {
        logManifestPropertyInvalidURL("scope"_s);
        return std::nullopt;
    }

    if (!protocolHostAndPortAreEqual(scopeURL, documentURL)) {
        auto scopeURLOrigin = SecurityOrigin::create(scopeURL);
        auto documentOrigin = SecurityOrigin::create(documentURL);
        logDeveloperWarning(makeString("The scope's origin of \""_s, scopeURLOrigin->toString(), "\" is different from the document's origin of \""_s, documentOrigin->toString(), "\"."_s));
        return std::nullopt;
    }

    if (!isInScope(scopeURL, startURL)) {
        logDeveloperWarning("The start URL is not within scope of the provided scope URL."_s);
        return std::nullopt;
    }

    return scopeURL;
}

Color ApplicationManifestParser::parseColor(const JSON::Object& manifest, const String& propertyName)
{
    return CSSParser::parseColorWithoutContext(parseGenericString(manifest, propertyName));
}

String ApplicationManifestParser::parseGenericString(const JSON::Object& manifest, const String& propertyName)
{
    auto value = manifest.getValue(propertyName);
    if (!value)
        return { };

    auto stringValue = value->asString();
    if (!stringValue) {
        logManifestPropertyNotAString(propertyName);
        return { };
    }

    return stringValue.trim(deprecatedIsSpaceOrNewline);
}

} // namespace WebCore

#endif // ENABLE(APPLICATION_MANIFEST)

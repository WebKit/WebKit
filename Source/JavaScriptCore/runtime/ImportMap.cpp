/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ImportMap.h"

#include "SourceCode.h"
#include <algorithm>
#include <wtf/JSONValues.h>

namespace JSC {
namespace ImportMapInternal {
static constexpr bool verbose = false;
}

Expected<URL, String> ImportMap::resolveImportMatch(const String& normalizedSpecifier, const URL& asURL, const SpecifierMap& specifierMap)
{
    // https://wicg.github.io/import-maps/#resolve-an-imports-match

    auto result = specifierMap.find(normalizedSpecifier);

    // 1.1.1. If resolutionResult is null, then throw a TypeError indicating that resolution of specifierKey was blocked by a null entry.
    if (result != specifierMap.end()) {
        if (result->value.isNull())
            return makeUnexpected("speficier is blocked"_s);
        return result->value;
    }

    if (!asURL.isValid() || asURL.hasSpecialScheme()) {
        int64_t length = -1;
        std::optional<URL> matched;
        for (auto& [key, value] : specifierMap) {
            if (key.endsWith('/')) {
                auto position = normalizedSpecifier.find(key);
                if (position == 0) {
                    if (key.length() > length) {
                        matched = value;
                        length = key.length();
                    }
                }
            }
        }

        if (matched) {
            if (matched->isNull())
                return makeUnexpected("speficier is blocked"_s);
            auto afterPrefix = normalizedSpecifier.substring(length);
            ASSERT(matched->string().endsWith('/'));
            URL url { matched.value(), afterPrefix };
            if (!url.isValid())
                return makeUnexpected("speficier is blocked"_s);
            if (!url.string().startsWith(matched->string()))
                return makeUnexpected("speficier is blocked"_s);
            return url;
        }
    }

    return { };
}

static URL parseURLLikeModuleSpecifier(const String& specifier, const URL& baseURL)
{
    // https://wicg.github.io/import-maps/#parse-a-url-like-import-specifier

    if (specifier.startsWith('/') || specifier.startsWith("./"_s) || specifier.startsWith("../"_s))
        return URL(baseURL, specifier);

    return URL { specifier };
}

URL ImportMap::resolve(const String& specifier, const URL& baseURL) const
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier
    // https://wicg.github.io/import-maps/#new-resolve-algorithm

    URL asURL = parseURLLikeModuleSpecifier(specifier, baseURL);
    String normalizedSpecifier = asURL.isValid() ? asURL.string() : specifier;

    dataLogLnIf(ImportMapInternal::verbose, "Resolve ", specifier, " with ", baseURL);
    for (auto& entry : m_scopes) {
        dataLogLnIf(ImportMapInternal::verbose, "    Scope ", entry.m_scope);
        if (entry.m_scope == baseURL || (entry.m_scope.string().endsWith('/') && baseURL.string().startsWith(entry.m_scope.string()))) {
            dataLogLnIf(ImportMapInternal::verbose, "        Matching");
            auto result = resolveImportMatch(normalizedSpecifier, asURL, entry.m_map);
            if (!result)
                return { };
            URL scopeImportsMatch = WTFMove(result.value());
            if (!scopeImportsMatch.isNull())
                return scopeImportsMatch;
        }
    }

    dataLogLnIf(ImportMapInternal::verbose, "    Matching with imports");
    auto result = resolveImportMatch(normalizedSpecifier, asURL, m_imports);
    if (!result)
        return { };
    URL topLevelImportsMatch = WTFMove(result.value());
    if (!topLevelImportsMatch.isNull())
        return topLevelImportsMatch;

    if (asURL.isValid())
        return asURL;

    return { };
}

static String normalizeSpecifierKey(const String& specifierKey, const URL& baseURL, ImportMap::Reporter* reporter)
{
    // https://wicg.github.io/import-maps/#normalize-a-specifier-key

    if (UNLIKELY(specifierKey.isEmpty())) {
        if (reporter)
            reporter->reportWarning("specifier key is empty"_s);
        return nullString();
    }
    URL url = parseURLLikeModuleSpecifier(specifierKey, baseURL);
    if (url.isValid())
        return url.string();
    return specifierKey;
}

static ImportMap::SpecifierMap sortAndNormalizeSpecifierMap(Ref<JSON::Object> importsMap, const URL& baseURL, ImportMap::Reporter* reporter)
{
    // https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map

    ImportMap::SpecifierMap normalized;
    for (auto& [key, value] : importsMap.get()) {
        String normalizedSpecifierKey = normalizeSpecifierKey(key, baseURL, reporter);
        if (normalizedSpecifierKey.isNull())
            continue;
        if (auto valueAsString = value->asString(); LIKELY(!valueAsString.isNull())) {
            URL addressURL = parseURLLikeModuleSpecifier(valueAsString, baseURL);
            if (UNLIKELY(!addressURL.isValid())) {
                if (reporter)
                    reporter->reportWarning(makeString("value in specifier map cannot be parsed as URL "_s, valueAsString));
                normalized.add(normalizedSpecifierKey, URL { });
                continue;
            }
            if (UNLIKELY(key.endsWith('/') && !addressURL.string().endsWith('/'))) {
                if (reporter)
                    reporter->reportWarning(makeString("address "_s, addressURL.string(), " does not end with '/' while key "_s, key, " ends with '/'"_s));
                normalized.add(normalizedSpecifierKey, URL { });
                continue;
            }
            normalized.add(normalizedSpecifierKey, WTFMove(addressURL));
        } else {
            if (reporter)
                reporter->reportWarning("value in specifier map needs to be a string"_s);
            normalized.add(normalizedSpecifierKey, URL { });
            continue;
        }
    }
    return normalized;
}

Expected<void, String> ImportMap::registerImportMap(const SourceCode& sourceCode, const URL& baseURL, ImportMap::Reporter* reporter)
{
    // https://wicg.github.io/import-maps/#integration-register-an-import-map
    // https://wicg.github.io/import-maps/#parsing

    auto result = JSON::Value::parseJSON(sourceCode.view());
    if (!result)
        return makeUnexpected("ImportMap has invalid JSON"_s);

    auto rootMap = result->asObject();
    if (!rootMap)
        return makeUnexpected("ImportMap is not a map"_s);

    SpecifierMap normalizedImports;
    if (auto importsMapValue = rootMap->getValue("imports"_s)) {
        auto importsMap = importsMapValue->asObject();
        if (!importsMap)
            return makeUnexpected("imports is not a map"_s);

        normalizedImports = sortAndNormalizeSpecifierMap(importsMap.releaseNonNull(), baseURL, reporter);
    }

    Scopes scopes;
    if (auto scopesMapValue = rootMap->getValue("scopes"_s)) {
        auto scopesMap = scopesMapValue->asObject();
        if (!scopesMap)
            return makeUnexpected("scopes is not a map"_s);

        // https://wicg.github.io/import-maps/#sort-and-normalize-scopes
        for (auto& [key, value] : *scopesMap) {
            auto potentialSpecifierMap = value->asObject();
            if (!potentialSpecifierMap)
                return makeUnexpected("scopes' value is not a map"_s);
            URL scopePrefixURL { baseURL, key }; // Do not use parseURLLikeModuleSpecifier since we should accept non relative path.
            dataLogLnIf(ImportMapInternal::verbose, "scope key ", key, " and URL ", scopePrefixURL);
            if (UNLIKELY(!scopePrefixURL.isValid())) {
                if (reporter)
                    reporter->reportWarning(makeString("scope key"_s, key, " was not parsable"_s));
                continue;
            }

            scopes.append({ scopePrefixURL, sortAndNormalizeSpecifierMap(potentialSpecifierMap.releaseNonNull(), baseURL, reporter) });
        }
    }

    // Sort to accending order. So, more specific scope will come first.
    std::sort(scopes.begin(), scopes.end(), [&](const auto& lhs, const auto& rhs) -> bool {
        return codePointCompareLessThan(rhs.m_scope.string(), lhs.m_scope.string());
    });

    m_imports = WTFMove(normalizedImports);
    m_scopes = WTFMove(scopes);
    return { };
}


} // namespace JSC

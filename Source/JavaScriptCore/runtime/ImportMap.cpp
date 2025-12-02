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
#include <wtf/text/MakeString.h>

namespace JSC {
namespace ImportMapInternal {
static constexpr bool verbose = false;

// https://html.spec.whatwg.org/C#merge-module-specifier-maps
static void mergeModuleSpecifierMaps(ImportMap::SpecifierMap& oldMap, const ImportMap::SpecifierMap& newMap, const ImportMap::Reporter& reporter)
{
    // Instead of copying the maps and returning the copy, we're modifying the
    // maps in place.
    // 2. For each specifier → url of newMap:
    for (auto& [specifier, url] : newMap) {
        // 2.2. Set mergedMap[specifier] to url.
        auto iter = oldMap.add(specifier, url);
        // 2.1. If specifier exists in oldMap, then:
        if (!iter.isNewEntry) {
            // 2.1.1. The user agent may report the removed rule as a warning to the
            // developer console.
            reporter.reportWarning(makeString("An import map rule for specifier '"_s, specifier, "' was removed, as it conflicted with an existing rule."_s));
            // 2.1.2. Continue.
            continue;
        }
    }
}

}

ImportMap::ImportMap(SpecifierMap&& imports, ScopesMap&& scopesMap, IntegrityMap&& integrity)
    : m_imports(imports), m_scopesMap(scopesMap), m_integrity(integrity)
{
    // <spec label="sort-and-normalize-scopes" step="3">Return the result of
    // sorting normalized, with an entry a being less than an entry b if b’s key
    // is code unit less than a’s key.</spec>
    ASSERT(m_scopesVector.isEmpty());
    m_scopesVector = copyToVector(m_scopesMap.keys());
    std::ranges::sort(m_scopesVector, [](const auto& a, const auto& b) {
        return codePointCompareLessThan(b.string(), a.string());
    });
}

Expected<URL, String> ImportMap::resolveImportMatch(const AtomString& normalizedSpecifier, const URL& asURL, const SpecifierMap& specifierMap)
{
    // https://html.spec.whatwg.org/C#resolving-an-imports-match

    auto result = specifierMap.find(normalizedSpecifier);

    // 1.1.1. If resolutionResult is null, then throw a TypeError indicating that resolution of specifierKey was blocked by a null entry.
    if (result != specifierMap.end()) {
        if (result->value.isNull())
            return makeUnexpected("specifier is blocked"_s);
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
                return makeUnexpected("specifier is blocked"_s);
            auto afterPrefix = normalizedSpecifier.string().substring(length);
            ASSERT(matched->string().endsWith('/'));
            URL url { matched.value(), afterPrefix };
            if (!url.isValid())
                return makeUnexpected("specifier is blocked"_s);
            if (!url.string().startsWith(matched->string()))
                return makeUnexpected("specifier is blocked"_s);
            return url;
        }
    }

    return { };
}

static URL parseURLLikeModuleSpecifier(const String& specifier, const URL& baseURL)
{
    // https://html.spec.whatwg.org/C#resolving-a-url-like-module-specifier

    if (specifier.startsWith('/') || specifier.startsWith("./"_s) || specifier.startsWith("../"_s))
        return URL(baseURL, specifier);

    return URL { specifier };
}

URL ImportMap::resolve(const String& specifier, const URL& baseURL)
{
    // https://html.spec.whatwg.org/C#resolve-a-module-specifier

    URL asURL = parseURLLikeModuleSpecifier(specifier, baseURL);
    AtomString normalizedSpecifier = AtomString(asURL.isValid() ? asURL.string() : specifier);
    URL resolvedURL;

    dataLogLnIf(ImportMapInternal::verbose, "Resolve ", specifier, " with ", baseURL);
    for (auto& scope : m_scopesVector) {
        dataLogLnIf(ImportMapInternal::verbose, "    Scope ", scope);
        if (scope == baseURL || (scope.string().endsWith('/') && baseURL.string().startsWith(scope.string()))) {
            dataLogLnIf(ImportMapInternal::verbose, "        Matching");
            auto result = resolveImportMatch(normalizedSpecifier, asURL, m_scopesMap.get(scope));
            if (!result)
                return { };
            if (!result.value().isNull())
                resolvedURL = WTFMove(result.value());
        }
    }

    if (resolvedURL.isNull()) {
        dataLogLnIf(ImportMapInternal::verbose, "    Matching with imports");
        auto result = resolveImportMatch(normalizedSpecifier, asURL, m_imports);
        if (!result)
            return { };
        resolvedURL = WTFMove(result.value());
        if (resolvedURL.isNull() && asURL.isValid())
            resolvedURL = WTFMove(asURL);
    }
    if (!resolvedURL.isNull())
        addModuleToResolvedModuleSet(baseURL.string(), normalizedSpecifier);

    return resolvedURL;
}

static String normalizeSpecifierKey(const String& specifierKey, const URL& baseURL, const ImportMap::Reporter& reporter)
{
    // https://html.spec.whatwg.org/C#normalizing-a-specifier-key

    if (specifierKey.isEmpty()) [[unlikely]] {
        reporter.reportWarning("specifier key is empty"_s);
        return nullString();
    }
    URL url = parseURLLikeModuleSpecifier(specifierKey, baseURL);
    if (url.isValid())
        return url.string();
    return specifierKey;
}

static ImportMap::SpecifierMap sortAndNormalizeSpecifierMap(Ref<JSON::Object> importsMap, const URL& baseURL, const ImportMap::Reporter& reporter)
{
    // https://html.spec.whatwg.org/C#sorting-and-normalizing-a-module-specifier-map

    ImportMap::SpecifierMap normalized;
    for (auto& [key, value] : importsMap.get()) {
        AtomString normalizedSpecifierKey = AtomString(normalizeSpecifierKey(key, baseURL, reporter));
        if (normalizedSpecifierKey.isNull())
            continue;
        if (auto valueAsString = value->asString(); !valueAsString.isNull()) [[likely]] {
            URL addressURL = parseURLLikeModuleSpecifier(valueAsString, baseURL);
            if (!addressURL.isValid()) [[unlikely]] {
                reporter.reportWarning(makeString("value in specifier map cannot be parsed as URL "_s, valueAsString));
                normalized.add(normalizedSpecifierKey, URL { });
                continue;
            }
            if (key.endsWith('/') && !addressURL.string().endsWith('/')) [[unlikely]] {
                reporter.reportWarning(makeString("address "_s, addressURL.string(), " does not end with '/' while key "_s, key, " ends with '/'"_s));
                normalized.add(normalizedSpecifierKey, URL { });
                continue;
            }
            normalized.add(normalizedSpecifierKey, WTFMove(addressURL));
        } else {
            reporter.reportWarning("value in specifier map needs to be a string"_s);
            normalized.add(normalizedSpecifierKey, URL { });
            continue;
        }
    }
    return normalized;
}

std::optional<Ref<ImportMap>> ImportMap::parseImportMapString(const SourceCode& sourceCode, const URL& baseURL, const ImportMap::Reporter& reporter)
{
    // https://html.spec.whatwg.org/C#parse-an-import-map-string

    auto result = JSON::Value::parseJSON(sourceCode.view());
    if (!result) {
        reporter.reportError("ImportMap has invalid JSON"_s);
        return std::nullopt;
    }

    auto rootMap = result->asObject();
    if (!rootMap) {
        reporter.reportError("ImportMap is not a map"_s);
        return std::nullopt;
    }

    SpecifierMap normalizedImports;
    if (auto importsMapValue = rootMap->getValue("imports"_s)) {
        auto importsMap = importsMapValue->asObject();
        if (!importsMap) {
            reporter.reportError("Imports is not a map"_s);
            return std::nullopt;
        }

        normalizedImports = sortAndNormalizeSpecifierMap(importsMap.releaseNonNull(), baseURL, reporter);
    }

    ScopesMap scopesMap;
    if (auto scopesMapValue = rootMap->getValue("scopes"_s)) {
        auto scopesMapObject = scopesMapValue->asObject();
        if (!scopesMapObject) {
            reporter.reportError("scopes is not a map"_s);
            return std::nullopt;
        }

        // https://html.spec.whatwg.org/C#sorting-and-normalizing-scopes
        for (auto& [key, value] : *scopesMapObject) {
            auto potentialSpecifierMap = value->asObject();
            if (!potentialSpecifierMap) {
                reporter.reportError("scopes' value is not a map"_s);
                return std::nullopt;
            }
            URL scopePrefixURL { baseURL, key }; // Do not use parseURLLikeModuleSpecifier since we should accept non relative path.
            dataLogLnIf(ImportMapInternal::verbose, "scope key ", key, " and URL ", scopePrefixURL);
            if (!scopePrefixURL.isValid()) [[unlikely]] {
                reporter.reportWarning(makeString("scope key"_s, key, " was not parsable"_s));
                continue;
            }

            scopesMap.set(WTFMove(scopePrefixURL), sortAndNormalizeSpecifierMap(potentialSpecifierMap.releaseNonNull(), baseURL, reporter));
        }
    }

    IntegrityMap integrity;
    StringBuilder errorMessage;
    if (auto integrityValue = rootMap->getValue("integrity"_s)) {
        auto integrityMap = integrityValue->asObject();
        if (!integrityMap) {
            reporter.reportError("integrity is not a map"_s);
            return std::nullopt;
        }

        // https://html.spec.whatwg.org/C#normalizing-a-module-integrity-map
        for (auto& [key, value] : *integrityMap) {
            URL integrityURL = parseURLLikeModuleSpecifier(key, baseURL);
            if (integrityURL.isNull()) [[unlikely]] {
                errorMessage.append("Integrity URL "_s);
                errorMessage.append(key);
                errorMessage.append(" is not a valid absolute URL nor a relative URL starting with '/', './' or '../'\n"_s);
                continue;
            }

            auto valueAsString = value->asString();
            if (valueAsString.isNull()) [[unlikely]] {
                errorMessage.append("Integrity value of "_s);
                errorMessage.append(key);
                errorMessage.append(" is not a string\n"_s);
                continue;
            }

            integrity.set(integrityURL, valueAsString);
        }
    }

    if (!errorMessage.isEmpty())
        reporter.reportError(errorMessage.toString());

    return adoptRef(*new ImportMap(WTFMove(normalizedImports), WTFMove(scopesMap), WTFMove(integrity)));
}

String ImportMap::integrityForURL(const URL& url) const
{
    return url.isNull() ? String() : m_integrity.get(url);
}

// https://html.spec.whatwg.org/C/#merge-existing-and-new-import-maps
void ImportMap::mergeExistingAndNewImportMaps(Ref<ImportMap>&& newImportMap, const ImportMap::Reporter& reporter)
{
    // 1. Let newImportMapScopes be a deep copy of newImportMap's scopes.
    // 2. Let newImportMapImports be a deep copy of newImportMap's imports.
    //
    // Instead of copying we have moved the newImportMap here and are performing
    // the algorithm's mutations directly on them. That's fine because the move
    // guarantees that no one will use this map for anything else.
    ImportMap::ScopesMap& newImportMapScopes = newImportMap->m_scopesMap;
    ImportMap::SpecifierMap& newImportMapImports = newImportMap->m_imports;
    ImportMap::IntegrityMap& newImportMapIntegrity = newImportMap->m_integrity;

    // 3. For each scopePrefix → scopeImports of newImportMapScopes:
    for (auto& scope : newImportMapScopes) {
        auto scopeImports = WTFMove(scope.value);
        // 3.1. For each pair of global's resolved module set:
        //
        // 3.1.1. If pair's referring script does not start with scopePrefix,
        // continue.
        //
        // 3.1.2. For each specifier → url of scopeImports:
        //
        // 3.1.2.1. If pair's specifier starts with specifier, then:
        //
        //
        // We are using a different algorithm here, where instead of a resolved
        // module set, we have a scoped resolved module map. The map's keys are
        // scope prefixes, and its values are a set of specifier prefixes that
        // already exist in that scope. We grab the set of specifier prefixes using
        // the current scope and then iterate over the scope's imports, removing any
        // specifiers whose prefix is in the set.

        auto iter = m_scopedResolvedModuleMap.find(AtomString(scope.key.string()));
        if (iter != m_scopedResolvedModuleMap.end()) {
            auto& currentResolvedSet = iter->value;
            scopeImports.removeIf([&](const auto& pair) {
                if (currentResolvedSet.find(pair.key) != currentResolvedSet.end()) {
                    reporter.reportWarning(makeString("An import map scope rule for specifier '"_s, pair.key, "' was removed, as it conflicted with already resolved module specifiers."_s));
                    // 3.1.2.1.1. The user agent may report the removed rule as a warning to
                    // the developer console.
                    // 3.1.2.1.2. Remove scopeImports[specifier].
                    return true;
                }
                return false;
            });
        }

        // 3.2 If scopePrefix exists in oldImportMap's scopes, then set
        // oldImportMap's scopes[scopePrefix] to the result of merging module
        // specifier maps, given scopeImports and oldImportMap's
        // scopes[scopePrefix].
        const auto oldScopeSpecifierMapIt = m_scopesMap.find(scope.key);
        if (oldScopeSpecifierMapIt != m_scopesMap.end()) {
            ImportMap::SpecifierMap& oldScopeSpecifierMap = oldScopeSpecifierMapIt->value;
            ImportMapInternal::mergeModuleSpecifierMaps(oldScopeSpecifierMap, scopeImports, reporter);
        } else {
            // 3.3 Otherwise, set oldImportMap's scopes[scopePrefix] to
            // scopeImports.
            m_scopesMap.set(scope.key, WTFMove(scopeImports));
            m_scopesVector.append(scope.key);
        }
    }

    // 4. For each url → integrity of newImportMap's integrity:
    for (auto& url : newImportMapIntegrity.keys()) {
        const auto& newIntegrityValue = newImportMapIntegrity.get(url);
        // 4.2 Set oldImportMap's integrity[url] to integrity.
        auto iter = m_integrity.add(url, newIntegrityValue);
        // 4.1 If url exists in oldImportMap's integrity, then:
        if (!iter.isNewEntry) {
            // 4.1.1. The user agent may report the removed rule as a warning to the
            // developer console.
            reporter.reportWarning(makeString("An import map integrity rule for url '"_s, url.string(), "' was removed, as it conflicted with already defined integrity rules."_s));
            // 4.1.2 Continue.
            continue;
        }
    }
    // 5. For each pair of global's resolved module set:

    // 5.1. For each specifier → url of newImportMapImports:

    // 5.1.1. If specifier starts with pair's specifier, then:

    // We're using a different algorithm here where the resolved module set is
    // replaced with a set of all the prefixes of specifier resolved. For each
    // such prefix that exists in the new import map's imports section, we remove
    // it from that section.
    for (auto& specifier : m_toplevelResolvedModuleSet) {
        auto iter = newImportMapImports.find(specifier);
        if (iter == newImportMapImports.end())
            continue;
        // 5.1. The user agent may report the removed rule as a warning to the
        // developer console.
        reporter.reportWarning(makeString("An import map rule for specifier '"_s, specifier, "' was removed, as it conflicted with already resolved module specifiers."_s));
        // 5.2. Remove newImportMapImports[specifier].
        newImportMapImports.remove(iter);
    }
    // 6. Set oldImportMap's imports to the result of merge module specifier
    // maps, given newImportMapImports and oldImportMap's imports.
    ImportMapInternal::mergeModuleSpecifierMaps(m_imports, newImportMapImports, reporter);
}

static Vector<AtomString> findURLPrefixes(String specifier)
{
    constexpr size_t capacity = 6;
    Vector<size_t, capacity> positions;
    constexpr char slash = '/';
    size_t position = specifier.find(slash);

    while (position != notFound) {
        positions.append(++position);
        position = specifier.find(slash, position);
    }

    Vector<AtomString> result;
    result.reserveInitialCapacity(positions.size());
    for (size_t& pos : positions)
        result.append(AtomString(specifier.substring(0, pos)));

    return result;
}

// https://html.spec.whatwg.org/C#add-module-to-resolved-module-set
void ImportMap::addModuleToResolvedModuleSet(String referringScriptURL, AtomString specifier)
{
    // 1. Let global be settingsObject's global object.

    // 2. If global does not implement Window, then return.

    // 3. Let pair be a new referring script specifier pair, with referring script
    // set to referringScriptURL, and specifier set to specifier.

    // 4. Append pair to global's resolved module set.

    // We're using a different algorithm here where we find all the prefixes the
    // specifier has and add them to the top_level_resolved_module_set. We then
    // find all the prefixes that the referring script URL has, and add all the
    // prefixes to the sets of these referring prefixes in the
    // scoped_resolved_module_map.
    m_toplevelResolvedModuleSet.add(specifier);
    Vector<AtomString> specifierPrefixes = findURLPrefixes(specifier);
    for (auto& specifierPrefix : specifierPrefixes)
        m_toplevelResolvedModuleSet.add(specifierPrefix);

    Vector<AtomString> referringScriptPrefixes = findURLPrefixes(referringScriptURL);
    for (AtomString& referringScriptPrefix : referringScriptPrefixes) {
        const auto& currentSetIt = m_scopedResolvedModuleMap.find(referringScriptPrefix);
        UncheckedKeyHashSet<AtomString>* currentSet = nullptr;
        if (currentSetIt != m_scopedResolvedModuleMap.end())
            currentSet = &currentSetIt->value;
        else
            currentSet = &(m_scopedResolvedModuleMap.set(referringScriptPrefix, UncheckedKeyHashSet<AtomString>()).iterator->value);

        currentSet->add(specifier);
        for (AtomString& specifierPrefix : specifierPrefixes)
            currentSet->add(specifierPrefix);
    }
}

} // namespace JSC

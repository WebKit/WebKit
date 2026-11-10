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

std::expected<URL, RefPtr<StringImpl>> ImportMap::resolveImportMatch(AtomStringImpl* normalizedSpecifier, const URL& asURL, const SpecifierMap& specifierMap)
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
            if (endsWith(key, '/')) {
                auto position = find(normalizedSpecifier, key);
                if (position == 0) {
                    if (WTF::length(key) > length) {
                        matched = value;
                        length = WTF::length(key);
                    }
                }
            }
        }

        if (matched) {
            if (matched->isNull())
                return makeUnexpected("specifier is blocked"_s);
            auto afterPrefix = substring(normalizedSpecifier, length);
            ASSERT(endsWith(matched->string(), '/'));
            URL url { matched.value(), afterPrefix };
            if (!url.isValid())
                return makeUnexpected("specifier is blocked"_s);
            if (!startsWith(url.string(), matched->string()))
                return makeUnexpected("specifier is blocked"_s);
            return url;
        }
    }

    return { };
}

static URL parseURLLikeModuleSpecifier(StringImpl* specifier, const URL& baseURL)
{
    // https://html.spec.whatwg.org/C#resolving-a-url-like-module-specifier

    if (startsWith(specifier, '/') || startsWith(specifier, "./"_s) || startsWith(specifier, "../"_s))
        return URL(baseURL, specifier);

    return URL { specifier };
}

URL ImportMap::resolve(StringImpl* specifier, const URL& baseURL)
{
    // https://html.spec.whatwg.org/C#resolve-a-module-specifier

    URL asURL = parseURLLikeModuleSpecifier(specifier, baseURL);
    RefPtr<AtomStringImpl> normalizedSpecifier = AtomStringImpl::add(asURL.isValid() ? asURL.string().get() : specifier);

    URL resolvedURL;

    dataLogLnIf(ImportMapInternal::verbose, "Resolve ", specifier, " with ", baseURL);
    for (auto& scope : m_scopesVector) {
        dataLogLnIf(ImportMapInternal::verbose, "    Scope ", scope);
        if (scope == baseURL || (endsWith(scope.string(), '/') && startsWith(baseURL.string(), scope.string()))) {
            dataLogLnIf(ImportMapInternal::verbose, "        Matching");
            auto result = resolveImportMatch(normalizedSpecifier, asURL, m_scopesMap.get(scope));
            if (!result)
                return { };
            if (!result.value().isNull()) {
                resolvedURL = WTF::move(result.value());
                break;
            }
        }
    }

    if (resolvedURL.isNull()) {
        dataLogLnIf(ImportMapInternal::verbose, "    Matching with imports");
        auto result = resolveImportMatch(normalizedSpecifier, asURL, m_imports);
        if (!result)
            return { };
        resolvedURL = WTF::move(result.value());
        if (resolvedURL.isNull() && asURL.isValid())
            resolvedURL = WTF::move(asURL);
    }
    if (!resolvedURL.isNull()) {
        addModuleToResolvedModuleSet(baseURL.string(), normalizedSpecifier);
        dataLogLnIf(ImportMapInternal::verbose, "    ", resolvedURL);
    }

    return resolvedURL;
}

static RefPtr<StringImpl> normalizeSpecifierKey(StringImpl* specifierKey, const URL& baseURL, const ImportMap::Reporter& reporter)
{
    // https://html.spec.whatwg.org/C#normalizing-a-specifier-key

    if (isEmpty(specifierKey)) [[unlikely]] {
        reporter.reportWarning("specifier key is empty"_si);
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
    for (auto& key : importsMap->keys()) {
        auto value = importsMap->getValue(key);
        RefPtr<AtomStringImpl> normalizedSpecifierKey = AtomStringImpl::add(normalizeSpecifierKey(key, baseURL, reporter));
        if (!normalizedSpecifierKey)
            continue;
        if (auto valueAsString = value->asString(); valueAsString) [[likely]] {
            URL addressURL = parseURLLikeModuleSpecifier(valueAsString, baseURL);
            if (!addressURL.isValid()) [[unlikely]] {
                reporter.reportWarning(makeString("value in specifier map cannot be parsed as URL "_s, valueAsString));
                normalized.set(normalizedSpecifierKey, URL { });
                continue;
            }
            if (endsWith(key, '/') && !endsWith(addressURL.string(), '/')) [[unlikely]] {
                reporter.reportWarning(makeString("address "_s, addressURL.string(), " does not end with '/' while key "_s, key, " ends with '/'"_s));
                normalized.set(normalizedSpecifierKey, URL { });
                continue;
            }
            normalized.set(normalizedSpecifierKey, WTF::move(addressURL));
        } else {
            reporter.reportWarning("value in specifier map needs to be a string"_si);
            normalized.set(normalizedSpecifierKey, URL { });
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
        reporter.reportError("ImportMap has invalid JSON"_si);
        return std::nullopt;
    }

    auto rootMap = result->asObject();
    if (!rootMap) {
        reporter.reportError("ImportMap is not a map"_si);
        return std::nullopt;
    }

    SpecifierMap normalizedImports;
    if (auto importsMapValue = rootMap->getValue("imports"_si)) {
        auto importsMap = importsMapValue->asObject();
        if (!importsMap) {
            reporter.reportError("Imports is not a map"_si);
            return std::nullopt;
        }

        normalizedImports = sortAndNormalizeSpecifierMap(importsMap.releaseNonNull(), baseURL, reporter);
    }

    ScopesMap scopesMap;
    if (auto scopesMapValue = rootMap->getValue("scopes"_si)) {
        auto scopesMapObject = scopesMapValue->asObject();
        if (!scopesMapObject) {
            reporter.reportError("scopes is not a map"_si);
            return std::nullopt;
        }

        // https://html.spec.whatwg.org/C#sorting-and-normalizing-scopes
        for (auto& key : scopesMapObject->keys()) {
            auto value = scopesMapObject->getValue(key);
            auto potentialSpecifierMap = value->asObject();
            if (!potentialSpecifierMap) {
                reporter.reportError("scopes' value is not a map"_si);
                return std::nullopt;
            }
            URL scopePrefixURL { baseURL, key }; // Do not use parseURLLikeModuleSpecifier since we should accept non relative path.
            dataLogLnIf(ImportMapInternal::verbose, "scope key ", key, " and URL ", scopePrefixURL);
            if (!scopePrefixURL.isValid()) [[unlikely]] {
                reporter.reportWarning(makeString("scope key"_s, key, " was not parsable"_s));
                continue;
            }

            scopesMap.set(WTF::move(scopePrefixURL), sortAndNormalizeSpecifierMap(potentialSpecifierMap.releaseNonNull(), baseURL, reporter));
        }
    }

    IntegrityMap integrity;
    StringBuilder errorMessage;
    if (auto integrityValue = rootMap->getValue("integrity"_si)) {
        auto integrityMap = integrityValue->asObject();
        if (!integrityMap) {
            reporter.reportError("integrity is not a map"_si);
            return std::nullopt;
        }

        // https://html.spec.whatwg.org/C#normalizing-a-module-integrity-map
        for (auto& key : integrityMap->keys()) {
            auto value = integrityMap->getValue(key);
            URL integrityURL = parseURLLikeModuleSpecifier(key, baseURL);
            if (integrityURL.isNull()) [[unlikely]] {
                errorMessage.append("Integrity URL "_s);
                errorMessage.append(key);
                errorMessage.append(" is not a valid absolute URL nor a relative URL starting with '/', './' or '../'\n"_s);
                continue;
            }

            auto valueAsString = value->asString();
            if (!valueAsString) [[unlikely]] {
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

    return adoptRef(*new ImportMap(WTF::move(normalizedImports), WTF::move(scopesMap), WTF::move(integrity)));
}

RefPtr<StringImpl> ImportMap::integrityForURL(const URL& url) const
{
    return url.isNull() ? nullptr : m_integrity.get(url);
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
        auto scopeImports = WTF::move(scope.value);
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

        auto iter = m_scopedResolvedModuleMap.find(AtomStringImpl::add(scope.key.string()).get());
        if (iter != m_scopedResolvedModuleMap.end()) {
            auto& currentResolvedSet = iter->value;
            scopeImports.removeIf([&](const auto& pair) {
                if (currentResolvedSet.find(pair.key) != currentResolvedSet.end()) {
                    auto warning = makeString("An import map scope rule for specifier '"_s, pair.key, "' was removed, as it conflicted with already resolved module specifiers."_s);
                    reporter.reportWarning(warning);
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
            m_scopesMap.set(scope.key, WTF::move(scopeImports));
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

static Vector<RefPtr<AtomStringImpl>> findURLPrefixes(RefPtr<StringImpl> specifier)
{
    constexpr size_t capacity = 6;
    Vector<size_t, capacity> positions;
    constexpr char slash = '/';
    size_t position = find(specifier, slash);

    while (position != notFound) {
        positions.append(++position);
        position = find(specifier, slash, position);
    }

    Vector<RefPtr<AtomStringImpl>> result(ReserveCapacityTag { }, positions.size());
    for (size_t& pos : positions)
        result.append(AtomStringImpl::add(substring(specifier, 0, pos)));

    return result;
}

// https://html.spec.whatwg.org/C#add-module-to-resolved-module-set
void ImportMap::addModuleToResolvedModuleSet(RefPtr<StringImpl> referringScriptURL, RefPtr<AtomStringImpl> specifier)
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
    Vector<RefPtr<AtomStringImpl>> specifierPrefixes = findURLPrefixes(specifier);
    for (auto& specifierPrefix : specifierPrefixes)
        m_toplevelResolvedModuleSet.add(specifierPrefix);

    Vector<RefPtr<AtomStringImpl>> referringScriptPrefixes = findURLPrefixes(referringScriptURL);
    for (RefPtr<AtomStringImpl>& referringScriptPrefix : referringScriptPrefixes) {
        const auto& currentSetIt = m_scopedResolvedModuleMap.find(referringScriptPrefix);
        UncheckedKeyHashSet<RefPtr<AtomStringImpl>>* currentSet = nullptr;
        if (currentSetIt != m_scopedResolvedModuleMap.end())
            currentSet = &currentSetIt->value;
        else
            currentSet = &(m_scopedResolvedModuleMap.set(referringScriptPrefix, UncheckedKeyHashSet<RefPtr<AtomStringImpl>>()).iterator->value);

        currentSet->add(specifier);
        for (RefPtr<AtomStringImpl>& specifierPrefix : specifierPrefixes)
            currentSet->add(specifierPrefix);
    }
}

} // namespace JSC

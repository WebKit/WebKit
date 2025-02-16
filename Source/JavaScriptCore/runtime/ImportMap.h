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

#pragma once

#include <wtf/Expected.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>

namespace JSC {

class SourceCode;

class ImportMap final : public RefCounted<ImportMap> {
public:
    using SpecifierMap = UncheckedKeyHashMap<AtomString, URL>;
    using ScopesMap = UncheckedKeyHashMap<URL, SpecifierMap>;
    using ScopesVector = Vector<URL>;
    using IntegrityMap = UncheckedKeyHashMap<URL, String>;

    class Reporter {
    public:
        virtual ~Reporter() = default;
        virtual void reportWarning(const String&) const { };
        virtual void reportError(const String&) const { };
    };

    static Ref<ImportMap> create() { return adoptRef(*new ImportMap()); }

    JS_EXPORT_PRIVATE URL resolve(const String& specifier, const URL& baseURL);

    JS_EXPORT_PRIVATE String integrityForURL(const URL&) const;

    // https://html.spec.whatwg.org/C#parse-an-import-map-string
    JS_EXPORT_PRIVATE static std::optional<Ref<ImportMap>> parseImportMapString(const SourceCode&, const URL& baseURL, const ImportMap::Reporter&);

    // https://html.spec.whatwg.org/C/#merge-existing-and-new-import-maps
    // `newImportMap` is modified in place here, and should not be used after
    // this call.
    JS_EXPORT_PRIVATE void mergeExistingAndNewImportMaps(Ref<ImportMap>&& newImportMap, const ImportMap::Reporter&);

    void addModuleToResolvedModuleSet(String referringScriptURL, AtomString specifier);
private:
    ImportMap() = default;
    ImportMap(SpecifierMap&&, ScopesMap&&, IntegrityMap&&);

    static Expected<URL, String> resolveImportMatch(const AtomString&, const URL&, const SpecifierMap&);

    SpecifierMap m_imports;
    ScopesMap m_scopesMap;
    ScopesVector m_scopesVector;
    IntegrityMap m_integrity;

    // https://html.spec.whatwg.org/C#resolved-module-set
    //
    // We replace the spec's set with two different data structures: a set of all
    // the prefixes resolved at the top-level scope, and a map of scopes to sets
    // of prefixes resolved in them. That permits us to reduce the cost of merging
    // a new map, by performing more work at addModuleToResolvedModuleSet time,
    // and by keeping more prefixes in memory.
    UncheckedKeyHashSet<AtomString> m_toplevelResolvedModuleSet;
    UncheckedKeyHashMap<AtomString, UncheckedKeyHashSet<AtomString>> m_scopedResolvedModuleMap;
};

} // namespace JSC

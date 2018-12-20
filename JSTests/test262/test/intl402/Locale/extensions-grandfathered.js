// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies handling of options with grandfathered tags.
info: |
    ApplyOptionsToTag( tag, options )

    ...
    9. If tag matches neither the privateuse nor the grandfathered production, then
    ...

    ApplyUnicodeExtensionToTag( tag, options, relevantExtensionKeys )

    ...
    2. If tag matches the privateuse or the grandfathered production, then
        a. Let result be a new Record.
        b. Repeat for each element key of relevantExtensionKeys in List order,
            i. Set result.[[<key>]] to undefined.
        c. Set result.[[locale]] to tag.
        d. Return result.
    ...
    7. Repeat for each element key of relevantExtensionKeys in List order,
        e. Let optionsValue be options.[[<key>]].
        f. If optionsValue is not undefined, then
            ii. Let value be optionsValue.
            iv. Else,
                1. Append the Record{[[Key]]: key, [[Value]]: value} to keywords.
    ...

features: [Intl.Locale]
---*/

const testData = [
    // Irregular grandfathered without modern replacement.
    {
        tag: "i-default",
        options: {
            language: "fr",
            script: "Cyrl",
            region: "DE",
            numberingSystem: "latn",
        },
        canonical: "fr-Cyrl-DE-u-nu-latn",
    },

    // Irregular grandfathered with modern replacement.
    {
        tag: "en-gb-oed",
        options: {
            language: "fr",
            script: "Cyrl",
            region: "US",
            numberingSystem: "latn",
        },
        canonical: "fr-Cyrl-US-oxendict-u-nu-latn",
    },

    // Regular grandfathered without modern replacement.
    {
        tag: "cel-gaulish",
        options: {
            language: "fr",
            script: "Cyrl",
            region: "FR",
            numberingSystem: "latn",
        },
        canonical: "fr-Cyrl-FR-u-nu-latn",
    },

    // Regular grandfathered with modern replacement.
    {
        tag: "art-lojban",
        options: {
            language: "fr",
            script: "Cyrl",
            region: "ZZ",
            numberingSystem: "latn",
        },
        canonical: "fr-Cyrl-ZZ-u-nu-latn",
    },
];

for (const {tag, options, canonical} of testData) {
    const loc = new Intl.Locale(tag, options);
    assert.sameValue(loc.toString(), canonical);

    for (const [name, value] of Object.entries(options)) {
        assert.sameValue(loc[name], value);
    }
}

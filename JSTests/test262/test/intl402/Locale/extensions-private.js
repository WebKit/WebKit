// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies handling of options with privateuse tags.
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

const loc = new Intl.Locale("x-default", {
  language: "fr",
  script: "Cyrl",
  region: "DE",
  numberingSystem: "latn",
});
assert.sameValue(loc.toString(), "fr-Cyrl-DE-u-nu-latn");
assert.sameValue(loc.language, "fr");
assert.sameValue(loc.script, "Cyrl");
assert.sameValue(loc.region, "DE");
assert.sameValue(loc.numberingSystem, "latn");

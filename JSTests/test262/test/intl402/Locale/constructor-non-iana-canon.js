// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies canonicalization, minimization and maximization of specific tags.
info: |
    ApplyOptionsToTag( tag, options )
    10. Return CanonicalizeLanguageTag(tag).

    Intl.Locale.prototype.maximize ()
    3. Let maximal be the result of the Add Likely Subtags algorithm applied to loc.[[Locale]].

    Intl.Locale.prototype.minimize ()
    3. Let minimal be the result of the Remove Likely Subtags algorithm applied to loc.[[Locale]].
features: [Intl.Locale]
---*/

// Test some language tags where we know that either CLDR or ICU produce
// different results compared to the canonicalization specified in RFC 5646.
var testData = [
    {
        tag: "mo",
        canonical: "ro",
        maximized: "ro-Latn-RO",
    },
    {
        tag: "es-ES-preeuro",
        maximized: "es-Latn-ES-preeuro",
        minimized: "es-preeuro",
    },
    {
        tag: "uz-UZ-cyrillic",
        maximized: "uz-Latn-UZ-cyrillic",
        minimized: "uz-cyrillic",
    },
    {
        tag: "posix",
    },
    {
        tag: "hi-direct",
        maximized: "hi-Deva-IN-direct",
    },
    {
        tag: "zh-pinyin",
        maximized: "zh-Hans-CN-pinyin",
    },
    {
        tag: "zh-stroke",
        maximized: "zh-Hans-CN-stroke",
    },
    {
        tag: "aar-x-private",
    },
    {
        tag: "heb-x-private",
    },
    {
        tag: "und-ita",
        // canonical: "und" or "ita" ?
        maximized: "en-Latn-US",
        minimized: "und",
    },
    {
        tag: "de-u-kf",
        maximized: "de-Latn-DE-u-kf",
    },
    {
        tag: "ces",
    },
    {
        tag: "hy-arevela",
        canonical: "hy",
        maximized: "hy-Armn-AM",
    },
    {
        tag: "hy-arevmda",
        canonical: "hyw",
    },
];

for (const {tag, canonical = tag, maximized = canonical, minimized = canonical} of testData) {
    assert.sameValue(Intl.getCanonicalLocales(tag)[0], canonical);

    const loc = new Intl.Locale(tag);
    assert.sameValue(loc.toString(), canonical);
    assert.sameValue(loc.maximize().toString(), maximized);
    assert.sameValue(loc.minimize().toString(), minimized);
}

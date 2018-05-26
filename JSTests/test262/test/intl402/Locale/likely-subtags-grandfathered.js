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

const irregularGrandfathered = [
    {
        tag: "en-GB-oed",
        canonical: "en-GB-oxendict",
        maximized: "en-Latn-GB-oxendict",
    },
    {
        tag: "i-ami",
        canonical: "ami",
    },
    {
        tag: "i-bnn",
        canonical: "bnn",
    },
    {
        tag: "i-default",
        canonical: "i-default",
    },
    {
        tag: "i-enochian",
        canonical: "i-enochian",
    },
    {
        tag: "i-hak",
        canonical: "hak",
        maximized: "hak-Hans-CN",
    },
    {
        tag: "i-klingon",
        canonical: "tlh",
    },
    {
        tag: "i-lux",
        canonical: "lb",
        maximized: "lb-Latn-LU",
    },
    {
        tag: "i-mingo",
        canonical: "i-mingo",
    },
    {
        tag: "i-navajo",
        canonical: "nv",
        maximized: "nv-Latn-US",
    },
    {
        tag: "i-pwn",
        canonical: "pwn",
    },
    {
        tag: "i-tao",
        canonical: "tao",
    },
    {
        tag: "i-tay",
        canonical: "tay",
    },
    {
        tag: "i-tsu",
        canonical: "tsu",
    },
    {
        tag: "sgn-BE-FR",
        canonical: "sfb",
    },
    {
        tag: "sgn-BE-NL",
        canonical: "vgt",
    },
    {
        tag: "sgn-CH-DE",
        canonical: "sgg",
    },
];

for (const {tag, canonical, maximized = canonical, minimized = canonical} of irregularGrandfathered) {
    assert.sameValue(Intl.getCanonicalLocales(tag)[0], canonical);

    const loc = new Intl.Locale(tag);
    assert.sameValue(loc.toString(), canonical);

    assert.sameValue(loc.maximize().toString(), maximized);
    assert.sameValue(loc.maximize().maximize().toString(), maximized);

    assert.sameValue(loc.minimize().toString(), minimized);
    assert.sameValue(loc.minimize().minimize().toString(), minimized);

    assert.sameValue(loc.maximize().minimize().toString(), minimized);
    assert.sameValue(loc.minimize().maximize().toString(), maximized);
}

const regularGrandfathered = [
    {
        tag: "art-lojban",
        canonical: "jbo",
        maximized: "jbo-Latn-001",
    },
    {
        tag: "cel-gaulish",
        canonical: "cel-gaulish",
    },
    {
        tag: "no-bok",
        canonical: "nb",
        maximized: "nb-Latn-NO",
    },
    {
        tag: "no-nyn",
        canonical: "nn",
        maximized: "nn-Latn-NO",
    },
    {
        tag: "zh-guoyu",
        canonical: "cmn",
    },
    {
        tag: "zh-hakka",
        canonical: "hak",
        maximized: "hak-Hans-CN",
    },
    {
        tag: "zh-min",
        canonical: "zh-min",
    },
    {
        tag: "zh-min-nan",
        canonical: "nan",
        maximized: "nan-Hans-CN",
    },
    {
        tag: "zh-xiang",
        canonical: "hsn",
        maximized: "hsn-Hans-CN",
    },
];

for (const {tag, canonical, maximized = canonical, minimized = canonical} of regularGrandfathered) {
    assert.sameValue(Intl.getCanonicalLocales(tag)[0], canonical);

    const loc = new Intl.Locale(tag);
    assert.sameValue(loc.toString(), canonical);

    assert.sameValue(loc.maximize().toString(), maximized);
    assert.sameValue(loc.maximize().maximize().toString(), maximized);

    assert.sameValue(loc.minimize().toString(), minimized);
    assert.sameValue(loc.minimize().minimize().toString(), minimized);

    assert.sameValue(loc.maximize().minimize().toString(), minimized);
    assert.sameValue(loc.minimize().maximize().toString(), maximized);
}

// Add constiants, extensions, and privateuse subtags to regular grandfathered
// language tags and ensure it produces the "expected" result.
const extras = [
    "fonipa",
    "a-not-assigned",
    "u-attr",
    "u-co",
    "u-co-phonebk",
    "x-private",
];

for (const {tag} of regularGrandfathered) {
    const priv = "-x-0";
    const tagMax = new Intl.Locale(tag + priv).maximize().toString().slice(0, -priv.length);
    const tagMin = new Intl.Locale(tag + priv).minimize().toString().slice(0, -priv.length);

    for (const extra of extras) {
        const loc = new Intl.Locale(tag + "-" + extra);
        assert.sameValue(loc.toString(), tag + "-" + extra);

        assert.sameValue(loc.maximize().toString(), tagMax + "-" + extra);
        assert.sameValue(loc.maximize().maximize().toString(), tagMax + "-" + extra);

        assert.sameValue(loc.minimize().toString(), tagMin + "-" + extra);
        assert.sameValue(loc.minimize().minimize().toString(), tagMin + "-" + extra);

        assert.sameValue(loc.maximize().minimize().toString(), tagMin + "-" + extra);
        assert.sameValue(loc.minimize().maximize().toString(), tagMax + "-" + extra);
    }
}


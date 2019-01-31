// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies canonicalization, minimization and maximization of specific tags.
info: |
    ApplyOptionsToTag( tag, options )

    2. If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError exception.

    9. Set tag to CanonicalizeLanguageTag(tag).

    CanonicalizeLanguageTag( tag )

    The CanonicalizeLanguageTag abstract operation returns the canonical and
    case-regularized form of the locale argument (which must be a String value
    that is a structurally valid Unicode BCP 47 Locale Identifier as verified by
    the IsStructurallyValidLanguageTag abstract operation).

    IsStructurallyValidLanguageTag ( locale )

    The IsStructurallyValidLanguageTag abstract operation verifies that the
    locale argument (which must be a String value)

    represents a well-formed Unicode BCP 47 Locale Identifier" as specified in
    Unicode Technical Standard 35 section 3.2, or successor,


    Intl.Locale.prototype.maximize ()
    3. Let maximal be the result of the Add Likely Subtags algorithm applied to loc.[[Locale]].

    Intl.Locale.prototype.minimize ()
    3. Let minimal be the result of the Remove Likely Subtags algorithm applied to loc.[[Locale]].
features: [Intl.Locale]
---*/

const irregularGrandfathered = [
    "en-GB-oed",
    "i-ami",
    "i-bnn",
    "i-default",
    "i-enochian",
    "i-hak",
    "i-klingon",
    "i-lux",
    "i-mingo",
    "i-navajo",
    "i-pwn",
    "i-tao",
    "i-tay",
    "i-tsu",
    "sgn-BE-FR",
    "sgn-BE-NL",
    "sgn-CH-DE",
];

for (const tag of irregularGrandfathered) {
    assert.throws(RangeError, () => new Intl.Locale(tag));
}

const regularGrandfathered = [
    {
        tag: "art-lojban",
        canonical: "jbo",
        maximized: "jbo-Latn-001",
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

const regularGrandfatheredWithExtLang = [
    "no-bok",
    "no-nyn",
    "zh-min",
    "zh-min-nan",
];

for (const tag of regularGrandfatheredWithExtLang) {
    assert.throws(RangeError, () => new Intl.Locale(tag));
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


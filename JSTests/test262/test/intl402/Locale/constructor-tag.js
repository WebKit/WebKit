// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verifies canonicalization of specific tags.
info: |
    ApplyOptionsToTag( tag, options )
    10. Return CanonicalizeLanguageTag(tag).
features: [Intl.Locale]
---*/

const validLanguageTags = {
    "eN": "en",
    "en-gb": "en-GB",
    "IT-LATN-iT": "it-Latn-IT",
    "ar-ma-u-ca-islamicc": "ar-MA-u-ca-islamicc",
    "th-th-u-nu-thai": "th-TH-u-nu-thai",
    "X-u-foo": "x-u-foo",
    "en-x-u-foo": "en-x-u-foo",
    "en-a-bar-x-u-foo": "en-a-bar-x-u-foo",
    "en-x-u-foo-a-bar": "en-x-u-foo-a-bar",
    "en-u-baz-a-bar-x-u-foo": "en-a-bar-u-baz-x-u-foo",
    "Flob": "flob",
    "ZORK": "zork",
    "Blah-latn": "blah-Latn",
    "QuuX-latn-us": "quux-Latn-US",
    "SPAM-gb-x-Sausages-BACON-eggs": "spam-GB-x-sausages-bacon-eggs",
    "DE-1996": "de-1996",
    "sl-ROZAJ-BISKE-1994": "sl-rozaj-biske-1994",
    "zh-latn-pinyin-pinyin2": "zh-Latn-pinyin-pinyin2",
};

for (const [langtag, canonical] of Object.entries(validLanguageTags)) {
    assert.sameValue(new Intl.Locale(canonical).toString(), canonical,
                     `"${canonical}" should pass through unchanged`);
    assert.sameValue(new Intl.Locale(langtag).toString(), canonical,
                     `"${langtag}" should be canonicalized to "${canonical}`);
}

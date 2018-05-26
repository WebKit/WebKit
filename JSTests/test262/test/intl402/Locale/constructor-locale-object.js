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

// Pass Intl.Locale object and replace subtag.
const enUS = new Intl.Locale("en-US");
const enGB = new Intl.Locale(enUS, {region: "GB"});

assert.sameValue(enUS.toString(), "en-US");
assert.sameValue(enGB.toString(), "en-GB");

// Pass Intl.Locale object and replace Unicode extension keyword.
const zhUnihan = new Intl.Locale("zh-u-co-unihan");
const zhZhuyin = new Intl.Locale(zhUnihan, {collation: "zhuyin"});

assert.sameValue(zhUnihan.toString(), "zh-u-co-unihan");
assert.sameValue(zhZhuyin.toString(), "zh-u-co-zhuyin");

assert.sameValue(zhUnihan.collation, "unihan");
assert.sameValue(zhZhuyin.collation, "zhuyin");

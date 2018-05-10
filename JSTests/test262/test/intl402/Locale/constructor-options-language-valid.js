// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Checks error cases for the options argument to the Locale
    constructor.
info: |
    Intl.Locale( tag [, options] )
    10. If options is undefined, then
    11. Else
        a. Let options be ? ToObject(options).
    12. Set tag to ? ApplyOptionsToTag(tag, options).

    ApplyOptionsToTag( tag, options )
    ...
    9. If tag matches the langtag production, then
        a. If language is not undefined, then
            i. Set tag to tag with the substring corresponding to the language production replaced by the string language.

features: [Intl.Locale]
---*/

const validLanguageOptions = [
  [undefined, "en"],
  [null, "null"],
  ["zh-cmn", "cmn"],
  ["ZH-CMN", "cmn"],
  ["abcd", "abcd"],
  ["abcde", "abcde"],
  ["abcdef", "abcdef"],
  ["abcdefg", "abcdefg"],
  ["abcdefgh", "abcdefgh"],
  [{ toString() { return "de" } }, "de"],
];
for (const [language, expected] of validLanguageOptions) {
  let options = { language };
  assert.sameValue(
    new Intl.Locale('en', options).toString(),
    expected,
    `new Intl.Locale('en', options).toString() equals the value of ${expected}`
  );
}

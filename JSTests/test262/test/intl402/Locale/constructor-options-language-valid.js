// Copyright 2018 André Bargull; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verify valid language option values (various)
info: |
    Intl.Locale( tag [, options] )
    10. If options is undefined, then
    11. Else
        a. Let options be ? ToObject(options).
    12. Set tag to ? ApplyOptionsToTag(tag, options).

    ApplyOptionsToTag( tag, options )
    ...
    9. If tag matches neither the privateuse nor the grandfathered production, then
        b. If language is not undefined, then
            i. Set tag to tag with the substring corresponding to the language production replaced by the string language.

features: [Intl.Locale]
---*/

const validLanguageOptions = [
  [null, 'null'],
  ['zh-cmn', 'cmn'],
  ['ZH-CMN', 'cmn'],
  ['abcd', 'abcd'],
  [{ toString() { return 'de' } }, 'de'],
];
for (const [language, expected] of validLanguageOptions) {
  let expect = expected || 'en';

  assert.sameValue(
    new Intl.Locale('en', {language}).toString(),
    expect,
    `new Intl.Locale('en', {language: "${language}"}).toString() returns "${expect}"`
  );

  expect = (expected || 'en') + '-US';
  assert.sameValue(
    new Intl.Locale('en-US', {language}).toString(),
    expect,
    `new Intl.Locale('en-US', {language: "${language}"}).toString() returns "${expect}"`
  );

  expect = expected || 'en-els';
  assert.sameValue(
    new Intl.Locale('en-els', {language}).toString(),
    expect,
    `new Intl.Locale('en-els', {language: "${language}"}).toString() returns "${expect}"`
  );
}

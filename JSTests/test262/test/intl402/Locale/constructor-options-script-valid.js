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
    5. Let script be ? GetOption(options, "script", "string", undefined, undefined).
    ...
    9. If tag matches neither the privateuse nor the grandfathered production, then
      ...
      c. If script is not undefined, then
        i. If tag does not contain a script production, then
          1. Set tag to the concatenation of the language production of tag, "-", script, and the rest of tag.
        ii. Else,
          1. Set tag to tag with the substring corresponding to the script production replaced by the string script.


features: [Intl.Locale]
---*/

const validScriptOptions = [
  [undefined, undefined],
  [null, 'Null'],
  ['bali', 'Bali'],
  ['Bali', 'Bali'],
  ['bALI', 'BALI'], // TODO REVIEW: is this the correct case regularization?
  [{ toString() { return 'Brai' } }, 'Brai'],
];
for (const [script, expected] of validScriptOptions) {
  let options = { script };
  let expect = expected ? 'en-' + expected : 'en';

  assert.sameValue(
    new Intl.Locale('en', options).toString(),
    expect,
    `new Intl.Locale('en', options).toString() equals the value of ${expect}`
  );

  expect = (expected ? ('en-' + expected) : 'en') + '-DK';
  assert.sameValue(
    new Intl.Locale('en-DK', options).toString(),
    expect,
    `new Intl.Locale('en', options).toString() equals the value of ${expect}`
  );

  expect = expected ? ('en-' + expected) : 'en-Cyrl';
  assert.sameValue(
    new Intl.Locale('en-Cyrl', options).toString(),
    expect,
    `new Intl.Locale('en-Cyrl', options).toString() equals the value of ${expect}`
  );
}

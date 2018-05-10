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
    7. Let region be ? GetOption(options, "region", "string", undefined, undefined).
    ...
    9. If tag matches the langtag production, then
      ...
      c. If region is not undefined, then
        i. If tag does not contain a region production, then
          1. Set tag to the concatenation of the language production of tag, the substring corresponding to the "-" script production if present, "-", region, and the rest of tag.

features: [Intl.Locale]
---*/

const validRegionOptions = [
  ["FR", "en-FR"],
  ["554", "en-554"],
  [554, "en-554"],
];
for (const [region, expected] of validRegionOptions) {
  let options = { region };
  assert.sameValue(
    new Intl.Locale('en', options).toString(),
    expected,
    `new Intl.Locale('en', options).toString() equals the value of ${expected}`
  );
  assert.sameValue(
    new Intl.Locale('en-US', options).toString(),
    expected,
    `new Intl.Locale('en-US', options).toString() equals the value of ${expected}`
  );
}

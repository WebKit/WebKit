// Copyright 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Verify valid language option values (undefined)
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

assert.sameValue(
  new Intl.Locale('en', {language: undefined}).toString(),
  'en',
  `new Intl.Locale('en', {language: undefined}).toString() returns "en"`
);

assert.sameValue(
  new Intl.Locale('en-US', {language: undefined}).toString(),
  'en-US',
  `new Intl.Locale('en-US', {language: undefined}).toString() returns "en-US"`
);

assert.sameValue(
  new Intl.Locale('en-els', {language: undefined}).toString(),
  'en-els',
  `new Intl.Locale('en-els', {language: undefined}).toString() returns "en-els"`
);

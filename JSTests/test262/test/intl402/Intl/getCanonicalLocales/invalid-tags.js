// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.getcanonicallocales
description: >
  Throws a RangeError if the language tag is invalid.
info: |
  8.2.1 Intl.getCanonicalLocales (locales)
    1. Let ll be ? CanonicalizeLocaleList(locales).
    ...

  9.2.1 CanonicalizeLocaleList (locales)
    ...
    7. Repeat, while k < len
      ...
      c. If kPresent is true, then
        ...
        iv. If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError exception.
        ...
includes: [testIntl.js]
---*/

var invalidLanguageTags = [
  "", // empty tag
  "i", // singleton alone
  "x", // private use without subtag
  "u", // extension singleton in first place
  "419", // region code in first place
  "u-nu-latn-cu-bob", // extension sequence without language
  "hans-cmn-cn", // "hans" could theoretically be a 4-letter language code,
                 // but those can't be followed by extlang codes.
  "cmn-hans-cn-u-u", // duplicate singleton
  "cmn-hans-cn-t-u-ca-u", // duplicate singleton
  "de-gregory-gregory", // duplicate variant
  "*", // language range
  "de-*", // language range
  "中文", // non-ASCII letters
  "en-ß", // non-ASCII letters
  "ıd", // non-ASCII letters

  // underscores in different parts of the language tag
  "de_DE",
  "DE_de",
  "cmn_Hans",
  "cmn-hans_cn",
  "es_419",
  "es-419-u-nu-latn-cu_bob",
  "i_klingon",
  "cmn-hans-cn-t-ca-u-ca-x_t-u",
  "enochian_enochian",
  "de-gregory_u-ca-gregory",

  "en\u0000", // null-terminator sequence
  " en", // leading whitespace
  "en ", // trailing whitespace
  "it-IT-Latn", // country before script tag
  "de-u", // incomplete Unicode extension sequences
  "de-u-",
  "de-u-ca-",
  "de-u-ca-gregory-",
  "si-x", // incomplete private-use tags
  "x-",
  "x-y-",
];

// make sure the data above is correct
for (var i = 0; i < invalidLanguageTags.length; ++i) {
  var invalidTag = invalidLanguageTags[i];
  assert(
    !isCanonicalizedStructurallyValidLanguageTag(invalidTag),
    "Test data \"" + invalidTag + "\" is a canonicalized and structurally valid language tag."
  );
}

for (var i = 0; i < invalidLanguageTags.length; ++i) {
  var invalidTag = invalidLanguageTags[i];
  assert.throws(RangeError, function() {
    Intl.getCanonicalLocales(invalidTag)
  }, "Language tag: " + invalidTag);
}

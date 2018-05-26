// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Checks error cases for the options argument to the Locale constructor.
info: |
    Intl.Locale( tag [, options] )

    ...
    28. If numberingSystem is not undefined, then
        a. If numberingSystem does not match the [(3*8alphanum) *("-" (3*8alphanum))] sequence, throw a RangeError exception.

features: [Intl.Locale]
---*/


/*
 alphanum = (ALPHA / DIGIT)     ; letters and numbers
 numberingSystem = [(3*8alphanum) *("-" (3*8alphanum))]
*/
const invalidNumberingSystemOptions = [
  "a",
  "ab",
  "abcdefghi",
  "abc-abcdefghi",
];
for (const invalidNumberingSystemOption of invalidNumberingSystemOptions) {
  assert.throws(RangeError, function() {
    new Intl.Locale("en", {numberingSystem: invalidNumberingSystemOption});
  }, `${invalidNumberingSystemOption} is an invalid numberingSystem option value`);
}

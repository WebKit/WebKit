// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
features: [Intl.NumberFormat-unified]
---*/

const options = new Intl.PluralRules([], {
  "minimumSignificantDigits": 1,
  "maximumSignificantDigits": 2,
}).resolvedOptions();

const expected = [
  "locale",
  "type",
  "minimumIntegerDigits",
  "minimumSignificantDigits",
  "maximumSignificantDigits",
  "pluralCategories",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);

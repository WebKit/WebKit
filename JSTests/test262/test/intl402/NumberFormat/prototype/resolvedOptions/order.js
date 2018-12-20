// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [compareArray.js]
---*/

const options = new Intl.NumberFormat([], {
  "style": "currency",
  "currency": "EUR",
  "currencyDisplay": "symbol",
  "minimumSignificantDigits": 1,
  "maximumSignificantDigits": 2,
}).resolvedOptions();

const expected = [
  "locale",
  "numberingSystem",
  "style",
  "currency",
  "currencyDisplay",
  "minimumIntegerDigits",
  "minimumFractionDigits",
  "maximumFractionDigits",
  "minimumSignificantDigits",
  "maximumSignificantDigits",
  "useGrouping",
];

assert.compareArray(Object.getOwnPropertyNames(options), expected);

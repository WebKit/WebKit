// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat
description: Checks handling of invalid value for the type option to the ListFormat constructor.
info: |
    InitializeListFormat (listFormat, locales, options)
    7. Let type be GetOption(options, "type", "string", « "conjunction", "disjunction", "unit" », "conjunction").
features: [Intl.ListFormat]
---*/

const invalidTypes = [
  "conjunction",
  "disjunction",
];

for (const type of invalidTypes) {
  assert.throws(RangeError, function() {
    new Intl.ListFormat([], { style: "narrow", type });
  }, `${type} is an invalid type option value when style is narrow.`);
}

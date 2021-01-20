// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Checks error cases for the options argument to the DateTimeFormat constructor.
info: |
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )
    23. Let _opt_.[[FractionalSecondDigits]] be ? GetNumberOption(_options_, `"fractionalSecondDigits"`, 0, 3, 0).

    ...
features: [Intl.DateTimeFormat-fractionalSecondDigits]
---*/


const invalidOptions = [
  "LONG",
  " long",
  "short ",
  "full",
  "numeric",
  -1,
  4,
  "4",
  "-1",
  -0.00001,
  3.000001,
];
for (const fractionalSecondDigits of invalidOptions) {
  assert.throws(RangeError, function() {
    new Intl.DateTimeFormat("en", { fractionalSecondDigits });
  },
  `new Intl.DateTimeFormat("en", { fractionalSecondDigits: "${fractionalSecondDigits}" }) throws RangeError`);
}

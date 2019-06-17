// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Checks error cases for the options argument to the DateTimeFormat constructor.
info: |
   [[Quarter]]    `"quarter"`    `"narrow"`, `"short"`, `"long"`
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )

    ...
features: [Intl.DateTimeFormat-quarter]
---*/


const invalidOptions = [
  "",
  "LONG",
  " long",
  "short ",
  "full",
  "numeric",
];
for (const quarter of invalidOptions) {
  assert.throws(RangeError, function() {
    new Intl.DateTimeFormat("en", { quarter });
  }, `new Intl.DateTimeFormat("en", { quarter: "${quarter}" }) throws RangeError`);
}

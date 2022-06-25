// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Checks error cases for the options argument to the DateTimeFormat constructor.
info: |
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )

    ...
    30. Let timeStyle be ? GetOption(options, "timeStyle", "string", « "full", "long", "medium", "short" », undefined).
features: [Intl.DateTimeFormat-datetimestyle]
---*/


const invalidOptions = [
  "",
  "FULL",
  " long",
  "short ",
  "narrow",
  "numeric",
];
for (const timeStyle of invalidOptions) {
  assert.throws(RangeError, function() {
    new Intl.DateTimeFormat("en", { timeStyle });
  }, `new Intl.DateTimeFormat("en", { timeStyle: "${timeStyle}" }) throws RangeError`);
}

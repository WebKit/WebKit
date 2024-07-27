// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.constructor
description: An ISO string is not valid input for a constructor's calendar param
features: [Temporal]
---*/

assert.throws(
  RangeError,
  () => new Temporal.PlainMonthDay(12, 15, "1997-12-04[u-ca=iso8601]", 1972),
  "An ISO string is not a valid calendar ID for constructor parameter"
);

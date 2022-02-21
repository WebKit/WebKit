// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: from() throws if the argument is not a built-in calendar name.
features: [Temporal]
---*/

const tests = [
  "local",
  "iso-8601",
  "[u-ca=iso8601]",
  "invalid-calendar",
];

for (const item of tests) {
  assert.throws(RangeError, () => Temporal.Calendar.from(item));
}

// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Rejects non-numeric nanosecond values reported by TimeZone-like object
features: [Temporal]
---*/

const invalidValues = [
  undefined,
  null,
  true,
  "2020-01-01T12:45:36",
  Symbol(),
  2n,
  {},
  Temporal.PlainDateTime,
  Temporal.PlainDateTime.prototype,
];

for (const dateTime of invalidValues) {
  let callCount = 0;
  const timeZone = {
    getOffsetNanosecondsFor(instant, calendar) {
      callCount += 1;
      return dateTime;
    },
  };

  assert.throws(TypeError, () => Temporal.Now.plainDateTime("iso8601", timeZone));
  assert.sameValue(callCount, 1, 'Invoked `getOffsetNanosecondsFor`');
}

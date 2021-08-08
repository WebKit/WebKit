// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Return value describes the start of a day
features: [Temporal]
---*/

const calendar = Temporal.Calendar.from("iso8601");
const timeZone = {
  getOffsetNanosecondsFor(instant) {
    return -Number(instant.epochNanoseconds % 86400_000_000_000n);
  }
};

const result = Temporal.Now.plainDateTime(calendar, timeZone);
for (const property of ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]) {
  assert.sameValue(result[property], 0, property);
}

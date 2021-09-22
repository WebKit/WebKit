// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetimeiso
description: Return value describes the start of a day
features: [BigInt, Temporal]
---*/
const timeZone = {
  getOffsetNanosecondsFor(instant) {
    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
};

const result = Temporal.Now.plainDateTimeISO(timeZone);

for (const property of ['hour', 'minute', 'second', 'millisecond', 'microsecond', 'nanosecond']) {
  assert.sameValue(result[property], 0, 'The value of result[property] is expected to be 0');
}

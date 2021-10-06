// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: dateAdd() is called with the correct three arguments
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

let actual = [];
class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateAdd(...args) {
    actual.push(this, ...args);
    return super.dateAdd(...args);
  }
}

const calendar = new Calendar();
const zdt = new Temporal.ZonedDateTime(0n, "UTC", calendar);
const result = zdt.round({ smallestUnit: "day" });
assert.sameValue(result.epochNanoseconds, 0n, "Result");

assert.sameValue(actual.length, 4, "three arguments");
assert.sameValue(actual[0], calendar, "this value");
TemporalHelpers.assertPlainDate(actual[1], 1970, 1, "M01", 1, "date argument");
TemporalHelpers.assertDuration(actual[2], 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "duration argument");
assert.sameValue(actual[3], undefined, "options should be undefined");

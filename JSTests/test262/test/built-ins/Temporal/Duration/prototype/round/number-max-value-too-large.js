// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  RoundDuration throws a RangeError when the result duration is invalid.
features: [Temporal]
---*/

function test(unit, nextSmallestUnit) {
  var duration = Temporal.Duration.from({
    [unit]: Number.MAX_VALUE,
    [nextSmallestUnit]: Number.MAX_VALUE,
  });

  var options = {smallestUnit: unit, largestUnit: unit};

  assert.throws(RangeError, () => duration.round(options));
}

test("days", "hours");
test("hours", "minutes");
test("minutes", "seconds");
test("seconds", "milliseconds");
test("milliseconds", "microseconds");
test("microseconds", "nanoseconds");

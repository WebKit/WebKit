// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Hebrew leap months
features: [Temporal]
---*/

// Valid leap month: Adar I 5779
var date = Temporal.PlainDate.from({
  year: 5779,
  month: 6,
  day: 1,
  calendar: "hebrew"
});
assert.sameValue(date.month, 6);
assert.sameValue(date.monthCode, "M05L");
assert.sameValue(date.day, 1);
date = Temporal.PlainDate.from({
  year: 5779,
  monthCode: "M05L",
  day: 1,
  calendar: "hebrew"
});
assert.sameValue(date.month, 6);
assert.sameValue(date.monthCode, "M05L");
assert.sameValue(date.day, 1);

// Creating dates in later months in a leap year
var date = Temporal.PlainDate.from({
  year: 5779,
  month: 7,
  day: 1,
  calendar: "hebrew"
});
assert.sameValue(date.month, 7);
assert.sameValue(date.monthCode, "M06");
assert.sameValue(date.day, 1);
date = Temporal.PlainDate.from({
  year: 5779,
  monthCode: "M06",
  day: 1,
  calendar: "hebrew"
});
assert.sameValue(date.month, 7);
assert.sameValue(date.monthCode, "M06");
assert.sameValue(date.day, 1);

// Invalid leap months: e.g. M02L
for (var i = 1; i <= 12; i++) {
  if (i === 5)
    continue;
  assert.throws(RangeError, () => Temporal.PlainDate.from({
    year: 5779,
    monthCode: `M${ i.toString().padStart(2, "0") }L`,
    day: 1,
    calendar: "hebrew"
  }));
}

// Leap month in non-leap year (reject): Adar I 5780
assert.throws(RangeError, () => Temporal.PlainDate.from({
  year: 5780,
  monthCode: "M05L",
  day: 1,
  calendar: "hebrew"
}, { overflow: "reject" }));

// Leap month in non-leap year (constrain): 15 Adar I 5780 => 30 Av 5780
var date = Temporal.PlainDate.from({
  year: 5780,
  monthCode: "M05L",
  day: 15,
  calendar: "hebrew"
});
assert.sameValue(date.month, 5);
assert.sameValue(date.monthCode, "M05");
assert.sameValue(date.day, 30);

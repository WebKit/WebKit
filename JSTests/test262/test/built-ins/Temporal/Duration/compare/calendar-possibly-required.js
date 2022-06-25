// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: relativeTo argument needed if days = 0 but years/months/weeks non-zero
features: [Temporal]
---*/
const duration1 = new Temporal.Duration(1);
const duration2 = new Temporal.Duration(0, 12);
const duration3 = new Temporal.Duration(0, 0, 5);
const duration4 = new Temporal.Duration(0, 0, 0, 42);
const relativeTo = new Temporal.PlainDate(2021, 12, 15);
assert.throws(
  RangeError,
  () => { Temporal.Duration.compare(duration1, duration1); },
  "cannot compare Duration values without relativeTo if year is non-zero"
);
assert.sameValue(0,
  Temporal.Duration.compare(duration1, duration1, { relativeTo }),
  "compare succeeds for year-only Duration provided relativeTo is supplied");
assert.throws(
  RangeError,
  () => { Temporal.Duration.compare(duration2, duration2); },
  "cannot compare Duration values without relativeTo if month is non-zero"
);
assert.sameValue(0,
  Temporal.Duration.compare(duration2, duration2, { relativeTo }),
  "compare succeeds for year-and-month Duration provided relativeTo is supplied");
assert.throws(
  RangeError,
  () => { Temporal.Duration.compare(duration3, duration3); },
  "cannot compare Duration values without relativeTo if week is non-zero"
);
assert.sameValue(0,
  Temporal.Duration.compare(duration3, duration3, { relativeTo }),
  "compare succeeds for year-and-month-and-week Duration provided relativeTo is supplied"
);

assert.sameValue(0,
  Temporal.Duration.compare(duration4, duration4),
  "compare succeeds for zero year-month-week non-zero day Duration even without relativeTo");

// Double-check that the comparison also works with a relative-to argument
assert.sameValue(0,
  Temporal.Duration.compare(duration4, duration4, { relativeTo }),
  "compare succeeds for zero year-month-week non-zero day Duration with relativeTo"
);

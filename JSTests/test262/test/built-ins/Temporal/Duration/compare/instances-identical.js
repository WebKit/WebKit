// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: >
  Shortcut return with no observable user code calls when two Temporal.Duration
  have identical internal slots
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const duration1 = new Temporal.Duration(0, 0, 0, 5, 5, 5, 5, 5, 5, 5);
const duration2 = new Temporal.Duration(0, 0, 0, 5, 5, 5, 5, 5, 5, 5);
assert.sameValue(Temporal.Duration.compare(duration1, duration2), 0, "identical Duration instances should be equal");

const dateDuration1 = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
const dateDuration2 = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
assert.sameValue(
  Temporal.Duration.compare(dateDuration1, dateDuration2),
  0,
  "relativeTo is not required if two distinct Duration instances are identical"
);

const calendar = TemporalHelpers.calendarThrowEverything();
const relativeTo = new Temporal.PlainDate(2000, 1, 1, calendar);
assert.sameValue(
  Temporal.Duration.compare(dateDuration1, dateDuration2, { relativeTo }),
  0,
  "no calendar methods are called if two distinct Duration instances are identical"
);

const dateDuration3 = new Temporal.Duration(5, 5, 5, 5, 4, 65, 5, 5, 5, 5);
assert.throws(
  RangeError,
  () => Temporal.Duration.compare(dateDuration1, dateDuration3),
  "relativeTo is required if two Duration instances are the same length but not identical"
);

assert.throws(
  Test262Error,
  () => Temporal.Duration.compare(dateDuration1, dateDuration3, { relativeTo }),
  "calendar methods are called if two Duration instances are the same length but not identical"
);

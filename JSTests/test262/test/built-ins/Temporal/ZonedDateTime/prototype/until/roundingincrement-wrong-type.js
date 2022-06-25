// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Type conversions for roundingIncrement option
info: |
    sec-getoption step 8.a:
      a. Set _value_ to ? ToNumber(value).
    sec-temporal-totemporalroundingincrement step 5:
      5. Let _increment_ be ? GetOption(_normalizedOptions_, *"roundingIncrement"*, « Number », *undefined*, 1).
    sec-temporal.zoneddatetime.prototype.until step 12:
      12. Let _roundingIncrement_ be ? ToTemporalRoundingIncrement(_options_, _maximum_, *false*).
includes: [temporalHelpers.js, compareArray.js]
features: [Temporal]
---*/

const earlier = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC");
const later = new Temporal.ZonedDateTime(1_000_090_061_988_655_322n, "UTC");

TemporalHelpers.checkRoundingIncrementOptionWrongType(
  (roundingIncrement) => earlier.until(later, { roundingIncrement }),
  (result, descr) => TemporalHelpers.assertDuration(result, 0, 0, 0, 0, 25, 1, 1, 1, 1, 1, descr),
  (result, descr) => TemporalHelpers.assertDuration(result, 0, 0, 0, 0, 25, 1, 1, 1, 1, 0, descr),
);

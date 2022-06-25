// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: An ISO 8601 date string should be converted as input
info: |
  4. If Type(temporalDateLike) is not Object or temporalDateLike does not have an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]] internal slot, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  5. Return ! IsISOLeapYear(temporalDateLike.[[ISOYear]]).
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

assert.sameValue(cal.inLeapYear("2019-03-18"), false);
assert.sameValue(cal.inLeapYear("2020-03-18"), true);

assert.sameValue(cal.inLeapYear("+002023-03-18"), false);
assert.sameValue(cal.inLeapYear("+002024-03-18"), true);

assert.sameValue(cal.inLeapYear("2019-03-18T13:00:00+00:00[UTC]"), false);
assert.sameValue(cal.inLeapYear("2020-12-31T23:59:59+00:00[UTC]"), true);

assert.sameValue(cal.inLeapYear("+002023-03-18T13:00:00+00:00[UTC]"), false);
assert.sameValue(cal.inLeapYear("+002024-03-18T13:00:00+00:00[UTC]"), true);

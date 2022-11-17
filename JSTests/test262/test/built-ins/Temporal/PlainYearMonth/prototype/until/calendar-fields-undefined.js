// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: >
  When calendar.fields is undefined, until() doesn't perform an
  observable array iteration
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
calendar.fields = undefined;

const instance = new Temporal.PlainYearMonth(1981, 12, calendar);

// Detect observable array iteration:
const oldIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () {
  throw new Test262Error(`array shouldn't be iterated: ${new Error().stack}`);
};

instance.until(new Temporal.PlainYearMonth(2022, 10, calendar));
// Check that no iteration is performed when converting a property bag either
instance.since({ year: 2022, month: 10, calendar });

Array.prototype[Symbol.iterator] = oldIterator;

// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: >
  When calendar.fields is undefined, since() doesn't perform an
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

instance.since(new Temporal.PlainYearMonth(1979, 4, calendar));
// Check that no iteration is performed when converting a property bag either
instance.since({ year: 1979, month: 4, calendar });

Array.prototype[Symbol.iterator] = oldIterator;

// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.from
description: >
  When calendar.fields is undefined, from() doesn't perform an
  observable array iteration to convert the property bag to PlainDate
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
calendar.fields = undefined;

// Detect observable array iteration:
const oldIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () {
  throw new Test262Error(`array shouldn't be iterated: ${new Error().stack}`);
};

const arg = { year: 1981, month: 12, day: 15, calendar };

Temporal.PlainDate.from(arg);

Array.prototype[Symbol.iterator] = oldIterator;

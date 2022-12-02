// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: >
  When calendar.fields is undefined, equals() doesn't perform an
  observable array iteration to convert the property bag to PlainMonthDay
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
calendar.fields = undefined;

const instance = new Temporal.PlainMonthDay(5, 2);

// Detect observable array iteration:
const oldIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () {
  throw new Test262Error(`array shouldn't be iterated: ${new Error().stack}`);
};

const arg = { monthCode: "M12", day: 15, calendar };

instance.equals(arg);

Array.prototype[Symbol.iterator] = oldIterator;

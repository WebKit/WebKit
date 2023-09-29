// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: >
  Calling the method with a property bag argument with a builtin calendar causes
  no observable array iteration when getting the calendar fields.
features: [Temporal]
---*/

const arrayPrototypeSymbolIteratorOriginal = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function arrayIterator() {
  throw new Test262Error("Array should not be iterated");
}

const instance = new Temporal.PlainYearMonth(2000, 5);
const arg = { year: 2000, month: 5, calendar: "iso8601" };
instance.until(arg);

Array.prototype[Symbol.iterator] = arrayPrototypeSymbolIteratorOriginal;

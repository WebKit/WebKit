// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplainmonthday
description: >
  Calling the method on an instance constructed with a builtin calendar causes
  no observable array iteration when getting the calendar fields.
features: [Temporal]
---*/

const arrayPrototypeSymbolIteratorOriginal = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function arrayIterator() {
  throw new Test262Error("Array should not be iterated");
}

const instance = new Temporal.PlainDate(2023, 5, 2, "iso8601");
instance.toPlainMonthDay();

Array.prototype[Symbol.iterator] = arrayPrototypeSymbolIteratorOriginal;

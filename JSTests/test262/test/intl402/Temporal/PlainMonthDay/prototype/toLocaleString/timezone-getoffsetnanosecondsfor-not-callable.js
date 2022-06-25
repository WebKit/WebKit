// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tolocalestring
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

const instance = new Temporal.PlainMonthDay(5, 2);
Temporal.TimeZone.prototype.getPossibleInstantsFor = function () {
  return [];
};

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach((notCallable) => {
  Temporal.TimeZone.prototype.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => instance.toLocaleString(undefined, { calendar: "iso8601" }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

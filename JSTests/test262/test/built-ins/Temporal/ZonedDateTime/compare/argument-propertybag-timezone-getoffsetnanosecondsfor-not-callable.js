// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

Temporal.TimeZone.prototype.getPossibleInstantsFor = function () {
  return [];
};

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach((notCallable) => {
  const timeZone = new Temporal.TimeZone("UTC");
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  timeZone.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, hour: 12, timeZone }, datetime),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.compare(datetime, { year: 2000, month: 5, day: 2, hour: 12, timeZone }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

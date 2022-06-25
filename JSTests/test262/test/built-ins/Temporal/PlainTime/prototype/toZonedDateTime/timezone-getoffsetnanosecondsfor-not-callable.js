// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach((notCallable) => {
  const timeZone = new Temporal.TimeZone("UTC");
  const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
  const plainDate = new Temporal.PlainDate(2000, 5, 2);
  timeZone.getPossibleInstantsFor = function () {
    return [];
  };
  timeZone.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => time.toZonedDateTime({ plainDate, timeZone }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

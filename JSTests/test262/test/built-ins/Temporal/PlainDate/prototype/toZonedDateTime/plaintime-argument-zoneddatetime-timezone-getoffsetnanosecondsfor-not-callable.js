// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach((notCallable) => {
  const timeZone = new Temporal.TimeZone("UTC");
  const date = new Temporal.PlainDate(2000, 5, 2);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  timeZone.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => date.toZonedDateTime({ plainTime: datetime, timeZone: "UTC" }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

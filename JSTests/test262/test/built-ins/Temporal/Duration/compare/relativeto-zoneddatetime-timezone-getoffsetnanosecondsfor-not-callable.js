// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.compare
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach(notCallable => {
  const timeZone = new Temporal.TimeZone("UTC");
  const relativeTo = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  const duration1 = new Temporal.Duration(1);
  const duration2 = new Temporal.Duration(1, 1);
  timeZone.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => Temporal.Duration.compare(duration1, duration2, { relativeTo }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

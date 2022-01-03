// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.from
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach(notCallable => {
  const timeZone = new Temporal.TimeZone("UTC");
  timeZone.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, hour: 12, offset: "+00:00", timeZone }, { offset: "prefer" }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError (in offset=prefer and no disambiguation case)`
  );

  const badTimeZone = {
    getPossibleInstantsFor() { return []; },
    getOffsetNanosecondsFor: notCallable,
  };
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, hour: 12, offset: "+00:00", timeZone: badTimeZone }, { offset: "ignore" }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError (in offset=ignore and no possible instants case)`
  );
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, hour: 12, offset: "+00:00", timeZone: badTimeZone }, { offset: "prefer" }),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError (in offset=prefer and no possible instants case)`
  );
});

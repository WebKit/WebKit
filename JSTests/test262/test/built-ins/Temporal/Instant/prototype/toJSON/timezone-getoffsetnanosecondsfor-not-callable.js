// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tojson
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [BigInt, Symbol, Temporal, arrow-function]
---*/

const instance = new Temporal.Instant(1_000_000_000_987_654_321n);

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach((notCallable) => {
  Temporal.TimeZone.prototype.getOffsetNanosecondsFor = notCallable;
  assert.throws(
    TypeError,
    () => instance.toJSON(),
    `Uncallable ${notCallable === null ? 'null' : typeof notCallable} getOffsetNanosecondsFor should throw TypeError`
  );
});

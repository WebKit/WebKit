// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: An exception from TimeZone#getOffsetNanosecondsFor() is propagated.
features: [Temporal]
---*/

class TZ extends Temporal.TimeZone {
  constructor() { super("UTC") }
  getOffsetNanosecondsFor() { throw new Test262Error() }
}

const tz = new TZ();
const arg = new Temporal.ZonedDateTime(0n, tz);
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);

assert.throws(Test262Error, () => instance.withPlainDate(arg));

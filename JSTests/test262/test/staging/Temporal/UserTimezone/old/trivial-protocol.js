// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Trivial protocol implementation
features: [Temporal]
---*/

var obj = {
  getOffsetNanosecondsFor() {
    return 0;
  },
  getPossibleInstantsFor(dateTime) {
    var {year, month, day, hour, minute, second, millisecond, microsecond, nanosecond} = dateTime;
    var dayNum = MakeDay(year, month, day);
    var time = MakeTime(hour, minute, second, millisecond, microsecond, nanosecond);
    var epochNs = MakeDate(dayNum, time);
    return [new Temporal.Instant(epochNs)];
  },
  toString() {
    return "Etc/Custom/UTC_Protocol";
  }
};
var inst = Temporal.Instant.fromEpochNanoseconds(0n);

// offset prints in instant.toString
assert.sameValue(inst.toString({ timeZone: obj }), "1970-01-01T00:00:00+00:00")

// prints in zdt.toString
var zdt = new Temporal.ZonedDateTime(0n, obj);
assert.sameValue(zdt.toString(), "1970-01-01T00:00:00+00:00[Etc/Custom/UTC_Protocol]");

// works in Temporal.Now
assert(Temporal.Now.plainDateTimeISO(obj) instanceof Temporal.PlainDateTime);
assert(Temporal.Now.plainDateTime("gregory", obj) instanceof Temporal.PlainDateTime);
assert(Temporal.Now.plainDateISO(obj) instanceof Temporal.PlainDate);
assert(Temporal.Now.plainDate("gregory", obj) instanceof Temporal.PlainDate);
assert(Temporal.Now.plainTimeISO(obj) instanceof Temporal.PlainTime);

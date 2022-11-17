// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: sub-minute offset
features: [Temporal]
---*/

class SubminuteTimeZone extends Temporal.TimeZone {
  constructor() {
    super("-00:00:01.111111111");
  }
  toString() {
    return "Custom/Subminute";
  }
  getOffsetNanosecondsFor() {
    return -1111111111;
  }
  getPossibleInstantsFor(dateTime) {
    var utc = Temporal.TimeZone.from("UTC");
    var instant = utc.getInstantFor(dateTime);
    return [instant.add({ nanoseconds: 1111111111 })];
  }
  getNextTransition() {
    return null;
  }
  getPreviousTransition() {
    return null;
  }
}
var obj = new SubminuteTimeZone();
var inst = Temporal.Instant.fromEpochNanoseconds(0n);
var dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);

// is a time zone
assert.sameValue(typeof obj, "object")

// .id property
assert.sameValue(obj.id, "Custom/Subminute")

// .id is not available in from()
assert.throws(RangeError, () => Temporal.TimeZone.from("Custom/Subminute"));
assert.throws(RangeError, () => Temporal.TimeZone.from("2020-05-26T16:02:46.251163036-00:00:01.111111111[Custom/Subminute]"));

// has offset string -00:00:01.111111111
assert.sameValue(obj.getOffsetStringFor(inst), "-00:00:01.111111111")

// converts to DateTime
var fakeGregorian = { toString() { return "gregory"; }};
assert.sameValue(`${ obj.getPlainDateTimeFor(inst) }`, "1969-12-31T23:59:58.888888889");
assert.sameValue(`${ obj.getPlainDateTimeFor(inst, fakeGregorian) }`, "1969-12-31T23:59:58.888888889[u-ca=gregory]");

// converts to Instant
assert.sameValue(`${ obj.getInstantFor(dt) }`, "1976-11-18T15:23:31.2345679Z");

// converts to string
assert.sameValue(`${ obj }`, obj.id)

// offset prints with minute precision in instant.toString
assert.sameValue(inst.toString({ timeZone: obj }), "1969-12-31T23:59:58.888888889+00:00")

// offset prints with minute precision prints in zdt.toString
var zdt = new Temporal.ZonedDateTime(0n, obj);
assert.sameValue(zdt.toString(), "1969-12-31T23:59:58.888888889+00:00[Custom/Subminute]");

// has no next transitions
assert.sameValue(obj.getNextTransition(), null)

// has no previous transitions
assert.sameValue(obj.getPreviousTransition(), null)

// works in Temporal.Now
assert(Temporal.Now.plainDateTimeISO(obj) instanceof Temporal.PlainDateTime);
assert(Temporal.Now.plainDateTime(fakeGregorian, obj) instanceof Temporal.PlainDateTime);
assert(Temporal.Now.plainDateISO(obj) instanceof Temporal.PlainDate);
assert(Temporal.Now.plainDate(fakeGregorian, obj) instanceof Temporal.PlainDate);
assert(Temporal.Now.plainTimeISO(obj) instanceof Temporal.PlainTime);

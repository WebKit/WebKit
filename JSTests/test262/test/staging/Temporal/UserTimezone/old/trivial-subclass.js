// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Trivial subclass
features: [Temporal]
---*/

class CustomUTCSubclass extends Temporal.TimeZone {
  constructor() {
    super("UTC");
  }
  toString() {
    return "Etc/Custom/UTC_Subclass";
  }
  getOffsetNanosecondsFor() {
    return 0;
  }
  getPossibleInstantsFor(dateTime) {
    var {year, month, day, hour, minute, second, millisecond, microsecond, nanosecond} = dateTime;
    var dayNum = MakeDay(year, month, day);
    var time = MakeTime(hour, minute, second, millisecond, microsecond, nanosecond);
    var epochNs = MakeDate(dayNum, time);
    return [new Temporal.Instant(epochNs)];
  }
  getNextTransition() {
    return null;
  }
  getPreviousTransition() {
    return null;
  }
}

const nsPerDay = 86400_000_000_000n;
const nsPerMillisecond = 1_000_000n;

function Day(t) {
  return t / nsPerDay;
}

function MakeDate(day, time) {
  return day * nsPerDay + time;
}

function MakeDay(year, month, day) {
  const m = month - 1;
  const ym = year + Math.floor(m / 12);
  const mn = m % 12;
  const t = BigInt(Date.UTC(ym, mn, 1)) * nsPerMillisecond;
  return Day(t) + BigInt(day) - 1n;
}

function MakeTime(h, min, s, ms, µs, ns) {
  const MinutesPerHour = 60n;
  const SecondsPerMinute = 60n;
  const nsPerSecond = 1_000_000_000n;
  const nsPerMinute = nsPerSecond * SecondsPerMinute;
  const nsPerHour = nsPerMinute * MinutesPerHour;
  return (
    BigInt(h) * nsPerHour +
    BigInt(min) * nsPerMinute +
    BigInt(s) * nsPerSecond +
    BigInt(ms) * nsPerMillisecond +
    BigInt(µs) * 1000n +
    BigInt(ns)
  );
}

var obj = new CustomUTCSubclass();
var inst = Temporal.Instant.fromEpochNanoseconds(0n);
var dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);

// is a time zone
assert.sameValue(typeof obj, "object")

// .id property
assert.sameValue(obj.id, "Etc/Custom/UTC_Subclass")

// .id is not available in from()
assert.throws(RangeError, () => Temporal.TimeZone.from("Etc/Custom/UTC_Subclass"));
assert.throws(RangeError, () => Temporal.TimeZone.from("2020-05-26T16:02:46.251163036+00:00[Etc/Custom/UTC_Subclass]"));

// has offset string +00:00
assert.sameValue(obj.getOffsetStringFor(inst), "+00:00")

// converts to DateTime
var fakeGregorian = { toString() { return "gregory"; }};
assert.sameValue(`${ obj.getPlainDateTimeFor(inst) }`, "1970-01-01T00:00:00");
assert.sameValue(`${ obj.getPlainDateTimeFor(inst, fakeGregorian) }`, "1970-01-01T00:00:00[u-ca=gregory]");

// converts to Instant
assert.sameValue(`${ obj.getInstantFor(dt) }`, "1976-11-18T15:23:30.123456789Z");

// converts to string
assert.sameValue(`${ obj }`, obj.id)

// offset prints in instant.toString
assert.sameValue(inst.toString({ timeZone: obj }), "1970-01-01T00:00:00+00:00")

// prints in zdt.toString
var zdt = new Temporal.ZonedDateTime(0n, obj);
assert.sameValue(zdt.toString(), "1970-01-01T00:00:00+00:00[Etc/Custom/UTC_Subclass]");

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

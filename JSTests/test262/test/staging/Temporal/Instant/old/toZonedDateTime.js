// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant.toZonedDateTime() works
features: [Temporal]
---*/

var inst = Temporal.Instant.from("1976-11-18T14:23:30.123456789Z");

// throws without parameter
assert.throws(TypeError, () => inst.toZonedDateTime());

// throws with a string parameter
assert.throws(TypeError, () => inst.toZonedDateTime("Asia/Singapore"));

// time zone parameter UTC
var timeZone = Temporal.TimeZone.from("UTC");
var zdt = inst.toZonedDateTime({
  timeZone,
  calendar: "gregory"
});
assert.sameValue(inst.epochNanoseconds, zdt.epochNanoseconds);
assert.sameValue(`${ zdt }`, "1976-11-18T14:23:30.123456789+00:00[UTC][u-ca=gregory]");

// time zone parameter non-UTC
var timeZone = Temporal.TimeZone.from("America/New_York");
var zdt = inst.toZonedDateTime({
  timeZone,
  calendar: "gregory"
});
assert.sameValue(inst.epochNanoseconds, zdt.epochNanoseconds);
assert.sameValue(`${ zdt }`, "1976-11-18T09:23:30.123456789-05:00[America/New_York][u-ca=gregory]");

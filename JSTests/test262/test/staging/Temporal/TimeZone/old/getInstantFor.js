// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone.prototype.getInstantFor() works
features: [Temporal]
---*/


// recent date
var dt = Temporal.PlainDateTime.from("2019-10-29T10:46:38.271986102");
var tz = Temporal.TimeZone.from("+01:00");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "2019-10-29T09:46:38.271986102Z");

// year ≤ 99
var dt = Temporal.PlainDateTime.from("0098-10-29T10:46:38.271986102");
var tz = Temporal.TimeZone.from("+06:00");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0098-10-29T04:46:38.271986102Z");
dt = Temporal.PlainDateTime.from("+000098-10-29T10:46:38.271986102");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0098-10-29T04:46:38.271986102Z");

// year < 1
var dt = Temporal.PlainDateTime.from("0000-10-29T10:46:38.271986102");
var tz = Temporal.TimeZone.from("+06:00");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0000-10-29T04:46:38.271986102Z");
dt = Temporal.PlainDateTime.from("+000000-10-29T10:46:38.271986102");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0000-10-29T04:46:38.271986102Z");
dt = Temporal.PlainDateTime.from("-001000-10-29T10:46:38.271986102");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "-001000-10-29T04:46:38.271986102Z");

// year 0 leap day
var dt = Temporal.PlainDateTime.from("0000-02-29T00:00");
var tz = Temporal.TimeZone.from("-00:01:15");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0000-02-29T00:01:15Z");
dt = Temporal.PlainDateTime.from("+000000-02-29T00:00");
assert.sameValue(`${ tz.getInstantFor(dt) }`, "0000-02-29T00:01:15Z");

// outside of Instant range
var max = Temporal.PlainDateTime.from("+275760-09-13T23:59:59.999999999");
var offsetTz = Temporal.TimeZone.from("-01:00");
assert.throws(RangeError, () => offsetTz.getInstantFor(max));
var namedTz = Temporal.TimeZone.from("Etc/GMT+12");
assert.throws(RangeError, () => namedTz.getInstantFor(max));

// casts argument
var tz = Temporal.TimeZone.from("+01:00");
assert.sameValue(`${ tz.getInstantFor("2019-10-29T10:46:38.271986102") }`, "2019-10-29T09:46:38.271986102Z");
assert.sameValue(`${ tz.getInstantFor({
  year: 2019,
  month: 10,
  day: 29,
  hour: 10,
  minute: 46,
  second: 38
}) }`, "2019-10-29T09:46:38Z");



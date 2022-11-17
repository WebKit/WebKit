// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: .withPlainTime manipulation
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("2015-12-07T03:24:30.000003500[-08:00]");

// withPlainTime({ hour: 10 }) works
assert.sameValue(`${ zdt.withPlainTime({ hour: 10 }) }`, "2015-12-07T10:00:00-08:00[-08:00]");

// withPlainTime(time) works
var time = Temporal.PlainTime.from("11:22");
assert.sameValue(`${ zdt.withPlainTime(time) }`, "2015-12-07T11:22:00-08:00[-08:00]");

// withPlainTime('12:34') works
assert.sameValue(`${ zdt.withPlainTime("12:34") }`, "2015-12-07T12:34:00-08:00[-08:00]");

// incorrectly-spelled properties are ignored
assert.sameValue(`${ zdt.withPlainTime({
  hour: 10,
  seconds: 55
}) }`, "2015-12-07T10:00:00-08:00[-08:00]");

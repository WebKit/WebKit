// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: math order of operations and options
features: [Temporal]
---*/

var breakoutUnits = (op, zdt, d, options) => zdt[op]({ years: d.years }, options)[op]({ months: d.months }, options)[op]({ weeks: d.weeks }, options)[op]({ days: d.days }, options)[op]({
  hours: d.hours,
  minutes: d.minutes,
  seconds: d.seconds,
  milliseconds: d.milliseconds,
  microseconds: d.microseconds,
  nanoseconds: d.nanoseconds
}, options);

// order of operations: add / none
var zdt = Temporal.ZonedDateTime.from("2020-01-31T00:00-08:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = undefined;
var result = zdt.add(d, options);
assert.sameValue(result.toString(), "2020-03-01T00:00:00-08:00[America/Los_Angeles]");
assert.sameValue(breakoutUnits("add", zdt, d, options).toString(), result.toString());

// order of operations: add / constrain
var zdt = Temporal.ZonedDateTime.from("2020-01-31T00:00-08:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = { overflow: "constrain" };
var result = zdt.add(d, options);
assert.sameValue(result.toString(), "2020-03-01T00:00:00-08:00[America/Los_Angeles]");
assert.sameValue(breakoutUnits("add", zdt, d, options).toString(), result.toString());

// order of operations: add / reject
var zdt = Temporal.ZonedDateTime.from("2020-01-31T00:00-08:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = { overflow: "reject" };
assert.throws(RangeError, () => zdt.add(d, options));

// order of operations: subtract / none
var zdt = Temporal.ZonedDateTime.from("2020-03-31T00:00-07:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = undefined;
var result = zdt.subtract(d, options);
assert.sameValue(result.toString(), "2020-02-28T00:00:00-08:00[America/Los_Angeles]");
assert.sameValue(breakoutUnits("subtract", zdt, d, options).toString(), result.toString());

// order of operations: subtract / constrain
var zdt = Temporal.ZonedDateTime.from("2020-03-31T00:00-07:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = { overflow: "constrain" };
var result = zdt.subtract(d, options);
assert.sameValue(result.toString(), "2020-02-28T00:00:00-08:00[America/Los_Angeles]");
assert.sameValue(breakoutUnits("subtract", zdt, d, options).toString(), result.toString());

// order of operations: subtract / reject
var zdt = Temporal.ZonedDateTime.from("2020-03-31T00:00-07:00[America/Los_Angeles]");
var d = Temporal.Duration.from({
  months: 1,
  days: 1
});
var options = { overflow: "reject" };
assert.throws(RangeError, () => zdt.subtract(d, options));

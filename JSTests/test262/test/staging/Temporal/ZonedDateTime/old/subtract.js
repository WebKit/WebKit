// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.subtract()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("1969-12-25T12:23:45.678901234+00:00[UTC]");

// inst.subtract(durationObj)
var earlier = zdt.subtract(Temporal.Duration.from("PT240H0.000000800S"));
assert.sameValue(`${ earlier }`, "1969-12-15T12:23:45.678900434+00:00[UTC]");

// casts argument
assert.sameValue(`${ zdt.subtract("PT240H0.000000800S") }`, "1969-12-15T12:23:45.678900434+00:00[UTC]");
var mar31 = Temporal.ZonedDateTime.from("2020-03-31T15:00+00:00[UTC]");

// constrain when ambiguous result
assert.sameValue(`${ mar31.subtract({ months: 1 }) }`, "2020-02-29T15:00:00+00:00[UTC]");
assert.sameValue(`${ mar31.subtract({ months: 1 }, { overflow: "constrain" }) }`, "2020-02-29T15:00:00+00:00[UTC]");

// symmetrical with regard to negative durations in the time part
assert.sameValue(`${ mar31.subtract({ minutes: -30 }) }`, "2020-03-31T15:30:00+00:00[UTC]");
assert.sameValue(`${ mar31.subtract({ seconds: -30 }) }`, "2020-03-31T15:00:30+00:00[UTC]");

// throw when ambiguous result with reject
assert.throws(RangeError, () => mar31.subtract({ months: 1 }, { overflow: "reject" }));

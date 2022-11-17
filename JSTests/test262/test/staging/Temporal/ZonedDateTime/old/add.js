// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.add()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("1969-12-25T12:23:45.678901234+00:00[UTC]");
// cross epoch in ms
  var one = zdt.subtract({
    hours: 240,
    nanoseconds: 800
  });
  var two = zdt.add({
    hours: 240,
    nanoseconds: 800
  });
  var three = two.subtract({
    hours: 480,
    nanoseconds: 1600
  });
  var four = one.add({
    hours: 480,
    nanoseconds: 1600
  });
assert.sameValue(`${ one }`, "1969-12-15T12:23:45.678900434+00:00[UTC]");
assert.sameValue(`${ two }`, "1970-01-04T12:23:45.678902034+00:00[UTC]");
assert(three.equals(one));
assert(four.equals(two));

// zdt.add(durationObj)
var later = zdt.add(Temporal.Duration.from("PT240H0.000000800S"));
assert.sameValue(`${ later }`, "1970-01-04T12:23:45.678902034+00:00[UTC]");

// casts argument
assert.sameValue(`${ zdt.add("PT240H0.000000800S") }`, "1970-01-04T12:23:45.678902034+00:00[UTC]");
var jan31 = Temporal.ZonedDateTime.from("2020-01-31T15:00-08:00[-08:00]");

// constrain when ambiguous result
assert.sameValue(`${ jan31.add({ months: 1 }) }`, "2020-02-29T15:00:00-08:00[-08:00]");
assert.sameValue(`${ jan31.add({ months: 1 }, { overflow: "constrain" }) }`, "2020-02-29T15:00:00-08:00[-08:00]");

// symmetrical with regard to negative durations in the time part
assert.sameValue(`${ jan31.add({ minutes: -30 }) }`, "2020-01-31T14:30:00-08:00[-08:00]");
assert.sameValue(`${ jan31.add({ seconds: -30 }) }`, "2020-01-31T14:59:30-08:00[-08:00]");

// throw when ambiguous result with reject
assert.throws(RangeError, () => jan31.add({ months: 1 }, { overflow: "reject" }));

// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.total()
features: [Temporal]
---*/

var oneDay = new Temporal.Duration(0, 0, 0, 1);
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);

// Samoa skipped 24 hours
var relativeTo = Temporal.PlainDateTime.from("2011-12-29T12:00").toZonedDateTime("Pacific/Apia");
var totalDays = hours25.total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - (2 + 1 / 24)) < Number.EPSILON);
assert.sameValue(Temporal.Duration.from({ hours: 48 }).total({
  unit: "days",
  relativeTo
}), 3);
assert.sameValue(Temporal.Duration.from({ days: 2 }).total({
  unit: "hours",
  relativeTo
}), 24);
assert.sameValue(Temporal.Duration.from({ days: 3 }).total({
  unit: "hours",
  relativeTo
}), 48);

// casts relativeTo to ZonedDateTime if possible
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: {
    year: 2000,
    month: 10,
    day: 29,
    timeZone: "America/Vancouver"
  }
}), 25);

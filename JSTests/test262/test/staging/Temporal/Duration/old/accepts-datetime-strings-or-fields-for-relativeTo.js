// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Accepts datetime strings or fields for relativeTo.
features: [Temporal]
---*/

const d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);

// accepts datetime strings or fields for relativeTo
[
  "2020-01-01",
  "20200101",
  "2020-01-01T00:00:00.000000000",
  {
    year: 2020,
    month: 1,
    day: 1
  }
].forEach(relativeTo => {
  const daysPastJuly1 = 5 * 7 + 5 - 30;
  const partialDayNanos = d.hours * 3600000000000 + d.minutes * 60000000000 + d.seconds * 1000000000 + d.milliseconds * 1000000 + d.microseconds * 1000 + d.nanoseconds;
  const partialDay = partialDayNanos / (3600000000000 * 24);
  const partialMonth = (daysPastJuly1 + partialDay) / 31;
  const totalMonths = 5 * 12 + 5 + 1 + partialMonth;
  const total = d.total({
    unit: "months",
    relativeTo
  });
  assert.sameValue(total.toPrecision(15), totalMonths.toPrecision(15));
});

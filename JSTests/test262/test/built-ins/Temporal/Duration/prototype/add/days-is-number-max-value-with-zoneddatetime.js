// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: >
  AddZonedDateTime throws a RangeError when the intermediate instant is too large.
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1970, 1, 1);
const zonedDateTime = new Temporal.ZonedDateTime(0n, "UTC", "iso8601");

var duration = Temporal.Duration.from({days: 1, nanoseconds: Number.MAX_VALUE});

var options = {relativeTo: zonedDateTime};

assert.throws(RangeError, () => duration.add(duration, options));

// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.total
description: >
    Balancing the resulting duration takes the time zone's UTC offset shifts
    into account
features: [Temporal]
---*/

// Based on a test case by Adam Shaw

const duration = new Temporal.Duration(1, 0, 0, 0, 24);
const relativeTo = new Temporal.ZonedDateTime(
    941184000_000_000_000n /* = 1999-10-29T08Z */,
    "America/Vancouver"); /* = 1999-10-29T00-08 in local time */

const result = duration.total({ unit: "days", relativeTo });
assert.sameValue(result, 366.96, "24 hours does not balance to 1 day in 25-hour day");

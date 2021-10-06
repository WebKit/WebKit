// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.tozoneddatetime
description: Basic tests for toZonedDateTime().
features: [Temporal]
---*/

const plainTime = Temporal.PlainTime.from('12:00');
const plainDate = Temporal.PlainDate.from('2020-07-08');
const timeZone = Temporal.TimeZone.from('America/Los_Angeles');

const objects = plainTime.toZonedDateTime({ timeZone, plainDate });
assert.sameValue(objects.epochNanoseconds, 1594234800000000000n, "objects: epochNanoseconds");
assert.sameValue(objects.timeZone, timeZone, "objects: timeZone");

const timeZoneString = plainTime.toZonedDateTime({ timeZone: "America/Los_Angeles", plainDate });
assert.sameValue(timeZoneString.epochNanoseconds, 1594234800000000000n, "timeZone string: epochNanoseconds");
assert.sameValue(timeZoneString.timeZone.id, "America/Los_Angeles", "timeZone string: timeZone");

const plainDateString = plainTime.toZonedDateTime({ timeZone, plainDate: "2020-07-08" });
assert.sameValue(plainDateString.epochNanoseconds, 1594234800000000000n, "plainDate string: epochNanoseconds");
assert.sameValue(plainDateString.timeZone.id, "America/Los_Angeles", "plainDate string: timeZone");

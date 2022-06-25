// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetimeiso
description: Functions when time zone argument is omitted
features: [Temporal]
---*/

const zdt = Temporal.Now.zonedDateTimeISO();
const tz = Temporal.Now.timeZone();
assert(zdt instanceof Temporal.ZonedDateTime);
assert(zdt.calendar instanceof Temporal.Calendar);
assert.sameValue(zdt.calendar.id, "iso8601");
assert(zdt.timeZone instanceof Temporal.TimeZone);
assert.sameValue(zdt.timeZone.id, tz.id);

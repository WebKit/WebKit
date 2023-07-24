// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetimeiso
description: Functions when time zone argument is omitted
features: [Temporal]
---*/

const zdt = Temporal.Now.zonedDateTimeISO();
const tz = Temporal.Now.timeZoneId();
assert(zdt instanceof Temporal.ZonedDateTime);
assert.sameValue(zdt.getISOFields().calendar, "iso8601", "calendar slot should store a string");
assert.sameValue(zdt.getISOFields().timeZone, tz, "time zone slot should store a string");

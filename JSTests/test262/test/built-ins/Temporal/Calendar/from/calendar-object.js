// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: Converting objects to Temporal.Calendar
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");
const calFromObject = Temporal.Calendar.from({ calendar: cal });
assert(calFromObject instanceof Temporal.Calendar);
assert.sameValue(calFromObject.id, "iso8601");

const calFromString = Temporal.Calendar.from({ calendar: "iso8601" });
assert(calFromString instanceof Temporal.Calendar);
assert.sameValue(calFromString.id, "iso8601");

const custom = { id: "custom-calendar" };
assert.sameValue(Temporal.Calendar.from({ calendar: custom }), custom);
assert.sameValue(Temporal.Calendar.from(custom), custom);

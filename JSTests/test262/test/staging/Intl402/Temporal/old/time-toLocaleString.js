// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: time.toLocaleString()
features: [Temporal]
---*/

var time = Temporal.PlainTime.from("1976-11-18T15:23:30");
assert.sameValue(`${ time.toLocaleString("en", { timeZone: "America/New_York" }) }`, "3:23:30 PM")
assert.sameValue(`${ time.toLocaleString("de", { timeZone: "Europe/Vienna" }) }`, "15:23:30")

// should ignore units not in the data type
assert.sameValue(time.toLocaleString("en", { timeZoneName: "long" }), "3:23:30 PM");
assert.sameValue(time.toLocaleString("en", { year: "numeric" }), "3:23:30 PM");
assert.sameValue(time.toLocaleString("en", { month: "numeric" }), "3:23:30 PM");
assert.sameValue(time.toLocaleString("en", { day: "numeric" }), "3:23:30 PM");
assert.sameValue(time.toLocaleString("en", { weekday: "long" }), "3:23:30 PM");

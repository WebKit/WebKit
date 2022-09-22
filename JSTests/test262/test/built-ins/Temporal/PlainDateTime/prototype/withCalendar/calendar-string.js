// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: A calendar ID is valid input for Calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, { id: "replace-me" });

const arg = "iso8601";

const result = instance.withCalendar(arg);
assert.sameValue(result.calendar.id, "iso8601", `Calendar created from string "${arg}"`);

// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(1976, 11, 18, { id: "replace-me" });

const arg = "iSo8601";;
const result = instance.withCalendar(arg);
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

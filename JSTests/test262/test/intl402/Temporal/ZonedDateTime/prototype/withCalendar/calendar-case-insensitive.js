// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withcalendar
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", { id: "replace-me" });

const arg = "jApAnEsE";;
const result = instance.withCalendar(arg);
assert.sameValue(result.calendar.id, "japanese", "Calendar is case-insensitive");

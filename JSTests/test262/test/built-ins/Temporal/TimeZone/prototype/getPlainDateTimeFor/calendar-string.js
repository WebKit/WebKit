// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: A calendar ID is valid input for Calendar
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = "iso8601";

const result = instance.getPlainDateTimeFor(new Temporal.Instant(0n), arg);
assert.sameValue(result.calendar.id, "iso8601", `Calendar created from string "${arg}"`);

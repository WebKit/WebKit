// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = "iSo8601";;
const result = instance.getPlainDateTimeFor(new Temporal.Instant(0n), arg);
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

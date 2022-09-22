// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const instance = new Temporal.Instant(1_000_000_000_000_000_000n);

const arg = "iSo8601";;
const result = instance.toZonedDateTime({ calendar: arg, timeZone: "UTC" });
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

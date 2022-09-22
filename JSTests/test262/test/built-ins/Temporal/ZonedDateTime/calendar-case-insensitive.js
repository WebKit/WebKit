// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const arg = "iSo8601";

const result = new Temporal.ZonedDateTime(0n, "UTC", arg);
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

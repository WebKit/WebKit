// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const arg = "iSo8601";

const result = new Temporal.PlainMonthDay(12, 15, arg, 1972);
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

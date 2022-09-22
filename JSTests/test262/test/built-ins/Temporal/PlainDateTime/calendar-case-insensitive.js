// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime
description: Calendar names are case-insensitive
features: [Temporal]
---*/

const arg = "iSo8601";

const result = new Temporal.PlainDateTime(2000, 5, 2, 15, 23, 30, 987, 654, 321, arg);
assert.sameValue(result.calendar.id, "iso8601", "Calendar is case-insensitive");

// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const result = new Temporal.PlainMonthDay(2, 11, "islamicc", 1972);
assert.sameValue(result.calendarId, "islamic-civil", "calendar ID is canonicalized");

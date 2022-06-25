// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainmonthday.prototype.monthcode
description: There is no 'month' property on Temporal.PlainMonthDay
features: [Temporal]
---*/

const md = new Temporal.PlainMonthDay(1, 15);
assert.sameValue(md.month, undefined);
assert.sameValue("month" in md, false);
